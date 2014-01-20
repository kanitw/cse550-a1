#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#define MAXDATASIZE 1000000

int main(int argc, char ** argv)
{
	int port;
	int sock = -1;
	char buffer[MAXDATASIZE];
	struct sockaddr_in address;
	struct hostent * host;
	int len;

	if (argc != 4)
	{
		printf("usage: %s hostname port file_name\n", argv[0]);
		return -1;
	}

	if (sscanf(argv[2], "%d", &port) <= 0)
	{
		fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
		return -2;
	}

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock <= 0)
	{
		fprintf(stderr, "%s: error: cannot create socket\n", argv[0]);
		return -3;
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	host = gethostbyname(argv[1]);
	if (!host)
	{
		fprintf(stderr, "%s: error: unknown host %s\n", argv[0], argv[1]);
		return -4;
	}
	memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);
	if (connect(sock, (struct sockaddr *)&address, sizeof(address)))
	{
		fprintf(stderr, "%s: error: cannot connect to host %s\n", argv[0], argv[1]);
		return -5;
	}

	len = strlen(argv[3]);
	//send(sock, &len, sizeof(int),0);
	char * message = argv[3];
	//message = (char *)malloc((len+1)*sizeof(char));
	//message = argv[3];	 
	message[len] = '\n';

	send(sock, message, len+1,0);

	recv(sock,buffer,MAXDATASIZE,0);

	printf("%s\n",buffer);

	close(sock);

	return 0;
}
