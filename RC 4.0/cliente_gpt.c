#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUF_SIZE 1024 // Defined buffer size

int tcp_socket;

void erro(char *msg);
void* receive_multicast(void* arg);

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

    char message_received[BUF_SIZE];
    char message_sent[BUF_SIZE];
    while (1) {
        // Clear stored messages
        memset(message_received, 0, BUF_SIZE);
        memset(message_sent, 0, BUF_SIZE);
        read(tcp_socket, message_received, BUF_SIZE);

        int pos = strcspn(message_received, "\n");
        message_received[pos] = '\0';

        printf("Message received from server: %s\n", message_received);

        // Check if the message is an "ACCEPTED <multicast address>"
        if (strncmp(message_received, "ACCEPTED", 8) == 0) {
            char multicast_address[INET_ADDRSTRLEN];
            sscanf(message_received, "ACCEPTED %s", multicast_address);

            // Create a new thread to receive multicast messages
            pthread_t multicast_thread;
            MulticastArgs* args = malloc(sizeof(MulticastArgs));
            strcpy(args->multicast_address, multicast_address);
            pthread_create(&multicast_thread, NULL, receive_multicast, (void*)args);
        }

        // Get user input and send to server
        fgets(message_sent, BUF_SIZE, stdin);
        int pos2 = strcspn(message_sent, "\n");
        message_sent[pos2] = '\0';
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
    MulticastArgs* args = (MulticastArgs*)arg;
    char* multicast_address = args->multicast_address;
    int multicast_socket;
    struct sockaddr_in multicast_addr;
    struct ip_mreq mreq;
    char buffer[BUF_SIZE];

    // Create a UDP socket
    if ((multicast_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error creating socket");
        free(args);
        pthread_exit(NULL);
    }

    // Allow multiple sockets to use the same port number
    u_int ttl = 1;
    if (setsockopt(multicast_socket, SOL_SOCKET, SO_REUSEADDR, &ttl, sizeof(ttl)) < 0) {
        perror("Reusing ADDR failed");
        free(args);
        pthread_exit(NULL);
    }

    // Setup the multicast address structure
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    multicast_addr.sin_port = htons(12345); // Use the port number for the multicast group

    // Bind to the multicast port
    if (bind(multicast_socket, (struct sockaddr*) &multicast_addr, sizeof(multicast_addr)) < 0) {
        perror("Binding failed");
        free(args);
        pthread_exit(NULL);
    }

    // Specify the multicast group
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_address);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    // Join the multicast group
    if (setsockopt(multicast_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("Adding membership failed");
        free(args);
        pthread_exit(NULL);
    }

    // Receive messages from the multicast group
    while (1) {
        int nbytes = recvfrom(multicast_socket, buffer, BUF_SIZE, 0, NULL, 0);
        if (nbytes < 0) {
            perror("Receiving multicast message failed");
            free(args);
            pthread_exit(NULL);
        }
        buffer[nbytes] = '\0';
        printf("Multicast message: %s\n", buffer);
    }

    // Free the argument structure
    free(args);
    pthread_exit(NULL);
}