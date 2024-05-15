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

#define TAM 50
#define MAX_CLIENTES 10
#define MAX_SUBSCRICAO 10

#define PORTA_MULTICAST 5000
#define GRUPO_MULTICAST "239.0.0.1"

/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/
/*                                         [STRUCTS]                                                    */
struct TopicoNoticias {
    char nome[TAM];
    char titulo[TAM * 2];
    int fd_topicos;
    int numClientes;
    int clientes[MAX_CLIENTES];
    bool permissaoCriarTopico;
    bool permissaoPublicarNoticias;
    char multicast_address[INET_ADDRSTRLEN];
    int clientes_subscritos[MAX_CLIENTES];  // Array para armazenar os IDs dos clientes subscritos a este tópico

};

struct Utilizador {
    char nome[TAM];
    char password[16];
    char tipo_user[TAM];
    bool online;
    int topicos_subscritos[MAX_SUBSCRICAO];  // Array para armazenar os IDs dos tópicos subscritos
};
struct Admin {
    char username[TAM];
    char password[16];
};

struct infoUsers {
    char userCliente[TAM];
    char passCliente[TAM];
    char subscricao[MAX_SUBSCRICAO][TAM];
};

typedef struct shared_mem {
    int pid[MAX_CLIENTES];
    int num_clientes;
    struct Utilizador utilizadores[MAX_CLIENTES]; // INFO USER
    struct infoUsers info_users[MAX_CLIENTES];  // INFO USERS ao que estao subscritos
    struct TopicoNoticias topicos[MAX_CLIENTES]; // CONJUNTO DE TOPICOS
    bool server_off;
} shared_mem;

/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/
/*                                           [ADMIN]                                                    */
void process_admin(char* fich);
int remover(char* n);
void print_lista();
void novo_user(char* nome, char* pass, char* tipo);
void login();
int verifica_login(char* username, char* pass);
int criarSocketUDP();

/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/
/*                                           [SERVER]                                                   */
void ler_config(char* nome_fich);
void userSplit(char* linha);
void insere_ordenado(struct Utilizador user);
void init_utilizadores();
bool loginUsers(int client_socket, char* username, char* password);
int compara(struct Utilizador u1, struct Utilizador u2);
void joinMulticastGroup(const char* multicastAddress);
void subscribeTopic(int client_socket, int topic_id, int jackpot);
void sendToMulticastGroup(const char* multicastAddress, const char* news);

void erro(char* s);
void cleanup(int signum);
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/
/*                                             [TCP]                                                    */
int criarSocketTCP();
void clientesTCP();
int criarSocketMulticast();
void processamento_tcp(int client_socket);
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

FILE* config_file;
bool flag_5sendto = false;

struct Admin admin;
shared_mem* shm_var;

struct sockaddr_in client_addr, server_addr;

int  sockfd_tcp, shmid, udp_socket, recv_len;
int jackpot;

sem_t* semShm;

int topic_id;
int num_utilizadores = 0;
int PORTO_CONFIG; // udp
int PORTO_NOTICIAS;

/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

int main(int argc, char* argv[]) {
    // ARGUMENTOS CONSOLA
    if (argc != 4) {
        printf("news_server {PORTO_NOTICIAS} {PORTO_CONFIG} {ficheiro configuração}\n");
        exit(0);
    }

    // DEFINICAO DOS PORTOS
    if ((PORTO_NOTICIAS = atoi(argv[1])) == 0 || (PORTO_CONFIG = atoi(argv[2])) == 0) {
        printf("news_server {PORTO_NOTICIAS} {PORTO_CONFIG} {ficheiro configuração}\n");
        exit(0);
    }

    signal(SIGINT, cleanup);// CLEANUP

    // SEMAFOROS
    sem_unlink("semShm");
    semShm = sem_open("semShm", O_CREAT | O_EXCL, 0644, 1);
    if (semShm == SEM_FAILED) {
        perror("[MAIN] ERRO SEM");
        exit(1);
    }

    // CRIA ID SHARED MEMORY
    shmid = shmget(IPC_PRIVATE, sizeof(shared_mem), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("[MAIN] ERRO SHMID");
        exit(1);
    }

    // MEMORIA COMPARTILHADA ALOCACAO
    shm_var = (shared_mem*)shmat(shmid, NULL, 0);
    if (shm_var == (shared_mem*)(-1)) {
        perror("[MAIN] ERRO SHMAT SHM");
        exit(1);
    }
    //INICIAR USER A VAZIO
    init_utilizadores();

    //CONFIG DE USERS
    ler_config(argv[3]);

    // FORK UDP/ADMIN
    int pid_udp = fork();
    if (pid_udp == 0) { // caso udp
		signal(SIGINT, cleanup);// CLEANUP
        process_admin(argv[3]); // enquanto que admin -> faz coisas
        exit(0);                // sai
    }
    else if (pid_udp < 0) {
        perror("[MAIN] ERRO PROCESSO UDP");
        exit(1);
    }
    // FORK TCP -LEITOR/JORNALISTA
    int pid_tcp = fork();
    if (pid_tcp == 0) {
		signal(SIGINT, cleanup);// CLEANUP
        clientesTCP();
        exit(0);
    }
    else if (pid_tcp < 0) {
        perror("[MAIN] ERRO PROCESSO TCP");
        exit(1);
    }

    //ESPERA QUE OS PROCESSO TERMINEM
    int status;
    while ((wait(&status) > 0)) {
        if (WIFEXITED(status)) {
            WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status)) {
            WTERMSIG(status);
        }
    }
    return 0;
}


