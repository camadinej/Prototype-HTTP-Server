//
//Created by Jake Camadine
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <limits.h>

int serverInit() {
    //create a socket, bind, and listen:
    struct addrinfo *head, *current;
    struct addrinfo hints;
    int server_fd;
    int getaddr_status;
    int bindSuccess = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    getaddr_status = getaddrinfo(NULL, "8080", &hints, &head);
    if(getaddr_status != 0) {
        perror("getaddrinfo failure");
        exit(1);
    }
    for(current = head; current != NULL; current = current->ai_next) {
        if((server_fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol)) == -1) {
            continue;
        }
        if(bind(server_fd, current->ai_addr, current->ai_addrlen) == -1) {
            close(server_fd);
            continue;
        }
        bindSuccess = 1;
        break;
    }
    freeaddrinfo(head);
    if(bindSuccess != 1) {
        printf("failure creating and binding socket\n");
        exit(1);
    }
    if(listen(server_fd, 128) == -1) {
        perror("listen() failure");
    }
    return server_fd;
}

void generateResponse(int client_fd, char *path) {
    int requested_file_fd = open(path, O_RDONLY);

    if(requested_file_fd < 0) {
        char response[500];

        snprintf(response, 500,
                 "HTTP/1.0 404 Not Found\r\n"
                        "Content-Length:\r\n"
                        "\r\n"
                        "\r\n"
                        "The requested file could not be opened.");
        send(client_fd, response, strlen(response), 0);
    }
    else {
        struct stat file_info;
        stat(path, &file_info);
        char header[100];
        char fileBuffer[file_info.st_size + 1];

        if(read(requested_file_fd, fileBuffer, file_info.st_size) < 0) {
            perror("Read failure");
        }
        snprintf(header, 100, "HTTP/1.0 200 OK\r\n"
                     "Content-Length: %zd\r\n"
                     "\r\n"
                     "\r\n", file_info.st_size);
        char response[strlen(header) + file_info.st_size + 1];
        strncat(response, header, strlen(header));
        strncat(response, fileBuffer, file_info.st_size);
        send(client_fd, response, strlen(response), 0);
        close(requested_file_fd);
    }
}

/**
 * Validates the syntax of an HTTP request and populates buffer
 * with a path to the requested file.
 *
 * @param request - the client's request
 * @param path - the server's working directory
 * @param buffer - a place to store the formatted file path
 * @return 0 on successful parse. -1 otherwise.
 */
int parseGet(char *request, char *buffer) {
    char *stringToParse, *saveState, *token;

    stringToParse = request;
    token = strtok_r(stringToParse, " ", &saveState);
    if(strncmp("GET", token, 3) != 0) {
        return -1;
    }
    stringToParse = NULL;
    token = strtok_r(stringToParse, " ", &saveState);
    if(token[0] != '/') {
        return -1;
    }
    strncpy(buffer, token + 1, strlen(token) - 1);
    return 0;
}

void *serveRequest(void *client_fd) {
    //char request[PATH_MAX]
    char request[500];
    //char filePath[PATH_MAX]
    char filePath[500];
    //recv and check the return
    ssize_t bytesReceived =  recv((*(int *)client_fd), request, 500, 0);
    if(bytesReceived > 0 ) {
        //if parse get is successful call generateResponse
        if(parseGet(request, filePath) == 0) {
            generateResponse(*(int *)client_fd, filePath);
        }
    }
    //by this point we've served the request and are finished with this client
    close(*(int *)client_fd);
    return NULL;
}

int main(int argc, char** argv) {
    int server_fd = serverInit(); //we dont need to check this here because we do it all in the function
    int chdir_status = chdir(argv[1]);

    if(chdir_status != 0) {
        perror("chdir() failure");
        exit(1);
    }
    while(1) {
        //we can save this for last
        //accept a client
        //serve the client
        //pthread_join
    }
    return 0;
}
