#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <unistd.h>
#include <string.h>

#define MAXDATASIZE 100000

typedef struct
{
	int sock;
	struct sockaddr address;
	int addr_len;
} connection_t;

void * worker(void * ptr)
{
	char *buffer;
	buffer = (char *)malloc((MAXDATASIZE)*sizeof(char));
	char ch;
	char *message;
	message = (char *)malloc((MAXDATASIZE)*sizeof(char));
	int current_len = 0;
	int current_size = MAXDATASIZE;
	connection_t * conn;
	long addr = 0;

	if (!ptr) pthread_exit(0); 
	conn = (connection_t *)ptr;

	/* read length of message */
	//read(conn->sock, &len, sizeof(int));

	//buffer[len] = 0;

	/* read message */
	recv(conn->sock, buffer, MAXDATASIZE,0);
	buffer[strlen(buffer)] = '\0';
	printf("%s\n",buffer);
	
	FILE *fp;
	fp=fopen(buffer, "r");

	while( ( ch = fgetc(fp) ) != EOF ){
		//**note:error might occur when reallocate
		current_len++;
		if(current_len >= current_size-10){
			current_size*=2;
			message = (char*)realloc(message, current_size * sizeof(char));
		}
		message[strlen(message)] = ch;
	}

	message[strlen(message)] = '\0';
	printf("1: %s\n",message);
	send(conn->sock, message, current_size, 0);
	printf("2: %s\n",message);

	free(buffer);
	free(message);
	fclose(fp);

	/* close socket and clean up */
	close(conn->sock);
	free(conn);
	pthread_exit(0);
}

int main(int argc, char ** argv)
{
	int sock = -1;
	struct sockaddr_in address;
	int port;
	char host_address[80];
	connection_t * connection;
	pthread_t thread;

	/* check for command line arguments */
	if (argc != 3)
	{
		fprintf(stderr, "usage: %s address port\n", argv[0]);
		return -1;
	}

	/* obtain port number */
	if (sscanf(argv[1], "%s", &host_address) <= 0)
	{
		fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
		return -2;
	}

	if (sscanf(argv[2], "%d", &port) <= 0)
	{
		fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
		return -2;
	}

	/* create socket */
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock <= 0)
	{
		fprintf(stderr, "%s: error: cannot create socket\n", argv[0]);
		return -3;
	}

	/* bind socket to port */
	address.sin_family = AF_INET;
	//address.sin_addr.s_addr = INADDR_ANY;
	inet_pton(AF_INET, host_address, &address.sin_addr);
	address.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
	{
		fprintf(stderr, "%s: error: cannot bind socket to port %d\n", argv[0], port);
		return -4;
	}

	/* listen on port */
	if (listen(sock, 5) < 0)
	{
		fprintf(stderr, "%s: error: cannot listen on port\n", argv[0]);
		return -5;
	}

	printf("%s: ready and listening\n", argv[0]);
	
	while (1)
	{
		/* accept incoming connections */
		connection = (connection_t *)malloc(sizeof(connection_t));
		connection->sock = accept(sock, &connection->address, &connection->addr_len);
		if (connection->sock <= 0)
		{
			free(connection);
		}
		else
		{
			/* start a new thread but do not wait for it */
			pthread_create(&thread, 0, worker, (void *)connection);
			pthread_detach(thread);
		}
	}
	
	return 0;
}
