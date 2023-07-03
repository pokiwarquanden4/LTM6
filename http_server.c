#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define DEFAULT_PORT 8080
#define DEFAULT_ROOT "./"

void send_response(int client_socket, const char *message) {
    char response[BUFFER_SIZE];
    sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n%s", strlen(message), message);
    send(client_socket, response, strlen(response), 0);
}

void handle_directory(int client_socket, const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        send_response(client_socket, "404 Not Found");
        return;
    }

    struct dirent *dir_entry;
    char response[BUFFER_SIZE];
    sprintf(response, "<html><body><ul>");

    // Read directory contents
    while ((dir_entry = readdir(dir)) != NULL) {
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
            continue;
        }

        char entry_path[BUFFER_SIZE];
        sprintf(entry_path, "%s/%s", dir_path, dir_entry->d_name);

        struct stat entry_stat;
        if (stat(entry_path, &entry_stat) == 0) {
            if (S_ISDIR(entry_stat.st_mode)) {
                // Directory entry
                sprintf(response + strlen(response), "<li><b><a href=\"%s/\">%s/</a></b></li>", dir_entry->d_name, dir_entry->d_name);
            } else {
                // File entry
                sprintf(response + strlen(response), "<li><i><a href=\"%s\">%s</a></i></li>", dir_entry->d_name, dir_entry->d_name);
            }
        }
    }

    sprintf(response + strlen(response), "</ul></body></html>");
    send_response(client_socket, response);

    closedir(dir);
}

void handle_file(int client_socket, const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        send_response(client_socket, "404 Not Found");
        return;
    }

    // Read file contents
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *file_content = malloc(file_size + 1);
    fread(file_content, file_size, 1, file);
    file_content[file_size] = '\0';

    send_response(client_socket, file_content);

    free(file_content);
    fclose(file);
}

void handle_request(int client_socket, const char *request, const char *root) {
    char method[BUFFER_SIZE], path[BUFFER_SIZE], protocol[BUFFER_SIZE];
    sscanf(request, "%s %s %s", method, path, protocol);

    // Append root directory to the requested path
    char file_path[BUFFER_SIZE];
    sprintf(file_path, "%s%s", root, path);

    if (strcmp(method, "GET") == 0) {
        struct stat path_stat;
        if (stat(file_path, &path_stat) == 0) {
            if (S_ISDIR(path_stat.st_mode)) {
                handle_directory(client_socket, file_path);
            } else if (S_ISREG(path_stat.st_mode)) {
                handle_file(client_socket, file_path);
            } else {
                send_response(client_socket, "404 Not Found");
            }
        } else {
            send_response(client_socket, "404 Not Found");
        }
    } else {
        send_response(client_socket, "501 Not Implemented");
    }
}

int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_size = sizeof(client_address);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        return 1;
    }

    // Bind socket to port
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(DEFAULT_PORT);
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Failed to bind socket");
        return 1;
    }

    // Start listening for connections
    if (listen(server_socket, 5) < 0) {
        perror("Failed to listen for connections");
        return 1;
    }

    printf("Server started on port %d\n", DEFAULT_PORT);

    const char *root = DEFAULT_ROOT;
    if (argc >= 2) {
        root = argv[1];
    }

    while (1) {
        // Accept client connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (client_socket < 0) {
            perror("Failed to accept client connection");
            return 1;
        }

        char request[BUFFER_SIZE];
        memset(request, 0, BUFFER_SIZE);

        // Read client request
        recv(client_socket, request, BUFFER_SIZE - 1, 0);
        printf("Request:\n%s\n", request);

        // Handle client request
        handle_request(client_socket, request, root);

        // Close client connection
        close(client_socket); 
    }

    // Close server socket
    close(server_socket);

    return 0;
}