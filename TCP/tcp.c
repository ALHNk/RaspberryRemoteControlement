#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

// #include "../MoorControl/motor.c"

#define PORT 5050
#define MAX 100

int main()
{
    char buffer[MAX];
    char *message ="Hello";
    int connfd;
    int sockfd;
    int len;

    struct sockaddr_in servaddr, cliaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    if(bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("TCP server started at port: %d \n", PORT);

    if((listen(sockfd,5)) != 0)
    {
        perror("Listen Failed\n");
        exit(EXIT_FAILURE);
    }
    else {
        printf("Server started listenning \n");
    }
    len = sizeof(cliaddr);
    connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &len);
    if(connfd < 0)
    {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }
    else{
        printf("Accepted\n");
        write(connfd, "connected\n", 11);
    }

    while(1)
    {
        socklen_t len = sizeof(cliaddr);
        int n = read(sockfd, buffer, sizeof(buffer));
        if(n == 0 )
        {
            printf("Client Disconnected ! \n");
            break;
        }
        else if(n < 0 )
        {
            perror("Error receiving from client \n");
        }
        buffer[n] = '\0';
        // char *endptr;
        buffer[strcspn(buffer, "\r\n")] = 0;

        if(strcmp(buffer, "EXIT") == 0)
        {
            printf("EXITING \n");
            break;
        }

        // double angle = strtod(buffer, &endptr);
        // if(*endptr != '\0')
        // {
        //     printf("Parsing error at: %s \n", endptr);
        // }
        // else
        // {
        //     // rotate(angle);
        // }
        printf("came message %s \n", buffer);
        memset(buffer, 0, MAX);
    }
    close(connfd);
    close(sockfd);
    return 0;
}