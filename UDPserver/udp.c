#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#include "../MoorControl/motor.h"

#define PORT 5000


int main()
{
    char buffer[100];
    char *message ="Hello";
    int listenfd;

    struct sockaddr_in servaddr, cliaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(listenfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server started at port: %d \n", PORT);

    while(1)
    {
        socklen_t len = sizeof(cliaddr);
        int n = recvfrom(listenfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&cliaddr,&len);
        buffer[n] = '\0';
        char *endptr;
        buffer[strcspn(buffer, "\r\n")] = 0;

        if(strcmp(buffer, "EXIT") == 0)
        {
            printf("EXITING \n");
            break;
        }

        double angle = strtod(buffer, &endptr);
        if(*endptr != '\0')
        {
            printf("Parsing error at: %s \n", endptr);
        }
        else
        {
            rotate(angle);
        }
        printf("came message %s \n", buffer);
    }
    close(listenfd);
    return 0;
}