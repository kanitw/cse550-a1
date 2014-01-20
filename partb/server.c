#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <unistd.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/poll.h>

#define MAXDATASIZE 100000

typedef struct
{
	int sock;
	char *buffer;
	//struct sockaddr address;
	//int addr_len;
} connection_t;

void * worker(void * ptr)
{
	//char *buffer;
	//buffer = (char *)malloc((MAXDATASIZE)*sizeof(char));
	char ch;
	char *message;
	message = (char *)malloc((MAXDATASIZE)*sizeof(char));
	int current_len = 0;
	int current_size = MAXDATASIZE;
	connection_t * conn;
	long addr = 0;
	//sleep 5 seconds for testing
	sleep(5);

	if (!ptr) pthread_exit(0); 
	conn = (connection_t *)ptr;

	/* read length of message */
	//read(conn->sock, &len, sizeof(int));

	//buffer[len] = 0;

	/* waiting for the network input */
	//recv(conn->sock, buffer, MAXDATASIZE,0);
	//conn->buffer[strlen(conn->buffer)-1] = '\0';
	//printf("%d\n",strlen(conn->buffer));
	
	FILE *fp;
	fp=fopen(conn->buffer, "r");
	if(fp){
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

		fclose(fp);
		free(conn);
		return (void *) message;
	}else{
		free(conn);
		return (void *) NULL;
	}

	//pthread_exit(0);
}

/**
 * Set a socket to non-blocking mode.
 */
int setnonblock(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		return flags;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -1;

	return 0;
}

int main(int argc, char ** argv)
{
	int sock = -1;
	int new_sock;
	struct sockaddr_in address;
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	int port;
	char host_address[80];
	connection_t * connection;
	struct pollfd ufds[1];
	char buf1[MAXDATASIZE];
	int rv;
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

	/* Set the socket to non-blocking. */
	if (setnonblock(sock) < 0)
		err(1, "failed to set server socket to non-blocking");




	ufds[0].fd = sock;
	ufds[0].events = POLLIN; // check for normal or out-of-band
	while(1){
	 	rv = poll(ufds, 1, -1);
		if (rv == -1) {
		    perror("poll"); // error occurred in poll()
		} 
		else if (rv == 0) {
		    printf("Timeout occurred!  No data.\n");
		} else {
		    // check for events on sock:
		    if (ufds[0].revents & POLLIN) {
		    	
		    	addr_size = sizeof their_addr;
		    	new_sock = accept(sock,(struct sockaddr *)&their_addr, &addr_size);
		        recv(new_sock, buf1, sizeof buf1, 0); // receive normal data
		        buf1[strlen(buf1)-1] = '\0';
		        printf( "%s\n", buf1);
		        
		        connection = (connection_t *)malloc(sizeof(connection_t));
				connection->sock = new_sock;
				connection->buffer = buf1;
				if (connection->sock <= 0)
				{
					free(connection);
				}
				else
				{
					pthread_t thread;
					void *file_content;
					char *file_content_str;
					pthread_create(&thread, 0, worker, (void *)connection);
					pthread_join(thread, &file_content);
					if(file_content){
						file_content_str = (char*)file_content;
						send(new_sock, file_content_str, MAXDATASIZE, MSG_NOSIGNAL);
					}
					close(new_sock);
				}

		        
		    }

		}
	}
	printf("%s\n", "end somehow");

	return 0;
}
