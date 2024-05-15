CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

all: serverF client

serverF: serverF.o admin.o command_client.o
	$(CC) $(CFLAGS) -o serverF serverF.o admin.o command_client.o

serverF.o: serverF.c admin.h command_client.h
	$(CC) $(CFLAGS) -c serverF.c

admin.o: admin.c admin.h
	$(CC) $(CFLAGS) -c admin.c

command_client.o: comand_client.c command_client.h
	$(CC) $(CFLAGS) -c command_client.c

client: client.o
	$(CC) $(CFLAGS) -o client client.o

client.o: client.c
	$(CC) $(CFLAGS) -c client.c


