#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>

#define BUF_SIZE 10000
#define MULTICAST_ACCEPTED "ACCEPTED"

int tcp_socket;
int multicast_exit = 0;

void erro(char *msg);
void *receive_messages(void *arg);
void *receive_multicast_messages(void *multicast_address);

int main(int argc, char *argv[]) {
    char endServer[100];
    struct sockaddr_in addr;
    struct hostent *hostPtr;

    if (argc != 3) {
        erro("Invalid Syntax - Usage: ./client <host> <port>\n");
    }

    strcpy(endServer, argv[1]);
    if ((hostPtr = gethostbyname(endServer)) == 0) {
        erro("Couldn't obtain address\n");
    }

    int port_number = atoi(argv[2]);
    if (port_number < 0 || port_number > 65535){
        erro("Invalid Port number\n Use integers between 1024 and 65535\n");
    }

    bzero((void *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
    addr.sin_port = htons((short) port_number);
 
    if ((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        erro("Error creating socket\n");
    }
    if (connect(tcp_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        erro("Error connecting\n");
    }

    // Create a thread to receive messages from the server
    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receive_messages, NULL) != 0) {
        perror("Failed to create receive thread\n");
        exit(1);
    }

    char message_sent[BUF_SIZE];
    while(1) {
        memset(message_sent, 0, BUF_SIZE);
        fgets(message_sent, BUF_SIZE, stdin);
        int pos = strcspn(message_sent, "\n");
        message_sent[pos] = '\0';
        write(tcp_socket, message_sent, strlen(message_sent) + 1);
    }

    close(tcp_socket);
    exit(0);
}

void erro(char *msg) {
    printf("Error: %s\n", msg);
    exit(-1);
}

void *receive_messages(void *arg) {
    char message_buffer[BUF_SIZE];
    ssize_t bytes_received;

    while (1) {
        // Clear the message buffer
        memset(message_buffer, 0, sizeof(message_buffer));

        // Read data into the buffer
        bytes_received = recv(tcp_socket, message_buffer, BUF_SIZE - 1, MSG_DONTWAIT);
        
        // Handle errors and end of data
        if (bytes_received == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data available
                continue;
            } else {
                // Error occurred
                perror("recv");
                break;
            }
        } else if (bytes_received == 0) {
            // Connection closed by server
            printf("Server closed connection\n");
            break;
        }

        // Null-terminate the received data
        message_buffer[bytes_received] = '\0';

        // Process the complete message
        printf("Message received from server: %s\n", message_buffer);
    }

    // Clean up and exit thread
    close(tcp_socket);
    pthread_exit(NULL);
}