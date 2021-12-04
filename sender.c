/*
** talker.c -- a datagram "client" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVERPORT "4950" // the port users will be connecting to

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;

	if (argc != 2)
	{
		fprintf(stderr, "usage: talker hostname\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and make a socket
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("talker: socket");
			continue;
		}

		break;
	}

	if (p == NULL)
	{
		fprintf(stderr, "talker: failed to create socket\n");
		return 2;
	}

	for(;;)
	{
		printf("Please enter a line:\n");
		char *line = NULL;
		size_t len = 0;
		ssize_t lineSize = 0;
		lineSize = getline(&line, &len, stdin); //getline does malloc automatically on line
		line[lineSize - 1] = 0; // null terminated
		printf("You entered '%s', which has %zu chars.\n", line, lineSize - 1);

		// concatenate the strings to be in the form "hostname, port: message"
		char *str = malloc(sizeof(char) * (strlen(argv[1]) + strlen(SERVERPORT) + lineSize + 5));
		strncat(str, argv[1], strlen(argv[1]));
		strncat(str, ", ", 2);
		strncat(str, SERVERPORT, strlen(SERVERPORT));
		strncat(str, ": ", 2);
		strncat(str, line, lineSize);
		str[strlen(str)] = 0; // null-terminated

		if ((numbytes = sendto(sockfd, str, strlen(str), 0, p->ai_addr, p->ai_addrlen)) == -1)
		{
			perror("talker: sendto");
			exit(1);
		}
		printf("talker: sent %d bytes to %s\n", numbytes, str);

		free(line);
		free(str);
	}

	freeaddrinfo(servinfo);
	close(sockfd);

	return 0;
}