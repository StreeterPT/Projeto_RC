#include "dados.h"
#define UDP_MULTICAST_PORT 5100


// LIST_CLASSES

void list_classes(int client_socket, Shared_mem *shared_mem) {
    char message_sent[BUFLEN];
    sprintf(message_sent, "Received request to list all classes\n");
    write(client_socket, message_sent, strlen(message_sent)+1);

    // Semaphore wait
    //sem_wait(semShm);

    if (shared_mem->n_turmas == 0) {
        char *error_message = "<No Classes> There are currently no classes in the system.\n";
        write(client_socket,error_message,strlen(error_message)+1);
        //sem_post(semShm); // Release semaphore
        return;
    }

    // Construct the class list message
    char class_list[BUFLEN] = "<Class List>\n";
    for (int i = 0; i < shared_mem->n_turmas; i++) {
        char class_info[100]; // Buffer size for each class
        snprintf(class_info, sizeof(class_info), "CLASS %s\n", shared_mem->turmas[i].name);

        // Ensure we don't overflow class_list
        if (strlen(class_list) + strlen(class_info) < BUFLEN) {
            strcat(class_list, class_info);
        } else {
            break; // Stop adding if we reach the buffer limit
        }
    }

    // Semaphore post
    //sem_post(semShm);

    // Send the class list to the client
    
    write(client_socket,class_list,strlen(class_list)+1);
}


// Student Only

// LIST_SUBSCRIBED
void list_subscribed(char *username, int client_socket, Shared_mem *shared_mem) {
    char message_sent[BUFLEN];
    sprintf(message_sent, "Received request to list subscribed classes for user %s\n", username);
    write(client_socket,message_sent,strlen(message_sent)+1);

    // Semaphore wait
    //sem_wait(semShm);

    // Find the user
    int user_index = -1;
    for (int i = 0; i < shared_mem->n_users; i++) {
        if (strcmp(shared_mem->users[i].name, username) == 0) {
            user_index = i;
            break;
        }
    }

    if (user_index == -1) {
        char *error_message = "<User Not Found> The specified user does not exist.\n";
        write(client_socket,error_message,strlen(error_message)+1);
        //sem_post(semShm); // Release semaphore
        return;
    }

    User *user = &shared_mem->users[user_index];

    if (user->n_subscribed_classes == 0) {
        char *error_message = "<No Subscriptions> User is not subscribed to any classes.\n";
        write(client_socket,error_message,strlen(error_message)+1);
        //sem_post(semShm); // Release semaphore
        return;
    }

    // Construct the subscribed class list message
    char subscribed_list[BUFLEN] = "<Classes>\n";
    for (int i = 0; i < user->n_subscribed_classes; i++) {
        char class_info[100]; // Buffer size for each class
        const char *ip_address = get_class_ip(shared_mem, user->subscribed_classes[i]);
        snprintf(class_info, sizeof(class_info), "%s %s\n", user->subscribed_classes[i],ip_address);

        // Ensure we don't overflow subscribed_list
        if (strlen(subscribed_list) + strlen(class_info) < BUFLEN) {
            strcat(subscribed_list, class_info);
        } else {
            break; // Stop adding if we reach the buffer limit
        }
    }

    // Semaphore post
    //sem_post(semShm);

    // Send the subscribed class list to the client
    write(client_socket, subscribed_list, strlen(subscribed_list)+1);
}

// SUBSCRIBE <class_name>
void subscribe_user_to_class(char *username, char *class_name, int client_socket, Shared_mem *shared_mem) {
    //sem_wait(semShm);

    // Find the user
    int user_index = -1;
    for (int i = 0; i < shared_mem->n_users; i++) {
        if (strcmp(shared_mem->users[i].name, username) == 0) {
            user_index = i;
            break;
        }
    }

    // Find the class
    int class_index = -1;
    for (int i = 0; i < shared_mem->n_turmas; i++) {
        if (strcmp(shared_mem->turmas[i].name, class_name) == 0) {
            class_index = i;
            break;
        }
    }

    if (user_index == -1 || class_index == -1) {
        char *error_message = "<User or Class Not Found> The specified user or class does not exist.\n";
        write(client_socket,error_message,strlen(error_message)+1);
        //sem_post(semShm);
        return;
    }

    User *user = &shared_mem->users[user_index];
    Turma *turma = &shared_mem->turmas[class_index];

    // Check if the class is at capacity
    if (turma->current_size >= turma->size) {
        char *rejected_message = "REJECTED\n";
        write(client_socket,rejected_message,strlen(rejected_message)+1);
        //sem_post(semShm);
        return;
    }

    // Check if the user is already subscribed to the class
    for (int i = 0; i < turma->current_size; i++) {
        if (strcmp(turma->alunos[i].name, username) == 0) {
            char *error_message = "<Already Subscribed> User is already subscribed to this class.\n";
            write(client_socket,error_message,strlen(error_message)+1);
            //sem_post(semShm);
            return;
        }
    }

    // Subscribe the user to the class
    strcpy(turma->alunos[turma->current_size].name, username);

    turma->current_size++;

    strcpy(user->subscribed_classes[user->n_subscribed_classes],class_name);
    user->n_subscribed_classes+=1;
    //sem_post(semShm);

    char accepted_message[BUFLEN];
    sprintf(accepted_message, "ACCEPTED %s\n", turma->multicast_address);
    write(client_socket,accepted_message,strlen(accepted_message)+1);
}