/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void processamento_tcp(int client_socket) {
    char command[256];
    char username[256];
    char password[256];

    if (!loginUsers(client_socket, username, password)) {
        send(client_socket, "Authentication failed?.", strlen("Authentication failed?."), 0);
        return;
    }

    send(client_socket, "SUCCESS\n", strlen("SUCCESS\n"), 0);

    printf("[USER] [%s] [%s] HAS LOGGED IN\n", shm_var->utilizadores[jackpot].nome, shm_var->utilizadores[jackpot].tipo_user);

    while (1) {
        send(client_socket, "Opcoes disponiveis:\n", strlen("Opcoes disponiveis:\n"), 0);
        send(client_socket, "1. Listar topicos\n", strlen("1. Listar topicos\n"), 0);
        send(client_socket, "2. Subscrever topico\n", strlen("2. Subscrever topico\n"), 0);
        send(client_socket, "3. Criar topico\n", strlen("3. Criar topico\n"), 0);
        send(client_socket, "4. Enviar noticia\n", strlen("4. Enviar noticia\n"), 0);
        send(client_socket, "0. Sair\n", strlen("0. Sair\n"), 0);
        send(client_socket, "Escolha uma opcao: ", strlen("Escolha uma opcao: "), 0);
        recv(client_socket, command, TAM, 0);

        if (strcmp(command, "1\n") == 0) {
            // Listar tópicos
            for (int i = 0; i < MAX_SUBSCRICAO; i++) {
                if (shm_var->topicos[i].nome[0] != '\0') {
                    char topic_info[TAM * 4];
                    snprintf(topic_info, TAM * 4, "[ID]: %s [Nome]: %s ", shm_var->topicos[i].nome, shm_var->topicos[i].titulo);

                    // Check if the topic has associated clients
                    if (shm_var->topicos[i].numClientes > 0) {
                        strcat(topic_info, "[Tópicos Agregados]: ");

                        // Iterate over the associated clients
                        for (int j = 0; j < shm_var->topicos[i].numClientes; j++) {
                            char client_name[TAM];
                            snprintf(client_name, TAM, "%s ", shm_var->utilizadores[shm_var->topicos[i].clientes[j]].nome);
                            strcat(topic_info, client_name);
                        }
                    }

                    send(client_socket, topic_info, strlen(topic_info) + 1, 0);
                }
            }
        }

        else if (strcmp(command, "2\n") == 0) {
            // Subscrever tópico
            send(client_socket, "Insira o ID do tópico para subscrever: ", strlen("Insira o ID do tópico para subscrever: "), 0);
            char topic_id_str[TAM];
            recv(client_socket, topic_id_str, TAM, 0);
            int topic_id = atoi(topic_id_str);

            if (topic_id >= 0 && topic_id < MAX_SUBSCRICAO && shm_var->topicos[topic_id].nome[0] != '\0') {
                subscribeTopic(client_socket, topic_id, jackpot);
            }
            else {
                send(client_socket, "ID do tópico inválido.", strlen("ID do tópico inválido."), 0);
            }
        }
        else if (strcmp(command, "3\n") == 0) {
            // Criar tópico (apenas para jornalistas)
            if (strcmp(shm_var->utilizadores[jackpot].tipo_user, "jornalista") == 0) {
                printf("[JORNAL] %s ENTROU EDICAO \n", shm_var->utilizadores[jackpot].nome);
                send(client_socket, "Insira o ID do tópico: ", strlen("Insira o ID do tópico: "), 0);
                char topic_id_str[TAM];
                recv(client_socket, topic_id_str, TAM * 4, 0);
                printf("[JORNAL TOPICO] %s CRIOU [%s]\n", shm_var->utilizadores[jackpot].nome, topic_id_str);
                topic_id = atoi(topic_id_str);

                if (topic_id >= 0 && topic_id < MAX_CLIENTES) {
                    bool topic_exists = false;
                    for (int i = 0; i < MAX_CLIENTES; i++) {
                        if (i != topic_id && strcmp(shm_var->topicos[i].nome, topic_id_str) == 0) {
                            topic_exists = true;
                            break;
                        }
                    }

                    if (!topic_exists) {
                        strcpy(shm_var->topicos[topic_id].nome, topic_id_str);
                        send(client_socket, "Insira o nome do tópico: ", strlen("Insira o nome do tópico: "), 0);
                        recv(client_socket, shm_var->topicos[topic_id].titulo, TAM, 0);
                        send(client_socket, "Insira o endereço multicast: ", strlen("Insira o endereço multicast: "), 0);
                        recv(client_socket, shm_var->topicos[topic_id].multicast_address, TAM, 0);
                        shm_var->topicos[topic_id].permissaoCriarTopico = true;
                        shm_var->topicos[topic_id].permissaoPublicarNoticias = true;
                         send(client_socket, "Tópico criado com sucesso.", strlen("Tópico criado com sucesso."), 0);
                    }
                    else {
                        send(client_socket, "Já existe um tópico com esse ID.", strlen("Já existe um tópico com esse ID."), 0);
                    }
                }
                else {
                    send(client_socket, "ID de tópico inválido.", strlen("ID de tópico inválido."), 0);
                }
            }
            else {
                send(client_socket, "Você não tem permissão para criar tópicos.", strlen("Você não tem permissão para criar tópicos."), 0);
            }
        }
        else if (strcmp(command, "4\n") == 0) {
            // Enviar notícia (apenas para jornalistas)
            if (strcmp(shm_var->utilizadores[jackpot].tipo_user, "jornalista") == 0) {
                printf("[JORNAL] %s\n", shm_var->utilizadores[jackpot].nome);
                send(client_socket, "Insira o ID do tópico: ", strlen("Insira o ID do tópico: "), 0);
                char topic_id_str[TAM];
                recv(client_socket, topic_id_str, TAM, 0);
                topic_id = atoi(topic_id_str);

                if (topic_id >= 0 && topic_id < MAX_CLIENTES && strcmp(shm_var->topicos[topic_id].nome, topic_id_str) == 0) {
                    if (shm_var->topicos[topic_id].permissaoPublicarNoticias) {
                        send(client_socket, "Insira a notícia: ", strlen("Insira a notícia: "), 0);
                        char news[TAM];
                        recv(client_socket, news, TAM, 0);
                        char message[TAM * 4];
                        snprintf(message, TAM * 4, "[JORNAL] [%s]: %s", shm_var->utilizadores[jackpot].nome, news);
                        sendToMulticastGroup(shm_var->topicos[topic_id].multicast_address, message);
                        send(client_socket, "Notícia enviada com sucesso.", strlen("Notícia enviada com sucesso."), 0);
                    }
                    else {
                        send(client_socket, "Você não tem permissão para publicar notícias nesse tópico.", strlen("Você não tem permissão para publicar notícias nesse tópico."), 0);
                    }
                }
                else {
                    send(client_socket, "ID de tópico inválido.", strlen("ID de tópico inválido."), 0);
                }
            }
            else {
                send(client_socket, "Você não tem permissão para enviar notícias.", strlen("Você não tem permissão para enviar notícias."), 0);
            }
        }
        else if (strcmp(command, "0\n") == 0) {
            break;
        }
        else {
            send(client_socket, "Comando inválido.", strlen("Comando inválido."), 0);
        }
    }

    printf("[USER] [%s] [%s] HAS LOGGED OUT\n", shm_var->utilizadores[jackpot].nome, shm_var->utilizadores[jackpot].tipo_user);

    close(client_socket);
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/       [SOCKETS]         /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/
int criarSocketMulticast() {
    int sockfd;

    // CRIA SOCKET UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[SOCKET M] ERRO CRIAR SOCKET");
        exit(1);
    }

    // Define as opções de socket para permitir multicast
    int opcao = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opcao, sizeof(opcao)) < 0) {
        perror("[SOCKET M] ERRO OPCOES SOCKET");
        exit(1);
    }

    // Define o endereço de destino para o grupo multicast
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(9000);

    // Associa o socket ao endereço
    if (bind(sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("[SOCKET M] ERRO ASSOCIAR SOCKET");
        exit(1);
    }

    // Junta-se ao grupo multicast
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(GRUPO_MULTICAST);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
        perror("[SOCKET M] ERRO JOIN MULTICAST");
        exit(1);
    }

    return sockfd;
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/
int criarSocketTCP() {
    int sockfd;
    // CRIA SOCKET TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[TCP] ERRO AO CRIAR SOCKET");
        exit(1);
    }

    // ENDERECO DESTINO
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(9001);

    // ASSOCIA SOCKET AO ENDERECO
    if (bind(sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("[TCP] ERRO AO ASSOCIAR SOCKET");
        exit(1);
    }
    // REUTILIZA OS SOCKETS
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("[TCP] ERRO A REUTILIZAR SOCKET");
        exit(1);
    }


    // FICA A ESCUTA
    if (listen(sockfd, shm_var->num_clientes) < 0) {
        perror("[TCP] ERRO AO ESCUTAR SOCKET");
        exit(1);
    }

    return sockfd;
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/
int criarSocketUDP() {
    int sockfd;

    // CRIA SOCKE UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[UDP] ERRO AO CRIAR SOCKET");
        exit(1);
    }

    // ENDEREC DESTINO UDP
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORTO_CONFIG);

    // REUTILIZA O DESTINO ? -> explicar
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("[UDP] ERRO A REUTILIZAR SOCKET");
        exit(1);
    }

    // ASSOCIA SOCKET UDP A ENDEREC
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[UDP] ERRO A ASSOCIAR SOCKET");
        exit(1);
    }

    return sockfd;
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void ler_config(char* nome_fich)
{
    FILE* fich;
    char Linha[TAM];
    const char sep[3] = { "\r\n" };
    char* token;
    char frase[TAM];

    fich = fopen(nome_fich, "r");

    if (fich == NULL) {
        printf("[FICH] IMPOSSIVEL ABRIR!\n");
        exit(-1);
    }

    while (fgets(Linha, sizeof(Linha), fich) != NULL) {
        token = strtok(Linha, sep);
        while (token != NULL) {
            strcpy(frase, token);
            userSplit(frase);
            token = strtok(NULL, sep);
        }
    }
    fclose(fich);
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void userSplit(char* linha)
{
    struct Utilizador user;

    const char sep[4] = { ";\r\n" };
    char* token;
    char dados[20][TAM];
    int i = 0;

    token = strtok(linha, sep);
    while (token != NULL && i < 3) {  //(nome, password, tipo_user)
        strcpy(dados[i++], token);
        token = strtok(NULL, sep);
    }
    //dados[i][0] = '\0';

    if (i == 3) {
        if (strcmp(dados[2], "administrador") == 0) { // falta se houver mais um admin nao dmite
            strcpy(user.nome, dados[0]);
            strcpy(user.password, dados[1]);
            strcpy(user.tipo_user, dados[2]);
            insere_ordenado(user);

            // printf("[TRAT. DADOS ADMIN] %s %s\n", user.nome, user.password);
        }
        else {
            strcpy(user.nome, dados[0]);
            strcpy(user.password, dados[1]);
            strcpy(user.tipo_user, dados[2]);
            insere_ordenado(user);
            //printf("[TRAT. DADOS] %s %s %s\n", user.nome, user.password, user.tipo_user);

        }
    }
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void insere_ordenado(struct Utilizador user) {
    sem_wait(semShm);
    //printf("[INSERE] [%s] \n", user.nome);
    int i = 0;
    while (i < shm_var->num_clientes && compara(user, shm_var->utilizadores[i]) > 0) {
        i++;
    }

    if (i < MAX_CLIENTES) {
        // EMPURRA
        for (int j = MAX_CLIENTES - 1; j > i; j--) {
            strcpy(shm_var->utilizadores[j].nome, shm_var->utilizadores[j - 1].nome);
            strcpy(shm_var->utilizadores[j].password, shm_var->utilizadores[j - 1].password);
            strcpy(shm_var->utilizadores[j].tipo_user, shm_var->utilizadores[j - 1].tipo_user);

        }

        // INSERE NA POSICAO
        strcpy(shm_var->utilizadores[i].nome, user.nome);
        strcpy(shm_var->utilizadores[i].password, user.password);
        strcpy(shm_var->utilizadores[i].tipo_user, user.tipo_user);

        // printf("[INSERE NOME] [%s] \n", shm_var->utilizadores[i].nome);
    }
    shm_var->num_clientes++;

    sem_post(semShm);
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

int compara(struct Utilizador u1, struct Utilizador u2) {
    size_t i = 0;
    size_t l1 = strlen(u1.nome);
    size_t l2 = strlen(u2.nome);
    //printf("[COMP] [%s] [%s]\n", u1.nome, u2.nome);

    while (i < l1 && i < l2) {
        if (toupper(u1.nome[i]) < toupper(u2.nome[i])) {
            return -1;
        }
        else if (toupper(u1.nome[i]) > toupper(u2.nome[i])) {
            return 1;
        }
        i++;
    }
    if (l1 < l2) {
        return -1;
    }
    else if (l1 > l2) {
        return 1;
    }
    return 0;
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void init_utilizadores() {
    // INICIALIZA OS USERS COM VALORES VAZIOS
    for (int i = 0; i < MAX_CLIENTES; i++) {
        strcpy(shm_var->utilizadores[i].nome, "");
        strcpy(shm_var->utilizadores[i].password, "");
        strcpy(shm_var->utilizadores[i].tipo_user, "");
        //printf("[INIT USERS] [%s] [%s] [%s]\n", shm_var->utilizadores[i].nome, shm_var->utilizadores[i].password, shm_var->utilizadores[i].tipo_user);
    }

    // INICIALIZA OS TOPICOS
    for (int i = 0; i < MAX_CLIENTES; i++) {
        strcpy(shm_var->topicos[i].nome, "");
        strcpy(shm_var->topicos[i].titulo, "");
        shm_var->topicos[i].permissaoCriarTopico = false;
        shm_var->topicos[i].permissaoPublicarNoticias = false;
    }

    shm_var->num_clientes = 0;
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void login() {
    int n = 0;
    char buf[1024];
    socklen_t client_len = sizeof(client_addr);

    // LIMPA 4* (nc -u -v)
    if (flag_5sendto == false) {

        while (n < 5) {
            char palavra[20][TAM];
            char* token;
            const char sep[4] = " \r\n";
            int j = 0;

            bzero(buf, sizeof(buf));
            bzero(palavra, sizeof(palavra));

            sendto(udp_socket, buf, TAM, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));

            if ((recv_len = recvfrom(udp_socket, buf, TAM, 0, (struct sockaddr*)&client_addr, (socklen_t*)&client_len)) == -1) {
                erro("[LOGIN] ERRO NO RCVFROM");
            }
            buf[recv_len] = '\0';

            token = strtok(buf, sep);
            while (token != NULL) {
                strcpy(palavra[j++], token);
                token = strtok(NULL, sep);
            }
            n++;
        }
        flag_5sendto = true;
    }

    bzero(buf, sizeof(buf));
    sprintf(buf, "ADMIN: login {username} {password}\n");
    sendto(udp_socket, buf, TAM, 0, (struct sockaddr*)&client_addr, sizeof(client_addr)); // ENVIA MENS. ADMIN:

    while (1) {
        char* token;
        const char sep[4] = " \r\n";
        char palavra[20][TAM];
        int i = 0;
        char buf[1024];

        bzero(buf, sizeof(buf));
        bzero(palavra, sizeof(palavra));

        // ESPERA POR RESPOSTA
        if ((recv_len = recvfrom(udp_socket, buf, TAM, 0, (struct sockaddr*)&client_addr, (socklen_t*)&client_len)) == -1) {
            erro("[LOGIN][WHILE] ERRO NO RECVFROM");
        }
        // Ignore the remaining content (previous buffer)
        buf[recv_len] = '\0';

        //DIVIDE A RESPOSTA
        token = strtok(buf, sep);
        while (token != NULL) {
            strcpy(palavra[i++], token);
            token = strtok(NULL, sep);
        }
        palavra[i][0] = '\0';
        //TRATAMENTO DE DADOS
        if (strcmp(palavra[0], "login") == 0) {
            if (i != 3) {
                sprintf(buf, "ADMIN: login {username} {password}\n");
                sendto(udp_socket, buf, TAM, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
            }
            else {
                //VERIFICACAO NA LISTA DE USERS SE EXISTE OU NAO
                int result = verifica_login(palavra[1], palavra[2]);
                if (result == 1) {
                    sprintf(buf, "Login efetuado com sucesso!\n\n");
                    sendto(udp_socket, buf, TAM, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                    break;
                }
                else if (result == 2) {
                    sprintf(buf, "Utilizador tem de ser do tipo \"administrador\"!\nADMIN: login {username} {password}\n");
                    sendto(udp_socket, buf, TAM, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                }
                else if (result == 3) {
                    sprintf(buf, "Dados incorretos!\nADMIN: login {username} {password}\n");
                    sendto(udp_socket, buf, TAM, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                }
            }
        }
        else {
            sprintf(buf, "WRONG COMMAND!\nADMIN: login {username} {password}\n");
            sendto(udp_socket, buf, TAM, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
        }
    }
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

int verifica_login(char* username, char* pass) {
    int opt = 0;
    // PARA O NUMERO DE CLIENTES
    if (shm_var->num_clientes == 0) {
        return opt;
    }
    //VERIFICAMOS SE O QUE ENTROU PERTENCE OU NAO AO ADMIN
    for (int i = 0; i < shm_var->num_clientes; i++) {
        if (strcmp(shm_var->utilizadores[i].nome, username) == 0 &&
            strcmp(shm_var->utilizadores[i].password, pass) == 0) {
            if (strcmp(shm_var->utilizadores[i].tipo_user, "administrador") == 0) {
                shm_var->utilizadores[i].online = true; // NAO ACHO QUE SEJA NECESSARIO ?
                opt = 1; // OPCAO 1 -> LOGIN BEM SUCEDIDO
                return opt;
            }
            else {
                opt = 2; // OPCAO 2 -> LOGIN INVALIDO
                return opt;
            }
        }
    }

    opt = 3; // OPCAO 3 -> COMANDO ERRADO
    return opt;
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void novo_user(char* nome, char* pass, char* tipo) {
    int opt = 0;
    struct Utilizador new_user;
    strcpy(new_user.nome, nome);
    strcpy(new_user.password, pass);
    strcpy(new_user.tipo_user, tipo);
    new_user.online = false;

    for (int i = 0; i < shm_var->num_clientes; i++) {
        if (strcmp(shm_var->utilizadores[i].nome, new_user.nome) == 0) {
            printf("Já existe um utilizador com esse username!\n");
            opt += 1;
            break;
        }
    }

    if (opt == 0) {
        int a = shm_var->num_clientes;
        // Insert new_user into utilizadores array
        strcpy(shm_var->utilizadores[a].nome, new_user.nome);
        strcpy(shm_var->utilizadores[a].password, new_user.password);
        strcpy(shm_var->utilizadores[a].tipo_user, new_user.tipo_user);
        shm_var->utilizadores[a].online = new_user.online;

        printf("Utilizador inserido com sucesso!\n");
        shm_var->num_clientes++;
    }
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

int remover(char* n) {
    int opt = 0;
    for (int i = 0; i < shm_var->num_clientes; i++) {
        if (strcmp(shm_var->utilizadores[i].nome, n) == 0) {
            if (shm_var->utilizadores[i].online == true) {
                opt = 1;
                return opt;
            }

            for (int j = i; j < shm_var->num_clientes - 1; j++) {
                strcpy(shm_var->utilizadores[j].nome, shm_var->utilizadores[j + 1].nome);
                strcpy(shm_var->utilizadores[j].password, shm_var->utilizadores[j + 1].password);
                strcpy(shm_var->utilizadores[j].tipo_user, shm_var->utilizadores[j + 1].tipo_user);
                shm_var->utilizadores[j].online = shm_var->utilizadores[j + 1].online;
            }

            shm_var->num_clientes--;
            opt = 2;
            return opt;
        }
    }

    return opt;
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void print_lista() {
    char buf[256];

    for (int i = 0; i < shm_var->num_clientes; i++) {
        sprintf(buf, "Nome: %s, Password: %s, Tipo de Utilizador: %s\n",
            shm_var->utilizadores[i].nome, shm_var->utilizadores[i].password, shm_var->utilizadores[i].tipo_user);
        sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
    }
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void clientesTCP() {

    int socketfd;
    //printf("[TCP SOCKET] [%d]\n", socketfd);
    int client;
    struct sockaddr_in address, client_address;
    socklen_t client_address_size;

    bzero((void*)&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORTO_NOTICIAS);

    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        erro("[CLIENT TCP] ERRO SOCKET CLIENTE TCP");
    if (bind(socketfd, (struct sockaddr*)&address, sizeof(address)) < 0)
        erro("[CLIENT TCP] ERRO BIND CLIENTE TCP");
    if (listen(socketfd, 5) < 0)
        erro("[CLIENT TCP] ERRO LISTEN CLIENTE TCP");
    client_address_size = sizeof(client_address);

    int count = 0;
    while (1) {
        while (waitpid(-1, NULL, WNOHANG) > 0); // ESPERA SEM BLOQUEAR
        if (count <= 5) {
            client = accept(socketfd, (struct sockaddr*)&client_address, &client_address_size);
            count++;
            if (fork() == 0) {
                processamento_tcp(client);
                exit(0);
                count--;
            }
        }
    }

    close(socketfd);
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void process_admin(char* fich) {
    // CRIA SOCKET UDP
    udp_socket = criarSocketUDP();
    char buf[1024];
	bool flag_admin = false;
    //VERIFICACAO LOGIN

    while (1) {
		login();

        sprintf(buf, "----------------\n| COMANDOS ADMIN |\n----------------\n.ADD_USER {username} {password} {administrador/leitor/jornalista}\n.DEL {username}\n.LIST\n.QUIT\n.QUIT_SERVER\n\n");
        sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr)); //ENVIA COMANDO ADMIN
        while (1) {
            char* token;
            const char sep[4] = " \r\n";
            char palavra[20][TAM];
            int i = 0;
            socklen_t client_len = sizeof(client_addr);

            bzero(buf, sizeof(buf)); //LIMPA BUF
            bzero(palavra, sizeof(palavra));

            if ((recv_len = recvfrom(udp_socket, buf, sizeof(buf), 0, (struct sockaddr*)&client_addr, &client_len)) == -1) { //ESPERA P RECEBER DE COMANDOS ADMIN
                erro("[ADMIN] ERRO RECVFROM");
            }
            buf[recv_len] = '\0';

            token = strtok(buf, sep);
            while (token != NULL) {
                strcpy(palavra[i++], token);
                token = strtok(NULL, sep);
            }
            palavra[i][0] = '\0';

            if (strcmp(palavra[0], "QUIT") == 0) {
                sprintf(buf, "LOGOUT!\n\n");
                sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                break;
            }
            else if (strcmp(palavra[0], "QUIT_SERVER") == 0) {
                printf("CLOSING SERVER...\n");
                sprintf(buf, "SV CLOSED!\n");
                sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                flag_admin = true;
                break;
            }
            else if (strcmp(palavra[0], "LIST") == 0) {
                print_lista();
            }
            else if (strcmp(palavra[0], "ADD_USER") == 0) {
                if (i != 4) {
                    sprintf(buf, "ADD_USER {username} {password} {administrador/leitor/jornalista}\n");
                    sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                }
                else {
                    if (strcmp(palavra[3], "administrador") != 0 && strcmp(palavra[3], "jornalista") != 0 && strcmp(palavra[3], "leitor") != 0) {
                        sprintf(buf, "ADD_USER {username} {password} {administrador/leitor/jornalista}\n");
                        sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                    }
                    else {
                        novo_user(palavra[1], palavra[2], palavra[3]);
                    }
                }
            }
            else if (strcmp(palavra[0], "DEL") == 0) {
                if (i != 2) {
                    sprintf(buf, "DEL {username}\n");
                    sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                }
                else {
                    if (shm_var->num_clientes == 0) {
                        sprintf(buf, "LIST IS EMPTY!\n");
                        sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                    }
                    else {
						if(remover(palavra[1]) == 0){
							sprintf(buf, "UNABLE TO REMOVE!\n");
                            sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
						}

                        else if (remover(palavra[1]) == 1) {
                            sprintf(buf, "USER ONLINE! UNABLE TO REMOVE!\n");
                            sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                        }
                        else {
                            sprintf(buf, "USER HAS BEEN REMOVED!\n");
                            sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                        }
                    }
                }
            }
            else {
                sprintf(buf, "WRONG COMMAND!\n");
                sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
            }
        }
		if(flag_admin == true){
			kill(0, SIGTERM);
		}
    }
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

bool loginUsers(int client_socket, char* username, char* password) { //LOGIN , ENQUANTO VERIFICACAO
    send(client_socket, "Username: ", strlen("Username: "), 0);
    recv(client_socket, username, sizeof(username), 0);

    send(client_socket, "Password: ", strlen("Password: "), 0);
    recv(client_socket, password, sizeof(password), 0);

    // lixo
    username[strcspn(username, "\n")] = '\0';
    password[strcspn(password, "\n")] = '\0';


    // printf("[USER ENTRA] %s %s\n", username, password);

    for (int i = 0; i < shm_var->num_clientes; i++) {

        //printf("[LOGIN USER] %s %s\n", shm_var->utilizadores[i].nome, shm_var->utilizadores[i].password);
        if (strcmp(username, shm_var->utilizadores[i].nome) == 0 && strcmp(password, shm_var->utilizadores[i].password) == 0) {
            jackpot = i;
            return true;
        }
    }

    return false;
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void joinMulticastGroup(const char* multicastAddress) {
    struct ip_mreq mreq;
    struct sockaddr_in multicast_addr;
    int multicast_socket;
    //ENDERECO MULTICAST
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(PORTA_MULTICAST);
    multicast_addr.sin_addr.s_addr = inet_addr(multicastAddress);
    //SOCKET
    multicast_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (multicast_socket < 0) {
        perror("[JOIN MULTICAST] ERRO AO CRIAR SOCKET MULTICAST");
        exit(1);
    }
    //SETREUSE
    mreq.imr_multiaddr = multicast_addr.sin_addr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(multicast_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("[JOIN MULTICAST] ERRO AO JUNTAR AO GRUPO MULTICAST");
        exit(1);
    }

    close(multicast_socket);
}

void subscribeTopic(int client_socket, int topic_id, int jackpot) {
    // VERIFICA SUBSCRICAO
    bool is_subscribed = false;
    for (int i = 0; i < shm_var->topicos[topic_id].numClientes; i++) {
        if (shm_var->topicos[topic_id].clientes_subscritos[i] == jackpot) {
            is_subscribed = true;
            break;
        }
    }

    if (!is_subscribed) {
        // ADICIONA A LISTA DE TOPICOS SUBS
        if (shm_var->topicos[topic_id].numClientes < MAX_CLIENTES) {
            shm_var->topicos[topic_id].clientes_subscritos[shm_var->topicos[topic_id].numClientes] = jackpot;
            shm_var->topicos[topic_id].numClientes++;

            // JUNTA AO GRUPO MULTICAST
            joinMulticastGroup(shm_var->topicos[topic_id].multicast_address);

            char success_msg[TAM];
            sprintf(success_msg, "Subscrição realizada com sucesso no multicast %s.", shm_var->topicos[topic_id].multicast_address);
            send(client_socket, success_msg, strlen(success_msg), 0);

        } else {
            send(client_socket, "Não é possível subscrever mais tópicos.", strlen("Não é possível subscrever mais tópicos."), 0);
        }
    } else {
        char already_subscribed_msg[TAM * 4];
        sprintf(already_subscribed_msg, "Você já está inscrito no tópico %s.", shm_var->topicos[topic_id].nome);
        send(client_socket, already_subscribed_msg, strlen(already_subscribed_msg), 0);
    }
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void sendToMulticastGroup(const char* multicastAddress, const char* news) {
    struct sockaddr_in multicast_addr;
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(PORTA_MULTICAST);
    multicast_addr.sin_addr.s_addr = inet_addr(multicastAddress);

    int multicast_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (multicast_socket < 0) {
        perror("[SEND MULTICAST] ERRO AO CRIAR MULTICAST");
        exit(1);
    }

    if (sendto(multicast_socket, news, strlen(news), 0, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr)) < 0) {
        perror("[SEND MULTICAST] ERRO AO ENVIAR NOTICIA PARA MULTICAST");
        exit(1);
    }

    close(multicast_socket);
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void cleanup(int signum) {
	printf("CLEANUP DONE!\n");	
	sem_close(semShm);
	sem_destroy(semShm);

	close(udp_socket);
	close(sockfd_tcp);

	shmdt(shm_var);

	if (shmctl(shmid, IPC_RMID, NULL) == -1) {
		perror("Erro ao destruir a memória compartilhada");
		exit(1);
	}

	kill(0, SIGTERM);
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/

void erro(char* s) {
    perror(s);
    exit(1);
}
/*/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\*/