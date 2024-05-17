#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>


#define UDP_MULTICAST_PORT 5100

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

int main() {
    char ip_address[] = "192.168.1.1";

    int port=get_last_octet(ip_address);
    

    printf("port to use %d",port+UDP_MULTICAST_PORT);

    return 0;
}