char *get_class_ip(Shared_mem *shared_mem, const char *class_name) {
    // Implement this function based on your shared memory structure
    // Here, we assume shared_mem contains a list of classes with their corresponding IP addresses
    for (int i = 0; i < shared_mem->n_turmas; i++) {
        if (strcmp(shared_mem->turmas[i].name, class_name) == 0) {
            return shared_mem->turmas[i].multicast_address;
        }
    }
    return "<IP Not Found>"; // Class name not found
}
// Professor Only

// CREATE_CLASS <class_name> <capacity>
void create_class(char *class_name, int size, int client_socket, Shared_mem *shared_mem) {
    //sem_wait(semShm);

    // Check if class already exists
    for (int i = 0; i < shared_mem->n_turmas; i++) {
        if (strcmp(shared_mem->turmas[i].name, class_name) == 0) {
            char *error_message = "<Class Exists> Class with this name already exists.\n";
            write(client_socket,error_message,strlen(error_message)+1);
            //sem_post(semShm);
            return;
        }
    }

    if (shared_mem->n_turmas >= MAX_TURMAS) {
        char *error_message = "<Max Classes> Maximum number of classes reached.\n";
        write(client_socket,error_message,strlen(error_message)+1);
        //sem_post(semShm);
        return;
    }

    // Add new class
    Turma *new_class = &shared_mem->turmas[shared_mem->n_turmas];
    strcpy(new_class->name, class_name);
    new_class->size = size;
    new_class->current_size = 0;

    // Generate a multicast address
    sprintf(new_class->multicast_address, "239.0.0.%d", shared_mem->n_turmas + 1);

    // Create socket
    

    int multicast_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(multicast_socket < 0){
        printf("<Socket creation failed>\n");
        //sem_post(classes_sem);
        exit(1);
    }

    // Set multicast_address_struct to the multicast address and port

    unsigned long int address = inet_addr(new_class->multicast_address);

    struct sockaddr_in multicast_address_struct;
    memset(&multicast_address_struct, 0, sizeof(multicast_address_struct));
    multicast_address_struct.sin_family = AF_INET; // IPv4
    multicast_address_struct.sin_addr.s_addr = address; // Multicast address
    multicast_address_struct.sin_port = htons(UDP_MULTICAST_PORT+shared_mem->n_turmas); // Port
    

    int ttl = 6;
    if(setsockopt(multicast_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0){
        printf("<Set TTL failed>\n");
        close(multicast_socket);
       // sem_post(classes_sem);
        exit(1);
    }

    // Add class to shared memory

    new_class->udp_socket = multicast_socket;
    new_class->udp_address = multicast_address_struct;
    

    char confirmation_message[BUF_SIZE];
    sprintf(confirmation_message, "OK %s \n", new_class->multicast_address);
    write(client_socket,confirmation_message,strlen(confirmation_message)+1);

    shared_mem->n_turmas++;

}






// SEND <class_name> <message>
void send_content_to_class(char *class_name, char *content, int fd, struct sockaddr_in client_address, socklen_t client_address_len, Shared_mem *shared_mem) {
    //sem_wait(semShm);

    // Find the class
    int class_index = -1;
    for (int i = 0; i < shared_mem->n_turmas; i++) {
        if (strcmp(shared_mem->turmas[i].name, class_name) == 0) {
            class_index = i;
            break;
        }
    }

    if (class_index == -1) {
        char *error_message = "<Class Not Found> The specified class does not exist.\n";
        sendto(fd, error_message, strlen(error_message), 0, (struct sockaddr*) &client_address, client_address_len);
        //sem_post(semShm);
        return;
    }

    Turma *turma = &shared_mem->turmas[class_index];

    // Prepare the message
    char message[BUF_SIZE];
    snprintf(message, BUF_SIZE, "MESSAGE FROM %s: %s\n", class_name, content);

    // Multicast the message to the class's multicast address
    struct sockaddr_in multicast_addr;
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(UDP_MULTICAST_PORT); // Assuming you have a global UDP port
    inet_pton(AF_INET, turma->multicast_address, &multicast_addr.sin_addr);

    if (sendto(fd, message, strlen(message), 0, (struct sockaddr*) &multicast_addr, sizeof(multicast_addr)) < 0) {
        perror("sendto failed");
        //sem_post(semShm);
        return;
    }

    //sem_post(semShm);

    char confirmation_message[BUF_SIZE];
    sprintf(confirmation_message, "Content sent to class %s subscribers.\n", class_name);
    sendto(fd, confirmation_message, strlen(confirmation_message), 0, (struct sockaddr*) &client_address, client_address_len);
}
