#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <semaphore.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <signal.h>
#include <strings.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <semaphore.h>

#define BUF_SIZE	200
#define BUFLEN	200
#define USERS_MAX 100
#define MAX_TURMAS 30

//Estrutura Utilizadores

typedef struct{
    char name[BUF_SIZE];
    char password[BUF_SIZE];
    char type[BUF_SIZE];
} User;

//Estrutura Turma

typedef struct {
    char name[BUF_SIZE];
    int size;
    char multicast_address[INET_ADDRSTRLEN];
    User alunos[];
} Turma;

//Estrutura Memoria Partilhada 
typedef struct {
    User users[USERS_MAX];
    Turma turmas[MAX_TURMAS];
    int n_users;
    int n_turmas;
} Shared_mem;

//Estrutura argumento a passar threads

typedef struct {
    int porta;
    Shared_mem* shared_mem;
} ThreadArgs;


// COMANDOS CLIENTES

void client_login(char *username, char *password, int tcp_socket);
void list_classes(int tcp_socket);

// Student only
void list_subscribed(int tcp_socket);
void subscribe_class(char *class_name, int tcp_socket);

// Professor only
void create_class(char *class_name, int capacity, int tcp_socket);
void send_message(char *class_name, char *message, int tcp_socket);

//        COMANDOS ADMIN 

// ADD_USER <username> <password> <role>
void add_user(char *username, char *password, char *role, int fd, struct sockaddr_in client_address, socklen_t client_address_len, Shared_mem *shared_mem);
// DEL <username>
void remove_user(char *username, int fd, struct sockaddr_in client_address, socklen_t client_address_len, Shared_mem *shared_mem);
// LIST
void list_users(int fd, struct sockaddr_in client_address, socklen_t client_address_len, Shared_mem *shared_mem);
// QUIT_SERVER
void shutdown_server(int fd, struct sockaddr_in client_address, socklen_t client_address_len);
// LOGIN <username> <password>
void admin_login(char *username, char *password, int fd, struct sockaddr_in client_address, socklen_t client_address_len);