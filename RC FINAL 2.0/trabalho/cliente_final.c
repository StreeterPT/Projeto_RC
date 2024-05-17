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

#define BUF_SIZE 5000 // Defined buffer size

#define UDP_MULTICAST_PORT 5100


int tcp_socket;

void erro(char *msg);
void* receive_multicast(void* arg);
void* receive_messages(void *arg);

typedef struct {
    char multicast_address[INET_ADDRSTRLEN];
} MulticastArgs;

int main(int argc, char *argv[]) {
    char endServer[100];
    struct sockaddr_in addr;
    struct hostent *hostPtr;

    if (argc != 3) { // Only IP and port are needed
        erro("Invalid Syntax - client <host> <port>\n");
    }

    strcpy(endServer, argv[1]);
    if ((hostPtr = gethostbyname(endServer)) == 0)
        erro("Couldn't get address");

    int port_number = atoi(argv[2]);
    if (port_number < 0 || port_number > 65535) {
        erro("Invalid Port number\n Use integer between 1024 and 65535\n");
    }

    bzero((void *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
    addr.sin_port = htons((short) port_number);

    if ((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        erro("!!!ERROR creating socket!!!");
    if (connect(tcp_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        erro("!!!Error connecting!!!");

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

void* receive_multicast(void* arg) {
    char* multicast_address = (char*)arg;
    int multicast_socket;
    struct sockaddr_in multicast_addr;
    struct ip_mreq mreq;
    char buffer[BUF_SIZE];

    // Create a UDP socket
    if ((multicast_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error creating socket");
        pthread_exit(NULL);
    }

    // Allow multiple sockets to use the same port number
    int ttl = 5;
    if (setsockopt(multicast_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        perror("Setting TTL failed");
        pthread_exit(NULL);
    }
    int port = get_last_octet(multicast_address);

    // Setup the multicast address structure
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    multicast_addr.sin_port = htons(UDP_MULTICAST_PORT+port); // Use the port number for the multicast group

    // Bind to the multicast port
    if (bind(multicast_socket, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr)) < 0) {
        perror("Binding failed");
        pthread_exit(NULL);
    }

    // Specify the multicast group
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_address);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    // Join the multicast group
    if (setsockopt(multicast_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("Adding membership failed");
        pthread_exit(NULL);
    }

    // Receive messages from the multicast group
    while (1) {
        int nbytes = recvfrom(multicast_socket, buffer, BUF_SIZE, 0, NULL, 0);
        if (nbytes < 0) {
            perror("Receiving multicast message failed");
            pthread_exit(NULL);
        }
        buffer[nbytes] = '\0';
        printf("Multicast message: %s\n", buffer);
    }

    pthread_exit(NULL);
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

        if (strncmp(message_buffer, "ACCEPTED", 8) == 0) {
            char multicast_address[INET_ADDRSTRLEN];
            sscanf(message_buffer, "ACCEPTED %s", multicast_address);

            // Create a new thread to receive multicast messages
            pthread_t multicast_thread;
            MulticastArgs* args = malloc(sizeof(MulticastArgs));
            strcpy(args->multicast_address, multicast_address);
            pthread_create(&multicast_thread, NULL, receive_multicast, (void*)args);
        }
    }

    // Clean up and exit thread
    close(tcp_socket);
    pthread_exit(NULL);
}

int get_last_octet(const char *ip_address) {
    char last_octet[10];
    // Duplicate the IP address string to avoid modifying the original
    char ip_copy[INET_ADDRSTRLEN];
    strncpy(ip_copy, ip_address, INET_ADDRSTRLEN);
    
    // Split the string by '.' and get the last token
    char *token = strtok(ip_copy, ".");
    char *prev_token = NULL;
    
    while (token != NULL) {
        prev_token = token; // Keep track of the last token
        token = strtok(NULL, ".");
    }

    // Copy the last token to the output variable
    if (prev_token != NULL) {
        strncpy(last_octet, prev_token, INET_ADDRSTRLEN);
    }
    return atoi(last_octet);
}