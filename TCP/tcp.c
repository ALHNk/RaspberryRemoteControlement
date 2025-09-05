#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "../MoorControl/motor.h"

int connfd, sockfd;

void handle_sigint(int sig)
{
    printf("Exitting \n");
    disconnectMotor();
    close(connfd);
    close(sockfd);
    exit(0);

}
#define PORT 5050
#define MAX 100
int main()
{ 
    signal(SIGINT, handle_sigint);
    if(connectMotor() != 0 )
    {
        return 1;
    }
    const char *SECRET = getenv("MOTOR_SECRET");
    if(!SECRET)
    {
        fprintf(stderr, "Secret is not set\n");
        exit(1);
    }
    char buffer[MAX];
    
    socklen_t len;

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

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
         perror("setsockopt(SO_REUSEADDR) failed");
         exit(EXIT_FAILURE);
     }

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
    while(1)
    {
        connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &len);
        if(connfd < 0)
        {
            perror("Accept failed");
            continue;
        }
        else{
            printf("Connected\n");
            // write(connfd, "connected\n", 11);
        }
        int n = read(connfd, buffer, sizeof(buffer));
        if( n <=0)
        {
            perror("Client failed to send secret\n");
            close(connfd);
            continue;
        }

        buffer[strcspn(buffer, "\r\n")] = 0;

        if(strcmp(buffer, SECRET) != 0)
        {
            printf("Wrong Secret\n%s\n", buffer);
            write(connfd, "unauthorized\n", 13);
            close(connfd);
            continue;
        }

        printf("Client accepted\n");
        uint32_t velosityStored = getProfileVelocity();
        char reply[50];
        snprintf(reply,sizeof(reply),"accepted\nvelocity:%d\n",velosityStored);
        write(connfd, reply, strlen(reply));

        while(1)
        {
            int n = read(connfd, buffer, sizeof(buffer));
            if(n == 0 )
            {
                printf("Client Disconnected ! \n");
                break;
            }
            else if(n < 0 )
            {
                perror("Error receiving from client \n");
            }        buffer[n] = '\0';
            buffer[strcspn(buffer, "\r\n")] = 0;

            if(strcmp(buffer, "EXIT") == 0)
            {
                printf("EXITING \n");
                break;
            }

            char *ptr = buffer;

            while(*ptr)
            {
                if(strncmp(ptr, "angle:", 6) == 0)
                {
                    ptr += 6;
                    double  angle = strtod(ptr, &ptr);
                    rotateMotor(angle);                
                }
                else if(strncmp(ptr, "velocity:", 9) == 0)
                {
                    ptr += 9;
                    double velocity = strtod(ptr, &ptr);
                    setProfileVelocity(velocity);
                }
                else 
                {
                    ptr++;
                }
            }
            printf("came message %s \n", buffer);
            memset(buffer, 0, MAX);
        }
        printf("Closing client \n");
        close(connfd);
    }

    
    close(sockfd);
    disconnectMotor();
    return 0;
}
