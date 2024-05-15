#include "dados.h"

// LOGIN <username> <password>
void client_login(char *username, char *password, int tcp_socket){
    char message_send[BUF_SIZE];
    sprintf(message_send, "Received request to login as a client with username %s and password %s\n", username, password);
    write(tcp_socket, message_send, strlen(message_send) + 1);
}

// LIST_CLASSES
void list_classes(int tcp_socket){
    char message_send[BUF_SIZE];
    sprintf(message_send, "Received request to list classes\n");
    write(tcp_socket, message_send, strlen(message_send) + 1);
}

// Student Only

// LIST_SUBSCRIBED
void list_subscribed(int tcp_socket){
    char message_send[BUF_SIZE];
    sprintf(message_send, "Received request to list subscribed classes\n");
    write(tcp_socket, message_send, strlen(message_send) + 1);
}

// SUBSCRIBE <class_name>
void subscribe_class(char *class_name, int tcp_socket){
    char message_send[BUF_SIZE];
    sprintf(message_send, "Received request to subscribe to class %s\n", class_name);
    write(tcp_socket, message_send, strlen(message_send) + 1);
}

// Professor Only

// CREATE_CLASS <class_name> <capacity>
void create_class(char *class_name, int capacity, int tcp_socket){
    char message_send[BUF_SIZE];
    sprintf(message_send, "Received request to create class %s with capacity %d\n", class_name, capacity);
    write(tcp_socket, message_send, strlen(message_send) + 1);
}

// SEND <class_name> <message>
void send_message(char *class_name, char *message, int tcp_socket){
    char message_send[BUF_SIZE];
    sprintf(message_send, "Received request to send message %s to class %s\n", message, class_name);
    write(tcp_socket, message_send, strlen(message_send) + 1);
}
