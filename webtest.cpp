#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <iostream>

#define SOCKET_ERROR        -1
#define BUFFER_SIZE         10000
#define HOST_NAME_SIZE      255
#define NSTD 3
#define SUPER_DEBUG

int main(int argc, char *argv[])
{
    int NSOCKETS;
    struct hostent *pHostInfo;   /* holds info about a machine */
    struct sockaddr_in Address;  /* Internet socket address stuct */
    long nHostAddress;
    char pBuffer[BUFFER_SIZE];
    unsigned nReadAmount;
    char strHostName[HOST_NAME_SIZE];
    int nHostPort;
    char *path;
    int debugFlag = 0;
    char option;

    if (argc < 3)
    {
        printf("\nUsage: webtest host-name host-port path [-d]\n");
        return 0;
    }
    else
    {
        while ((option = getopt(argc, argv, "d")) != -1)
        {
            switch (option)
            {
                case 'd':
                    debugFlag = 1;
                    break;
                default:
                    perror("Invalid flag. \nUsage: webtest host-name host-port path num-requests [-d]\n");
                    abort();
            }
        }
        strcpy(strHostName, argv[optind++]);
        std::string port = argv[optind++];
        path = argv[optind++];
        NSOCKETS = atoi(argv[optind]);
//        if (debugFlag)
//            printf("Host: %s, Port %s, Path: %s, Count: %d, debug: %d", strHostName, port, path, NSOCKETS, debugFlag);

    }

    int hSocket[NSOCKETS];                 /* handle to socket */
    double latency[NSOCKETS];
    struct timeval oldtime[NSOCKETS + NSTD];

    printf("\nMaking a socket");
    /* make a socket */
    for (int i = 0; i < NSOCKETS; i++)
    {
        hSocket[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (hSocket[i] == SOCKET_ERROR)
        {
            printf("\nCould not make a socket\n");
            return 0;
        }
    }

    /* get IP address from name */
    pHostInfo = gethostbyname(strHostName);
    /* copy address into long */
    memcpy(&nHostAddress, pHostInfo->h_addr, pHostInfo->h_length);

    /* fill address struct */
    Address.sin_addr.s_addr = nHostAddress;
    Address.sin_port = htons(nHostPort);
    Address.sin_family = AF_INET;

    int epollFD = epoll_create(1);
    // Send the requests and set up the epoll data
    for (int i = 0; i < NSOCKETS; i++)
    {
        /* connect to host */
        printf("Getting here");
        if (connect(hSocket[i], (struct sockaddr *) &Address, sizeof(Address))
            == SOCKET_ERROR)
        {
            printf("\nCould not connect to host\n");
            return 0;
        }
        char request[] = "GET /test.html HTTP/1.0\r\n\r\n";

        write(hSocket[i], request, strlen(request));
        // Keep track of the time when we sent the request
        gettimeofday(&oldtime[hSocket[i]], NULL);
        // Tell epoll that we want to know when this socket has data
        struct epoll_event event;
        event.data.fd = hSocket[i];
        event.events = EPOLLIN;
        int ret = epoll_ctl(epollFD, EPOLL_CTL_ADD, hSocket[i], &event);
        if (ret)
            perror("epoll_ctl");
    }

    double sum = 0.0;
    for (int i = 0; i < NSOCKETS; i++)
    {
        struct epoll_event event;
        int rval = epoll_wait(epollFD, &event, 1, -1);
        if (rval < 0)
            perror("epoll_wait");
        rval = read(event.data.fd, pBuffer, BUFFER_SIZE);
        struct timeval newtime;
        // Get the current time and subtract the starting time for this request.
        gettimeofday(&newtime, NULL);
        double usec = (newtime.tv_sec - oldtime[event.data.fd].tv_sec) * (double) 1000000 +
                      (newtime.tv_usec - oldtime[event.data.fd].tv_usec);
        latency[i] = usec / 1000000;
        if (debugFlag)
            printf("Request: %d, Time: %f\n", i + 1, latency[i]);
        sum += latency[i];
        printf("\nClosing socket\n");
        /* close socket */
        if (close(hSocket[i]) == SOCKET_ERROR)
        {
            printf("\nCould not close socket\n");
            return 0;
        }
        // Take this one out of epoll
        epoll_ctl(epollFD, EPOLL_CTL_DEL, event.data.fd, &event);
    }
    double average = sum / NSOCKETS;
    double standardDeviation = 0.0;
    // Now close the sockets
    for (int i = 0; i < NSOCKETS; i++)
    {
        double difference = latency[i] - average;
        standardDeviation += (difference * difference);

    }
    printf("Average: %f, Standard Deviation: %f", average, standardDeviation);
}