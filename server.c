/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define SENDPORT "33490" // the port users will be connecting to
#define REVPORT "34950"  // the port users will be connecting to

#define BACKLOG 10 // how many pending connections queue will hold

#define MAXDATASIZE 100 // max number of bytes we can get at once
int num_rev = 0;        //the number of current receivers
pthread_mutex_t num_rev_mutex;

int num_received = 0;
pthread_cond_t num_received_cond;
pthread_mutex_t num_received_mutex;

char message[1000] = "";
pthread_cond_t message_cond;
pthread_mutex_t message_mutex;

int revListening();
void *acceptRev(void *arg);
void *sendToRev(void *arg);

int sendListening();
void *acceptSender(void *arg);
void *revFromSender(void *arg);

int revSocket(int sockfd); //just here to sends the message to a receiver, need to write sendToRev() to send to multiple receivers

void sigchld_handler(int s)
{
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// return the listen()ing socket descriptor for listening for new incoming sender connections
int sendListening()
{
    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    hints.ai_protocol = IPPROTO_TCP;

    if ((rv = getaddrinfo(NULL, SENDPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the socket we can
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

    printf("server: waiting for connections...\n");

    return sockfd;
}

struct connection
{
    int socket;
    char *prefix;
};

void *acceptSender(void *arg)
{
    int *addr = arg;
    int sockfd = *addr;
    int new_connection;

    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN]; // The IP address of the senders
    for (;;)
    {
        new_connection = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_connection == -1)
        {
            perror("accept");
            continue;
        }

        // s would be the IP address of the new sender
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got sender connection from %s\n", s);

        // x would be the port of the new sender
        int x = (int)(((struct sockaddr_in6 *)&their_addr)->sin6_port);
        int length = snprintf(NULL, 0, "%d", x); //length of x

        // make the prefix
        int total_length = length + sizeof(char) * strlen(s) + 4;
        char *prefix = (char *)calloc(total_length, sizeof(char));
        snprintf(prefix, total_length + 1, "%s, %d: ", s, x);
        printf("New Connection:%s\n", prefix);

        struct connection *con = (struct connection *)malloc(sizeof(struct connection));
        con->socket = new_connection;
        con->prefix = prefix;
        // Create a thread for this sender connection
        pthread_attr_t tattr;
        pthread_t sender_connection;
        int ret;

        /* initialized with default attributes */
        ret = pthread_attr_init(&tattr);
        ret = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
        ret = pthread_create(&sender_connection, &tattr, &revFromSender, con);
    }
}

void *revFromSender(void *arg)
{
    int new_fd = ((struct connection *)arg)->socket;
    char *prefix = ((struct connection *)arg)->prefix;
    printf("new socket: %d, new prefix:%s\n", new_fd, prefix);
    free(arg);

    int numbytes, total;
    char buf[MAXDATASIZE];

    while (1)
    { // main accept() loop
        if ((numbytes = recv(new_fd, buf, MAXDATASIZE - 1, 0)) == -1)
        {
            perror("recv");
            exit(1);
        }
        if (numbytes != 0)
        {
            buf[numbytes] = '\0';
            printf("server: received '%s'\n", buf);

            // concatenate the strings to be in the form "hostname, port: message"
            total = strlen(prefix) + numbytes + 4;
            char *str = (char *)calloc(total, sizeof(char));
            strncat(str, prefix, strlen(prefix));
            strncat(str, buf, numbytes);
            str[total - 1] = '\0';
            printf("You entered '%s', which has %d chars.%zu\n", str, total, strlen(str));

            // pthread_mutex_lock(&num_received_mutex);
            // while (num_received != num_rev)
            // {
            //     pthread_cond_wait(&num_received_cond, &num_received_mutex);
            // }
            // pthread_mutex_unlock(&num_received_mutex);

            pthread_mutex_lock(&message_mutex);
            strcpy(message, str);
            pthread_mutex_unlock(&message_mutex);
            pthread_cond_broadcast(&message_cond);

            free(str);
            str = 0;
        }
    }
    free(prefix);
}

int main(void)
{
    pthread_mutex_init(&num_received_mutex, NULL);
    pthread_cond_init(&num_received_cond, NULL);

    pthread_mutex_init(&message_mutex, NULL);
    pthread_cond_init(&message_cond, NULL);

    int sockfd = sendListening();

    //  THREAD TO ACCEPT NEW SENDER CONNECTIONS
    pthread_attr_t tattr;
    pthread_t accept_sender_t;
    int ret;
    // detached thread
    ret = pthread_attr_init(&tattr);
    ret = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&accept_sender_t, &tattr, &acceptSender, &sockfd);

    // initialize num_rev_mutex
    pthread_mutex_init(&num_rev_mutex, NULL);

    int revListenSocket = revListening();

    pthread_t accept_rev_t;
    // THREAD TO ACCEPT NEW RECEIVER CONNECTION
    ret = pthread_create(&accept_rev_t, &tattr, &acceptRev, &revListenSocket);

    for (;;)
    {
    }

    return 0;
}

int revListening()
{
    int sockfd; // listen on sock_fd
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    hints.ai_protocol = IPPROTO_TCP;

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
    return sockfd;
}

void *acceptRev(void *arg)
{
    int *addr = arg;
    int sockfd = *addr;
    int new_connection;

    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN]; // The IP address of the receiver
    for (;;)
    {
        new_connection = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_connection == -1)
        {
            perror("accept");
            continue;
        }
        // increment the number of receiver threads
        pthread_mutex_lock(&num_rev_mutex);
        num_rev += 1;
        printf("current num of receivers:%d\n", num_rev);
        pthread_mutex_unlock(&num_rev_mutex);

        // s would be the IP address of the new receiver
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got receiver connection from %s\n", s);

        // Create a thread for this receiver connection
        pthread_attr_t tattr;
        pthread_t rev_connection;
        int ret;

        int *list = (int *)calloc(2, sizeof(int));
        list[0] = sockfd;
        list[1] = new_connection;

        /* initialized with default attributes */
        ret = pthread_attr_init(&tattr);
        ret = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
        ret = pthread_create(&rev_connection, &tattr, &sendToRev, list);
    }
}

void *sendToRev(void *arg)
{
    int *list = (int *)arg;
    int sockfd = list[0];
    int newfd = list[1];
    char oldmessage[1000] = "";
    free(arg);

    for (;;)
    {
        pthread_mutex_lock(&message_mutex);
        while (strcmp("", message) == 0 || strcmp(message, oldmessage) == 0)
        {
            pthread_cond_wait(&message_cond, &message_mutex);
        }
        // send a message
        if (!fork())
        { // this is the child process
            close(sockfd);
            if (send(newfd, message, strlen(message), MSG_NOSIGNAL) == -1){
                pthread_mutex_lock(&num_rev_mutex);
                num_rev -= 1;
                pthread_mutex_unlock(&num_rev_mutex);
            }
            close(newfd);
            exit(0);
        }
        strcpy(oldmessage, message);

        // increment the number of received by 1
        pthread_mutex_lock(&num_received_mutex);
        num_received += 1;
        if (num_received == num_rev)
        {
            strcpy(message, "");
        }
        pthread_mutex_unlock(&num_received_mutex);
        //pthread_cond_broadcast(&num_received_cond);

        pthread_mutex_unlock(&message_mutex);
    }
    return 0;
}
