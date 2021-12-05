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
    hints.ai_family = AF_INET6;
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
    char* prefix;
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
        printf("server: got connection from %s\n", s);

        // x would be the port of the new sender
        int x = (int)(((struct sockaddr_in6 *)&their_addr)->sin6_port);
        int length = snprintf(NULL, 0, "%d", x); //length of x

        // make the prefix
        int total_length = length + sizeof(char) * strlen(s) + 4;
        char *prefix = (char *)calloc(total_length, sizeof(char));
        snprintf(prefix, total_length + 1, "%s, %d: ", s, x);
        printf("New Connection:%s\n", prefix);
        
        struct connection* con = (struct connection*) malloc(sizeof(struct connection));
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
    int new_fd = ((struct connection*) arg)->socket;
    char* prefix = ((struct connection*) arg)->prefix;
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

            //TO-DO: for-loop through the clients and send message
            // // send a message
            // if (!fork())
            // { // this is the child process
            //     close(oldsocketfd);
            //     if (send(revsocketfd, str, strlen(str), 0) == -1)
            //         perror("send");
            //     close(revsocketfd);
            //     exit(0);
            // }

            free(str);
            str = 0;
        }
    }
    free(prefix);
}