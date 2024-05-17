#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <multicast_address> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in addr;
    char message[BUF_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    addr.sin_port = htons(atoi(argv[2]));

    printf("Enter a message to send: ");
    fgets(message, BUF_SIZE, stdin);
    if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    close(sockfd);
    return 0;
}