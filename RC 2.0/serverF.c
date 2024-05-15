#include "dados.h"


int n_users = 0;

int admin_logged_in = 0;

int showed_menu=0;
sem_t* semShm;

Shared_mem* create_shared_memory(const char* shm_name);

Shared_mem* shared_mem; 
const char* shm_name;

int tcp_socket;
int udp_socket;

char* CONFIG_FILE; // Global variable for config file name

void print_credentials(int n_users);
void read_file(char *filename,Shared_mem *shared_mem);
bool check_user(char *name, char *password, Shared_mem *shared_mem);
bool check_type(char *name, char *type, Shared_mem *shared_mem);
void* udp(void* arg);
void* tcp(void* arg);
void process_client(int new_client, Shared_mem *shared_mem);
void erro(char *msg);
void receive_client_command(char* command, int new_client, Shared_mem *shared_mem);
void receive_admin_command(char* command, int new_client, struct sockaddr_in addr, socklen_t addrlen, Shared_mem *shared_mem);

void destroy_shared_memory(const char* shm_name, Shared_mem* shared_mem) {
    if (shared_mem != NULL) {
        // Free dynamically allocated memory for each turma's alunos
        for (int i = 0; i < shared_mem->n_turmas; i++) {
            free(shared_mem->turmas[i].alunos);
        }

        size_t shm_size = sizeof(Shared_mem);
        munmap(shared_mem, shm_size);
    }
    shm_unlink(shm_name);
}

void handle_sigint(int sig){
    if(sig == SIGINT){
        close(tcp_socket);
        close(udp_socket);

        destroy_shared_memory(shm_name,shared_mem);

        printf("Closing Server...\n");

        exit(0);
    }
}
//Comando de execução será ./class_server {port_turmas}{port_config}{ficheiro}

int main(int argc, char *argv[]){
  if (argc != 4){
	erro("Wrong number of inputs, please use ./class_server{port_turmas} {port_config} {ficheiro}\n");
  }

  //Check port availibility
  int port_turmas = atoi(argv[1]);
  int port_config = atoi(argv[2]);
  CONFIG_FILE = argv[3];

  if (port_turmas < 1024 || port_turmas > 65535 || port_config < 1024 || port_config > 65535){
     printf("<Invalid port>\n[Port must be integers between 1024 and 65535]\n");
     return 1;
  }

  //Check if ports are equal
  if (port_turmas == port_config){
     printf("<Invalid port>\n[port_turmas and port_config must be different]\n");
      return 1;
  }

  //semaforo para controlar acesso á memoria partilhada

  semShm = sem_open("/my_semaphore", O_CREAT, 0666, 1); // Initialize with value 1 (mutex)

    if (semShm == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }

    //Criar memoria partilhada

   shm_name = "/my_shared_mem";
   shared_mem = create_shared_memory(shm_name);

    if (shared_mem == NULL) {
        fprintf(stderr, "Failed to create shared memory\n");
        return EXIT_FAILURE;
    }

  
  
  
  read_file(CONFIG_FILE,shared_mem);

  
 
  
  

  pthread_t udp_thread, tcp_thread;

  ThreadArgs udp_args;
  udp_args.porta = port_config;
  udp_args.shared_mem = shared_mem;

  ThreadArgs tcp_args;
  tcp_args.porta= port_turmas;
  tcp_args.shared_mem = shared_mem;

  // Start listening for UDP messages
  pthread_create(&udp_thread, NULL, udp, &udp_args);

  // Start listening for TCP messages
  pthread_create(&tcp_thread, NULL, tcp, &tcp_args);

  //pthread_join(udp_thread, NULL);
  pthread_join(tcp_thread, NULL);

  return 0;

}

