#include "dados.h"

// ADD_USER <username> <password> <role>
void add_user(char *username, char *password, char *role, int fd, struct sockaddr_in client_address, socklen_t client_address_len,Shared_mem *shared_mem){
    char message_sent[BUFLEN];
    sprintf(message_sent, "Received request to add user %s with password %s and role %s\n", username, password, role);
    sendto(fd, message_sent, strlen(message_sent), 0, (struct sockaddr*) &client_address, client_address_len);
}

// DEL <username>
void remove_user(char *username, int fd, struct sockaddr_in client_address, socklen_t client_address_len,Shared_mem *shared_mem){
    char message_sent[BUFLEN];
    sprintf(message_sent, "Received request to remove user %s\n", username);
    sendto(fd, message_sent, strlen(message_sent) + 1, 0, (struct sockaddr*) &client_address, client_address_len);
}

// LIST
void list_users(int fd, struct sockaddr_in client_address, socklen_t client_address_len,Shared_mem *shared_mem){
    char message_sent[BUFLEN];
    sprintf(message_sent, "Received request to list users\n");
    sendto(fd, message_sent, strlen(message_sent) + 1, 0, (struct sockaddr*) &client_address, client_address_len);
}

// QUIT_SERVER
void shutdown_server(int fd, struct sockaddr_in client_address, socklen_t client_address_len){
    char message_sent[BUFLEN];
    sprintf(message_sent, "Received request to shutdown server\n");
    sendto(fd, message_sent, strlen(message_sent) + 1, 0, (struct sockaddr*) &client_address, client_address_len);
}

// LOGIN <username> <password>
void admin_login(char *username, char *password, int fd, struct sockaddr_in client_address, socklen_t client_address_len){
    char message_sent[BUFLEN];
    sprintf(message_sent, "Received request to login as admin with username %s and password %s\n", username, password);
    sendto(fd, message_sent, strlen(message_sent) + 1, 0, (struct sockaddr*) &client_address, client_address_len);
}
