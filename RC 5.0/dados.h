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
#define BUFLEN	1024
#define USERS_MAX 100
#define MAX_TURMAS 30

//Estrutura Utilizadores

typedef struct{
    char name[BUF_SIZE];
    char password[BUF_SIZE];
    char type[BUF_SIZE];
    char subscribed_classes[MAX_TURMAS][BUF_SIZE];
    int n_subscribed_classes;
} User;

//Estrutura Turma

typedef struct {
    char name[BUF_SIZE];
    int size;
    char multicast_address[INET_ADDRSTRLEN];
    int current_size;
    int udp_socket;
    struct sockaddr_in udp_address;
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

void list_classes(int client_socket, Shared_mem *shared_mem);
void create_class(char *class_name, int size, int client_socket, Shared_mem *shared_mem);

// Student only
void list_subscribed(char *username, int client_socket, Shared_mem *shared_mem);
void subscribe_user_to_class(char *username, char *class_name, int client_socket, Shared_mem *shared_mem);

// Professor only
void create_class(char *class_name, int size, int client_socket, Shared_mem *shared_mem);
void send_content_to_class(char *class_name, char *content, int fd, struct sockaddr_in client_address, socklen_t client_address_len, Shared_mem *shared_mem);

// Retrieve class ip given class name

char *get_class_ip(Shared_mem *shared_mem, const char *class_name);

//        COMANDOS ADMIN 

//show menu
void show_menu(int fd, struct sockaddr_in client_address, socklen_t client_address_len);

// ADD_USER <username> <password> <role>
void add_user(char *username, char *password, char *role, int fd, struct sockaddr_in client_address, socklen_t client_address_len, Shared_mem *shared_mem,char* config_file);
void add_user_to_config_file(char* username, char* password, char* role,char* config_file);

// DEL <username>
void remove_user(char *username, int fd, struct sockaddr_in client_address, socklen_t client_address_len, Shared_mem *shared_mem, char *config_file);
void update_config_file(char *config_file, Shared_mem *shared_mem);

// LIST
void list_users(int fd, struct sockaddr_in client_address, socklen_t client_address_len, Shared_mem *shared_mem);

// QUIT_SERVER
void shutdown_server(int fd, struct sockaddr_in client_address, socklen_t client_address_len);

// LOGIN <username> <password>
void admin_login(char *username, char *password, int fd, struct sockaddr_in client_address, socklen_t client_address_len);