/*
** listener.c -- a datagram sockets "server" demo
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
#include <sys/wait.h>
#include <signal.h>

// PORT FOR SENDERS
#define SENDPORT "4950" // the port users will be connecting to

// PORT FOR RECEIVERS
#define REVPORT "3490"
#define BACKLOG 10 // how many pending connections queue will hold
void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

	errno = saved_errno;
}

#define MAXBUFLEN 100

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(void)
{
	int sendsockfd;
	struct addrinfo sendhints, *sendservinfo, *sendp;
	int rv;									//common
	int numbytes;							//num of bytes received from sender == number of bytes sent
	struct sockaddr_storage sendtheir_addr; // connector's address information
	char buf[MAXBUFLEN];					// string received from sender == string sent to receiver
	socklen_t sendaddr_len;					// size of sendtheir_addr (connector's address information)
	char sends[INET6_ADDRSTRLEN];
	// Sometimes you don’t want to look at a pile of binary numbers when
	//  looking at an IP address. You want it in a nice printable form,
	//   like 192.0.2.180, or 2001:db8:8714:3a90::12.

	// setup to connect to senders
	memset(&sendhints, 0, sizeof sendhints);
	sendhints.ai_family = AF_INET6; // set to AF_INET to use IPv4
	sendhints.ai_socktype = SOCK_DGRAM;
	sendhints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, SENDPORT, &sendhints, &sendservinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for (sendp = sendservinfo; sendp != NULL; sendp = sendp->ai_next)
	{
		if ((sendsockfd = socket(sendp->ai_family, sendp->ai_socktype,
								 sendp->ai_protocol)) == -1)
		{
			perror("server-talker: socket");
			continue;
		}

		if (bind(sendsockfd, sendp->ai_addr, sendp->ai_addrlen) == -1)
		{
			close(sendsockfd);
			perror("server-talker: bind");
			continue;
		}

		break;
	}

	if (sendp == NULL)
	{
		fprintf(stderr, "server-talker: failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(sendservinfo);


	/// setup for connecting to receivers
	int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, REVPORT, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
					   sizeof(int)) == -1)
		{
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)
	{
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1)
	{
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
	{
		perror("sigaction");
		exit(1);
	}

	printf("server-receiver: waiting for connections...\n");

	printf("server-talker: waiting to recvfrom...\n");

	sendaddr_len = sizeof sendtheir_addr;

	sin_size = sizeof their_addr;
	new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
	if (new_fd == -1)
	{
		perror("accept");
		// continue;
		return -1;
	}

	inet_ntop(their_addr.ss_family,
			  get_in_addr((struct sockaddr *)&their_addr),
			  s, sizeof s);
	printf("server: got connection from %s\n", s);

	for(;;)
	{
		if ((numbytes = recvfrom(sendsockfd, buf, MAXBUFLEN - 1, 0,
								 (struct sockaddr *)&sendtheir_addr, &sendaddr_len)) == -1)
		{
			perror("recvfrom");
			exit(1);
		}

		printf("server: got packet from %s\n",
			   inet_ntop(sendtheir_addr.ss_family,
						 get_in_addr((struct sockaddr *)&sendtheir_addr),
						 sends, sizeof sends));
		printf("server: packet is %d bytes long\n", numbytes);
		buf[numbytes] = '\0';
		printf("server: packet contains \"%s\"\n", buf);

		if (!fork())
		{ // this is the child process
			//close(sockfd); // child doesn't need the listener
			if (send(new_fd, buf, numbytes, 0) == -1)
				perror("send");
			close(new_fd);
			exit(0);
		}
		//close(new_fd); // parent doesn't need this
	}

	close(sendsockfd);

	return 0;
}