void* tcp(void* arg){

  ThreadArgs* args = (ThreadArgs*)arg;
  int port = args->porta;
  Shared_mem* shared_mem = args->shared_mem;

  int new_client;
  struct sockaddr_in addr;
  int addrlen = sizeof(addr);

  tcp_socket = socket(AF_INET, SOCK_STREAM, 0);// Create socket for TCP messages, AF_INET: IPv4 IP,SOCK_STREAM: TCP socket type,0: Use default protocol (TCP)
  if(tcp_socket < 0){
        erro("Error creating TCP socket");
  }

  memset(&addr, 0, sizeof(addr));


  bzero((void*)&addr, sizeof(addr));     // initializes the address variable to zero
  addr.sin_family = AF_INET;             // IPv4 address
  addr.sin_addr.s_addr = INADDR_ANY;     // server will listen on all available network interfaces
  addr.sin_port = htons(port);           // sets the port number to the value of port in network byte order


  // Bind
  if (bind(tcp_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(tcp_socket);
	erro("Error Binding");
  }

  if( listen(tcp_socket, 5) < 0){
    close(tcp_socket);
	erro("Error listen");
  }

  printf("TCP listening in port: %d\n",port);

  while(1){
  	if((new_client = accept(tcp_socket, (struct sockaddr *)&addr, (socklen_t *)&addrlen)) < 0){
		erro("Accept TCP failed");
	}
    	// Fork a new process to handle the client
    	pid_t pid = fork();
    	if(pid == -1){
        	erro("Error Forking process");
    	}

    	if(pid == 0){
   		    // Close the copy of the server socket in the child process
    		close(tcp_socket);

    		// Process a client
    		process_client(new_client,shared_mem);

    		// Exit the child process
    		exit(0);
    	}
    	else{
        	// Close the copy of the client socket in the parent process
        	close(new_client);
    	}
  }
  close(tcp_socket);
  return NULL;
}

void process_client(int new_client,Shared_mem *shared_mem){
    write(new_client, "Welcome to the server\n", 23);
    int bytes_received = 0;
    char buffer[BUF_SIZE];
    char msg_received[BUF_SIZE];

    // Read until the client disconnects
    while((bytes_received = read(new_client, buffer, BUF_SIZE - 1)) > 0){
        buffer[bytes_received - 1] = '\0'; // Add null terminator to the end of the message
        if(bytes_received == -1){
            perror("Error reading message");
            break;
        }

        printf("Message received - %s\n", buffer);

        // Send a message to the client
        printf(msg_received, "Message received - %s\n", buffer);

        // Interpret the command
        receive_client_command(buffer, new_client,shared_mem);
    }
    if(bytes_received == 0){
        printf("Client disconnected\n");
    }
    else{
        perror("Error reading message");
    }

    close(new_client);
}


void* udp(void* arg){

  ThreadArgs* args = (ThreadArgs*)arg;
  int port = args->porta;
  Shared_mem* shared_mem = args->shared_mem;

  
  struct sockaddr_in addr;
  char buffer[BUF_SIZE];

  if((udp_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
	erro("Error creating UPD socket");
  }

  // Address assignment
  bzero((void*) &addr, sizeof(addr));  // initializes the address variable to zero
  addr.sin_family = AF_INET;              // IPv4 address
  addr.sin_addr.s_addr = INADDR_ANY;      // server will listen on all available network interfaces
  addr.sin_port = htons(port);            // sets the port number to the value of port in network byte order


  if(bind(udp_socket,(struct sockaddr*)&addr, sizeof(addr)) == -1) {
	erro("Erro no bind");
  }

  printf("UDP listening in port: %d\n", port);

  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  
  while(1){
    memset(buffer, 0, BUF_SIZE);
  	recvfrom(udp_socket, buffer, BUF_SIZE - 1, 0, (struct sockaddr *) &client_addr, (socklen_t *)&client_addr_len);
    //printf("UDP Message: %s\n",buffer);
    if(showed_menu==0){
        show_menu(udp_socket,client_addr,client_addr_len);
        showed_menu=1;}

    receive_admin_command(buffer, udp_socket, client_addr, client_addr_len,shared_mem);
  }
  printf("Client disconnected\n");
  close(udp_socket);
  return NULL;
}

void receive_client_command(char* command, int new_client, Shared_mem *shared_mem){
    char* space = strtok(command, " ");
    if(space == NULL){
        write(new_client, "<Invalid command>\n", 18);
        return;
    }
    
    if(strcmp("LOGIN", space) == 0){
        char *error_message = "<Invalid command> - Correct Usage: LOGIN <username> <password>\n";
        // Check if the command has the correct number of arguments
        char* username = strtok(NULL, " ");
        char* password = strtok(NULL, " ");
        if(username == NULL || password == NULL){
            write(new_client, error_message, strlen(error_message));
            return;
        }
        // Check if there are no more arguments
        if(strtok(NULL, " ") != NULL){
            write(new_client, error_message, strlen(error_message));
            return;
        }
        if (check_user(username, password,shared_mem) && (check_type(username, "aluno",shared_mem) || check_type(username, "professor",shared_mem)) && !check_type(username, "admistrador",shared_mem)){
            client_login(username, password, new_client);
            return;
        }else {
            error_message = "<Invalid command> - Client not pre-saved or admin\n";
            write(new_client, error_message, strlen(error_message));
            return;
        }
    }
    
    if(strcmp("LIST_CLASSES", space) == 0){
        char *error_message = "<Invalid command> - Correct Usage: LIST_CLASSES\n";
        // Check if the command has the correct number of arguments
        if(strtok(NULL, " ") != NULL){
            write(new_client, error_message, strlen(error_message));
            return;
        }
        list_classes(new_client);
        return;
    }

    if(strcmp("LIST_SUBSCRIBED", space) == 0){
        char *error_message = "<Invalid command> - Correct Usage: LIST_SUBSCRIBED\n";
        // Check if the command has the correct number of arguments
        if(strtok(NULL, " ") != NULL){
            write(new_client, error_message, strlen(error_message));
            return;
        }
        list_subscribed(new_client);
        return;
    }

    if(strcmp("SUBSCRIBE_CLASS", space) == 0){
        char *error_message = "<Invalid command> - Correct Usage: SUBSCRIBE_CLASS <class_name>\n";
        // Check if the command has the correct number of arguments
        char* class_name = strtok(NULL, " ");
        if(class_name == NULL){
            write(new_client, error_message, strlen(error_message));
            return;
        }
        // Check if there are no more arguments
        if(strtok(NULL, " ") != NULL){
            write(new_client, error_message, strlen(error_message));
            return;
        }
        subscribe_class(class_name, new_client);
        return;
    }
    if(strcmp("CREATE_CLASS", space) == 0){
        char *error_message = "<Invalid command> - Correct Usage: CREATE_CLASS <name> <size>\n";
        // Check if the command has the correct number of arguments
        char* class_name = strtok(NULL, " ");
        char* capacity_str = strtok(NULL, " ");
        if(class_name == NULL || capacity_str == NULL){
            write(new_client, error_message, strlen(error_message));
            return;
        }
        // Check if there are no more arguments
        if(strtok(NULL, " ") != NULL){
            write(new_client, error_message, strlen(error_message));
            return;
        }
        int capacity = atoi(capacity_str);
        create_class(class_name, capacity, new_client);
        return;
    }

    if(strcmp("SEND", space) == 0){
        char *error_message = "<Invalid command> - Correct Usage: SEND <class_name> <message>\n";
        // Check if the command has the correct number of arguments
        char* class_name = strtok(NULL, " ");
        char* message = strtok(NULL, "");
        if(class_name == NULL || message == NULL){
            write(new_client, error_message, strlen(error_message));
            return;
        }
        // Check if there are no more arguments
        if(strtok(NULL, " ") != NULL){
            write(new_client, error_message, strlen(error_message));
            return;
        }
        send_message(class_name, message, new_client);
        return;
    }
    write(new_client, "<Invalid command>\n", 18);
    
}

void receive_admin_command(char* command, int new_client, struct sockaddr_in addr, socklen_t addrlen, Shared_mem *shared_mem){
    
    if (strcmp(command, "\n" ) != 0){
        printf("Command: %s\n", command);
        int ind = strcspn(command, "\n");
        command[ind] = ' ';   
        char *space = strtok(command, " ");

        if (space == NULL){
            sendto(new_client, "<Invalid command>\n", 20, 0, (struct sockaddr*) &addr, addrlen);
            return;
        }
    
        if(strcmp("LOGIN",space) == 0){
            char *error_message = "<Invalid command>\n Correct Usage: LOGIN <username> <password>\n";
            // Check if the command has the correct number of arguments
            char* username = strtok(NULL, " ");
            char* password = strtok(NULL, " ");
            if(username == NULL || password == NULL){
                sendto(new_client, error_message, strlen(error_message), 0, (struct sockaddr*) &addr, addrlen);
                return;
            }
            // Check if there are no more arguments
            if(strtok(NULL, " ") != NULL){
                sendto(new_client, error_message, strlen(error_message), 0, (struct sockaddr*) &addr, addrlen);
                return;
            }
            if (check_user(username, password,shared_mem) && check_type(username, "administrador",shared_mem)){
                admin_login(username, password, new_client, addr, addrlen);
                admin_logged_in=1;
                return;
            }else {
                error_message = "<Invalid command> - Client not pre-saved or not admistrator role\n";
                sendto(new_client, error_message, strlen(error_message), 0, (struct sockaddr*) &addr, addrlen);
                return;
            }
        }
    

        if(strcmp("ADD_USER",space) == 0 && admin_logged_in == 1){
            char *error_message = "<Invalid command>\n Correct Usage: ADD_USER <username> <password> <role>\n";
            // Check if the command has the correct number of arguments
            char* username = strtok(NULL, " ");
            char* password = strtok(NULL, " ");
            char* role = strtok(NULL, " ");
            if(username == NULL || password == NULL || role == NULL){
                sendto(new_client, error_message, strlen(error_message), 0, (struct sockaddr*) &addr, addrlen);
                return;
            }
            // Check if there are no more arguments
            if(strtok(NULL, " ") != NULL){
                sendto(new_client, error_message, strlen(error_message), 0, (struct sockaddr*) &addr, addrlen);
                return;
            }
            add_user(username, password, role, new_client, addr, addrlen,shared_mem,CONFIG_FILE);
            return;
        }

        if(strcmp("DEL",space) == 0 && admin_logged_in == 1){
            char *error_message = "<Invalid command>\n Correct Usage: DEL <username>\n";
            // Check if the command has the correct number of arguments
            char* username = strtok(NULL, " ");
            if(username == NULL){
                sendto(new_client, error_message, strlen(error_message), 0, (struct sockaddr*) &addr, addrlen);
                return;
            }
            // Check if there are no more arguments
            if(strtok(NULL, " ") != NULL){
                sendto(new_client, error_message, strlen(error_message), 0, (struct sockaddr*) &addr, addrlen);
                return;
            }
            remove_user(username, new_client, addr, addrlen,shared_mem,CONFIG_FILE);
            return;
        }

    
        if(strcmp("LIST",space) == 0 && admin_logged_in == 1){
            char *error_message = "<Invalid command>\n Correct Usage: LIST\n";
            // Check if the command has the correct number of arguments
            if(strtok(NULL, " ") != NULL){
                sendto(new_client, error_message, strlen(error_message), 0, (struct sockaddr*) &addr, addrlen);
                return;
            }
            list_users(new_client, addr, addrlen,shared_mem);
            return;
        }

        if(strcmp("QUIT_SERVER",space) == 0 && admin_logged_in == 1){
            char *error_message = "<Invalid command>\n Correct Usage: QUIT_SERVER\n";
            // Check if the command has the correct number of arguments
            if(strtok(NULL, " ") != NULL){
                sendto(new_client, error_message, strlen(error_message), 0, (struct sockaddr*) &addr, addrlen);
                return;
            }
            handle_sigint(SIGINT);
            //shutdown_server(new_client, addr, addrlen);
            return;
        }


        sendto(new_client, "<Invalid command>\n", 18, 0, (struct sockaddr*) &addr, addrlen);
    }else {
        sendto(new_client, "<Invalid command>\n", 18, 0, (struct sockaddr*) &addr, addrlen);
    }
}

void read_file(char *filename,Shared_mem *shared_mem){
    FILE* f;
    char buff[BUF_SIZE];
    f = fopen(filename, "r");

    if (f == NULL){
        erro("Erro a abrir o ficheiro\n");
    }
    while(fgets(buff, sizeof(buff), f)){
        char *name, *password, *type;
        name = strtok(buff, ";");
        password = strtok(NULL, ";");
        type = strtok(NULL, "\n");
        strcpy(shared_mem->users[shared_mem->n_users].name, name);
        strcpy(shared_mem->users[shared_mem->n_users].password, password);
        strcpy(shared_mem->users[shared_mem->n_users].type, type);
        shared_mem->n_users+=1;
        
    }
    fclose(f);
}

bool check_user(char *name, char *password,Shared_mem *shared_mem){
    int i;
    bool flag = false;
    for(i = 0; i < shared_mem->n_users; i++){
        if (strcmp(shared_mem->users[i].name, name) == 0 && strcmp(shared_mem->users[i].password,password) == 0){
            flag = true;
            break;
        }
    }
    return flag;
}

bool check_type(char *name, char *type, Shared_mem *shared_mem){
    int i;
    bool flag = false;
    for (i = 0; i < shared_mem->n_users; i++){
        if (strcmp(shared_mem->users[i].name, name) == 0 && strcmp(shared_mem->users[i].type,type) == 0){
            flag = true;
            break;
        }
    }
    return flag;
}


void erro(char *msg){
	printf("Erro: %s\n", msg);
	exit(-1);
}

Shared_mem* create_shared_memory(const char* shm_name) {
    // Open shared memory object
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return NULL;
    }

    // Set the size of the shared memory object
    size_t shm_size = sizeof(Shared_mem);
    if (ftruncate(shm_fd, shm_size) == -1) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(shm_name);
        return NULL;
    }

    // Map the shared memory object in the address space
    Shared_mem* shared_mem = (Shared_mem*)mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        shm_unlink(shm_name);
        return NULL;
    }
    shared_mem->n_users=0;
    shared_mem->n_turmas=0;

    // Initialize the shared memory
    memset(shared_mem, 0, shm_size);

    return shared_mem;
}