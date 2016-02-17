#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "CS360Utils.h"

#define SOCKET_ERROR        -1
#define BUFFER_SIZE         10000
#define HOST_NAME_SIZE      255
#define MAX_GET             1000

int main(int argc, char *argv[])
{
    int option;
    int handleToSocket;                 /* handle to socket */
    struct hostent *hostInfo;   /* holds info about a machine */
    struct sockaddr_in Address;  /* Internet socket address stuct */
    long hostAddress;
    char pBuffer[BUFFER_SIZE];
    unsigned nReadAmount;
    char strHostName[HOST_NAME_SIZE];
    int portNumber;
    int count = 1;
    int countFlag = 0;
    int debugFlag = 0;
    int downloadsSucceeded = 0;
    int downloadsFailed = 0;
    char *path;
    char contentType[MAX_MSG_SIZE];

    if (argc < 4 || argc > 6)
    {
        printf("\nUsage: download host-name host-port path [-c times-to-download | -d]\n");
        return 0;
    }
    else
    {
        while ((option = getopt(argc, argv, "dc:")) != -1)
        {
            switch (option)
            {
                case 'c':
                    count = atoi(optarg);
                    countFlag = 1;
                    break;
                case 'd':
                    debugFlag = 1;
                    break;
                default:
                    perror("Invalid flag. \nUsage: download host-name host-port path [-c times-to-download | -d]\n");
                    abort();
            }
        }
        if (argc - optind != 3)
            perror("\nUsage: download host-name host-port path [-c times-to-download | -d]\n");

        if (debugFlag)
            printf("count = %d, printHeaders = %d\n", count, debugFlag);

        strcpy(strHostName, argv[optind++]);
        std::string port = argv[optind++];
        if (port.find_first_not_of("0123456789") != string::npos)
        {
            perror("Port must only contain numbers.\n");
            return -1;
        }
        else
        {
            portNumber = atoi(&port[0]);
        }

        path = argv[optind];
    }

    while (downloadsFailed + downloadsSucceeded < count)
    {
        if (debugFlag)
            printf("\nMaking a socket.\n");
        /* make a socket */
        handleToSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (handleToSocket == SOCKET_ERROR)
        {
            printf("\nCould not make a socket.\n");
            //return 0;
        }

        /* get IP address from name */
        hostInfo = gethostbyname(strHostName);
        if (hostInfo == NULL)
        {
            perror("\nCouldn't connect to host.\n");
            return -1;
        }
        /* copy address into long */
        memcpy(&hostAddress, hostInfo->h_addr, hostInfo->h_length);

        /* fill address struct */
        Address.sin_addr.s_addr = hostAddress;
        Address.sin_port = htons(portNumber);
        Address.sin_family = AF_INET;
		
		int epollFD = epoll_create(1);
		// Send the requests and set up the epoll data
		for(int i = 0; i < NSOCKETS; i++)
		{
			/* connect to host */
			if (connect(handleToSocket, (struct sockaddr *) &Address, sizeof(Address)) == SOCKET_ERROR)
			{
				printf("\nCould not connect to host.\n");
				//return 0;
			}

			// Create HTTP Message
			char *message = (char *) malloc(MAX_GET);
			sprintf(message, "GET %s HTTP/1.1\r\nHost:%s:%d\r\n\r\n", path, strHostName, portNumber);

			// Send HTTP on the socket
			if (debugFlag)
				printf("Request: %s\n", message);
			write(handleToSocket, message, strlen(message));
			

			// First read the status line
			char *startLine = GetLine(handleToSocket);
			if (debugFlag)
				printf("Status: %s.\n\n", startLine);
			char status[2];
			status[0] = startLine[strlen(startLine) - 2];
			status[1] = startLine[strlen(startLine) - 1];
			if (status[0] == 'n' && status[1] == 'd')
			{
				perror("Invalid context, couldn't reach address.");
				return -1;
			}

			// Read the header lines
			vector<char *> headerLines;
			headerLines.clear();
			GetHeaderLines(headerLines, handleToSocket, false);
			if (debugFlag)
			{
				// Now print them out
				for (int i = 0; i < headerLines.size(); i++)
				{
					printf("%s\n", headerLines[i]);
					if (strstr(headerLines[i], "Content-Type"))
					{
						sscanf(headerLines[i], "Content-Type: %s", contentType);
					}
				}
				printf("\n=======================\n");
				printf("Headers are finished, now read the file.\n");
				printf("Content Type is %s\n", contentType);
				printf("=======================\n\n");
			}

			// Now read and print the rest of the file
			if (!countFlag)
			{
				int bytesRead;
				try
				{
					while ((bytesRead = read(handleToSocket, pBuffer, MAX_MSG_SIZE)) > 0)
					{
						write(1, pBuffer, (unsigned int) bytesRead);
					}
				}
				catch (...)
				{
					perror("Threw exception while reading socket.");
				}
			}
			//Tell epoll that we want to know when this socket has data
			struct epoll_event event;
			event.data.fd = hSocket[i];
			event.events = EPOLLIN;
			int ret = epoll_ctl(epollFD,EPOLL_CTL_ADD,hSocket[i],&event);
			if(ret)
				perror ("epoll_ctl");
			gettimeofday(&oldTime[event.data.fd],NULL);
		}
		for (int i = 0; i < NSOCKETS; i++)
		{
			struct epoll_event event;
			int rval = epoll_wait(epollFD,&event,1,-1);
			if(rval < 0)
				perror("epoll_wait");
			read(event.data.fd,pBuffer,BUFFER_SIZE);
			printf("got from %d\n",event.data.fd);
			struct timeval newTime; 
			gettimeofday(&newTime,NULL);
			double usec = (newTime.tv_sec - oldTime[event.data.fd].tv_sec)*(double)1000000+(newTime.tv_usec-oldTime[event.data.fd].tv_usec);
			std::cout << "Time "<<usec/1000000<<std::endl;
			/* close socket */
			if (debugFlag)
				printf("\nClosing socket.\n");
			if (close(handleToSocket) == SOCKET_ERROR)
			{
				printf("\nCould not close socket.\n");
			}
			/* determine success */
			if (status[0] == 'O' && status[1] == 'K')
				downloadsSucceeded++;
			else
				downloadsFailed++;
		}
	}
	if (countFlag)
		printf("=======================\n%d Downloaded Successfully, %d Downloads failed.\n", downloadsSucceeded,
			   downloadsFailed);
}