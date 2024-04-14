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

/**
 * Creates a socket suitable for serving clients,
 * and prepares it for accepting connections.
 *
 * @return server_fd or exit on failure of any stage of server initialization
 */
int serverInit() {
    struct addrinfo *head = NULL;
    struct addrinfo *current = NULL;
    struct addrinfo hints;
    int server_fd;
    int getaddr_status;
    int bindSuccess = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    getaddr_status = getaddrinfo(NULL, "8000", &hints, &head);
    if(getaddr_status != 0) {
        perror("getaddrinfo failure");
        exit(1);
    }
    //break on the first successful bind and socket
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
        exit(1);
    }
    return server_fd;
}

/**
 * Serves client requested files or notifies a client
 * that the requested files could not be found.
 *
 * @param client_fd - the client socket
 * @param path  - requested file
 */
void generateResponse(int client_fd, char *path) {
    int requested_file_fd = open(path, O_RDONLY);

    //if there is an error opening the file send the 404 not found error message
    if(requested_file_fd < 0) {
        char response[500] = {'\0'};

        snprintf(response, 500,
                 "HTTP/1.0 404 Not Found\r\n"
                        "Content-Length:\r\n"
                        "\r\n"
                        "The requested file could not be opened.\r\n");
        send(client_fd, response, strlen(response), 0);
    }
    else {
        struct stat file_info;
        stat(path, &file_info);
        char *header = malloc(100 * sizeof(char));
        //create the header for the 200 OK message
        snprintf(header, 100, "HTTP/1.0 200 OK\r\n"
                              "Content-Length: %zd\r\n"
                              "\r\n", file_info.st_size);
        //copy the header, and read the file into response
        char *response = malloc(((strlen(header) + file_info.st_size + 3) * (sizeof(char))));
        strncpy(response, header, strlen(header));
        if(read(requested_file_fd, response + strlen(header), file_info.st_size) < 0) {
            perror("Read failure");
        }
        response[strlen(header) + file_info.st_size] = '\r';
        response[strlen(header) + file_info.st_size + 1] = '\n';
        response[strlen(header) + file_info.st_size + 2] = '\0';
        send(client_fd, response, strlen(response), 0);
        free(response);
        free(header);
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

/**
 * Receives client request, validates syntax, and generates a response.
 *
 * @param client_fd - the client socket
 * @return NULL
 */
void *serveClient(void *client_fd) {
    char request[500] = {'\0'};
    char filePath[500] = {'\0'};
    ssize_t bytesReceived =  recv((*(int *)client_fd), request, 500, 0);
    //parse and respond to request
    if(bytesReceived > 0 ) {
        if(parseGet(request, filePath) == 0) {
            generateResponse(*(int *)client_fd, filePath);
        }
    }
    close(*(int *)client_fd);
    return NULL;
}

int main(int argc, char** argv) {
    int server_fd = serverInit();
    int chdir_status = chdir(argv[1]);
    struct sockaddr_storage client_address;
    socklen_t sin_size;

    if(chdir_status != 0) {
        perror("chdir() failure");
        exit(1);
    }
    //put this back in a loop //start doing tests in different branches silly
    while(1) {
        int client_fd;

        sin_size = sizeof(client_address);
        if((client_fd = accept(server_fd, (struct sockaddr *)&client_address, &sin_size)) == -1) {
            perror("'failed to accept client");
            continue;
        }
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, serveClient, &client_fd);
        pthread_join(client_thread, NULL);
    }
}
