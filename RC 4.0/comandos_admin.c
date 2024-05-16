#include "dados.h"
//show menu 
void show_menu( int fd, struct sockaddr_in client_address, socklen_t client_address_len) {
    // Prepare the menu message
    char message_sent[BUFLEN];
    sprintf(message_sent, "Welcome!!\nFirst enter your username and password using LOGIN <username> <password>\n ");
    // Send the menu message to the client
    sendto(fd, message_sent, strlen(message_sent) + 1, 0, (struct sockaddr*) &client_address, client_address_len);
    sprintf(message_sent, "\nCOMMANDS AVAILABLE:\n\n");
    sendto(fd, message_sent, strlen(message_sent) + 1, 0, (struct sockaddr*) &client_address, client_address_len);
    sprintf(message_sent, "Add user: ADD_USER {username} {password} {administrador/aluno/professor}\n");
    sendto(fd, message_sent, strlen(message_sent) + 1, 0, (struct sockaddr*) &client_address, client_address_len);
    sprintf(message_sent, "Remove user: DEL {username}\n");
    sendto(fd, message_sent, strlen(message_sent) + 1, 0, (struct sockaddr*) &client_address, client_address_len);
    sprintf(message_sent, "List users: LIST\n");
    sendto(fd, message_sent, strlen(message_sent) + 1, 0, (struct sockaddr*) &client_address, client_address_len);
    sprintf(message_sent, "Shutdown server: QUIT_SERVER\n");
    sendto(fd, message_sent, strlen(message_sent) + 1, 0, (struct sockaddr*) &client_address, client_address_len);
    
}

// ADD_USER <username> <password> <role>
void add_user(char *username, char *password, char *role, int fd, struct sockaddr_in client_address, socklen_t client_address_len,Shared_mem *shared_mem,char *config_file){
    char message_sent[BUFLEN];
    sprintf(message_sent, "Received request to add user %s with password %s and role %s\n", username, password, role);
    sendto(fd, message_sent, strlen(message_sent), 0, (struct sockaddr*) &client_address, client_address_len);

    
    if (shared_mem->n_users >= USERS_MAX) {
        char *error_message = "<Server Full> Maximum number of users reached. Cannot add more users.\n";
        sendto(fd, error_message, strlen(error_message), 0, (struct sockaddr*) &client_address, client_address_len);
        return;
    }
    
    //semwait
    // Check if the username already exists
    for (int i = 0; i < shared_mem->n_users; i++) {
        if (strcmp(shared_mem->users[i].name, username) == 0) {
            char *error_message = "<Username Taken> Username already exists. Please choose a different username.\n";
            sendto(fd, error_message, strlen(error_message), 0, (struct sockaddr*) &client_address, client_address_len);
            return;
        }
    }

    // Add the new user
    strcpy(shared_mem->users[shared_mem->n_users].name, username);
    strcpy(shared_mem->users[shared_mem->n_users].password, password);
    strcpy(shared_mem->users[shared_mem->n_users].type, role);
    shared_mem->n_users++;

    //sem post
    // Send confirmation message to the client
    add_user_to_config_file(username,password,role,config_file);
    char confirmation_message[BUF_SIZE];
    sprintf(confirmation_message, "<User Added> User %s added successfully with role %s.\n", username, role);
    sendto(fd, confirmation_message, strlen(confirmation_message), 0, (struct sockaddr*) &client_address, client_address_len);
}
void add_user_to_config_file(char* username, char* password, char* role,char* config_file) {
    FILE *config = fopen(config_file, "a");
    if (config == NULL) {
        printf("Error opening config file for appending.\n");
        return;
    }
    fprintf(config, "\n%s;%s;%s", username, password, role);
    fclose(config);
}

// DEL <username>

void remove_user(char *username, int fd, struct sockaddr_in client_address, socklen_t client_address_len, Shared_mem *shared_mem, char *config_file) {
    char message_sent[BUFLEN];
    sprintf(message_sent, "Received request to remove user %s\n", username);
    sendto(fd, message_sent, strlen(message_sent), 0, (struct sockaddr*) &client_address, client_address_len);

    //semwait

    int user_found = 0;
    for (int i = 0; i < shared_mem->n_users; i++) {
        if (strcmp(shared_mem->users[i].name, username) == 0) {
            user_found = 1;
            // Shift users down to remove the user
            for (int j = i; j < shared_mem->n_users - 1; j++) {
                shared_mem->users[j] = shared_mem->users[j + 1];
            }
            shared_mem->n_users--;
            break;
        }
    }

    if (!user_found) {
        char *error_message = "<User Not Found> The specified user does not exist.\n";
        sendto(fd, error_message, strlen(error_message), 0, (struct sockaddr*) &client_address, client_address_len);
        return;
    }

    //sem post
    // Update the configuration file
    update_config_file(config_file, shared_mem);

    // Send confirmation message to the client
    char confirmation_message[BUF_SIZE];
    sprintf(confirmation_message, "<User Removed> User %s removed successfully.\n", username);
    sendto(fd, confirmation_message, strlen(confirmation_message), 0, (struct sockaddr*) &client_address, client_address_len);
}


void update_config_file(char *config_file, Shared_mem *shared_mem) {
    FILE *file = fopen(config_file, "w");
    if (file == NULL) {
        printf("Error opening config file for writing.\n");
        return;
    }

    for (int i = 0; i < shared_mem->n_users; i++) {
        fprintf(file, "%s;%s;%s\n", shared_mem->users[i].name, shared_mem->users[i].password, shared_mem->users[i].type);
    }

    fclose(file);
}

// LIST
void list_users(int fd, struct sockaddr_in client_address, socklen_t client_address_len, Shared_mem *shared_mem) {
    char message_sent[BUFLEN];
    sprintf(message_sent, "Received request to list all users\n");
    sendto(fd, message_sent, strlen(message_sent), 0, (struct sockaddr*) &client_address, client_address_len);

    //semwait
    if (shared_mem->n_users == 0) {
        char *error_message = "<No Users> There are currently no users in the system.\n";
        sendto(fd, error_message, strlen(error_message), 0, (struct sockaddr*) &client_address, client_address_len);
        return;
    }

    // Construct the user list message
    char user_list[BUFLEN] = "<User List>\n";
    for (int i = 0; i < shared_mem->n_users; i++) {
        char user_info[200];
        sprintf(user_info, "Username: %s Role: %s\n", shared_mem->users[i].name, shared_mem->users[i].type);
        strcat(user_list, user_info);
    }

    //sem post
    // Send the user list to the client
    sendto(fd, user_list, strlen(user_list), 0, (struct sockaddr*) &client_address, client_address_len);
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
