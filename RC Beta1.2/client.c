#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#define BUF_SIZE 1024 //Definido um tamanho para o buf

int tcp_socket;

void erro(char *msg);

int main(int argc, char *argv[]) {
  char endServer[100];
  struct sockaddr_in addr;
  struct hostent *hostPtr;

  if (argc != 3) {//So sao necessarios o ip e a porta
    erro("Invalide Sintax - cliente <host> <port>\n");
  }

  strcpy(endServer, argv[1]);
  if ((hostPtr = gethostbyname(endServer)) == 0)
    erro("Não consegui obter endereço");

  int port_number = atoi(argv[2]);
  if (port_number < 0 || port_number > 65535){
	erro("Invalid Port number\n Use intenger between 1024 and 65535\n");
  }

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addr.sin_port = htons((short) port_number);
 
  if ((tcp_socket = socket(AF_INET,SOCK_STREAM,0)) == -1)
	  erro("!!!ERROR creating socket!!!");
  if (connect(tcp_socket,(struct sockaddr *)&addr,sizeof (addr)) < 0)
	  erro("!!!Error to connect!!!");

  char message_received[BUF_SIZE];
  char message_sent[BUF_SIZE];
  while(1){
	//Clear de messages stored
	memset(message_received, 0, BUF_SIZE);
	memset(message_sent, 0, BUF_SIZE);
	read(tcp_socket, message_received, BUF_SIZE);

	int pos = strcspn(message_received, "\n");
	message_received[pos] = '\0';

	printf("Message received from server: %s\n", message_received);

	//gets the user input and send them to server
	fgets(message_sent, BUF_SIZE, stdin);
	int pos2 = strcspn(message_sent, "\n");
	message_sent[pos2] = '\0';
	write(tcp_socket, message_sent, strlen(message_sent) + 1);

  }
  close(tcp_socket);
  exit(0);
}

void erro(char *msg) {
  printf("Erro: %s\n", msg);
	exit(-1);
}
