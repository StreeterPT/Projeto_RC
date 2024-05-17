#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>

#define BUF_SIZE	200

void* receive_multicast(void* arg);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <multicast_address>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, receive_multicast, argv[1]) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(thread_id, NULL); // Wait for the thread to finish
    return 0;
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

    // Setup the multicast address structure
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    multicast_addr.sin_port = htons(12345); // Use the port number for the multicast group

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