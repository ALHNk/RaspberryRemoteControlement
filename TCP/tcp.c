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
#include <time.h>

#include "../MoorControl/motor.h"

#define MOTOR_TYPE 1                 // if MX it is 0 if Pro it is 1
#define ALL_MOTORS 4                 //change it here and MOTORS_QUANTITY in ../MoorController/motor.c

int connfd, sockfd;
#define PORT 5050
#define MAX 100
// #define TRUE 1
// #define FALSE 0

typedef enum {false, true} bool;

double speedAngel = 0;
double globalSpeed = 0;

bool torquedoff = 1;
bool isSan = 0;
int disconnect_all_motors();
void handle_sigint(int sig)
{
    printf("Exitting \n");
    disconnect_all_motors();    
    close(connfd);
    close(sockfd);
    closeMotorPort();
    exit(0);

}

void generate_secret(char *buf, int length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int charsetSize = sizeof(charset) - 1;
    srand(time(NULL));
    for (int i = 0; i < length; i++) {
        buf[i] = charset[rand() % charsetSize];
    }
    buf[length] = '\0';
}

void change_speed(double speed, int motor_id)
{
    if(isSan == 0)
    {
        return;
    }
    if(speedAngel < 0)
        {
            setGoalSpeed(-speed, motor_id + 1, MOTOR_TYPE);
            speed = speed - speed*speedAngel/100;
            setGoalSpeed(speed, motor_id, MOTOR_TYPE);
        }
    else
        {
            setGoalSpeed(-speed, motor_id, MOTOR_TYPE);
            speed = speed - speed*speedAngel/100;
            setGoalSpeed(speed, motor_id + 1, MOTOR_TYPE);
        }
}

int connect_to_all_motors()
{
    //if(torquedoff == 0)
    //{
     //   return 0;
   // }
    for(int i = 0; i < ALL_MOTORS; i++)
    {
        if(connectMotor(i, MOTOR_TYPE) != 0 )
        {
            return 1;
        }
        torquedoff = 0;
    }
    return 0;
}

int disconnect_all_motors()
{
    //if(torquedoff == 1)
    //{
      //  return 0;
    //}
    for (int i = 0; i < ALL_MOTORS; i++)
    {
        disconnectMotor(i, MOTOR_TYPE);
        torquedoff = 1;
    }
    return 0;
}

int main()
{ 
    bool isLocked = false;
    signal(SIGINT, handle_sigint);
    openMotorPort();
    printf("OpenedMotors\n");

    // connect_to_all_motors();
    // rotateMotor(-177.77, 0, MOTOR_TYPE);
    // rotateMotor(178.53, 1, MOTOR_TYPE);
    // disconnect_all_motors();
        
    
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
        // uint32_t velosityStored = getProfileVelocity(0, MOTOR_TYPE);              // DO NOT FORGET ABPOUT THIS LINE IN THE FUTURE
        // char reply[50];
        // snprintf(reply,sizeof(reply),"accepted\nvelocity:%d\n",velosityStored);
        // write(connfd, reply, strlen(reply));

        double motor1velocity = getProfileVelocity(0, MOTOR_TYPE);
        double motor2velocity = getProfileVelocity(1, MOTOR_TYPE);
        double motor1position = getPosition(0, MOTOR_TYPE);
        double motor2position = getPosition(1, MOTOR_TYPE);
        double motor1limitlow = getLimitLow(0, MOTOR_TYPE);
        double motor1limitup  = getLimitUp(0, MOTOR_TYPE);
        double motor2limitlow = getLimitLow(1, MOTOR_TYPE);
        double motor2limitup  = getLimitUp(1, MOTOR_TYPE);
        
        char reply[512];
        snprintf(reply, sizeof(reply),
            "{"
            "\"status\":\"accepted\","
            "\"motor1velocity\":%f,"
            "\"motor2velocity\":%f,"
            "\"motor1position\":%f,"
            "\"motor2position\":%f,"
            "\"motor1limitlow\":%f,"
            "\"motor1limitup\":%f,"
            "\"motor2limitlow\":%f,"
            "\"motor2limitup\":%f"
            "}",
            motor1velocity,
            motor2velocity,
            motor1position,
            motor2position,
            motor1limitlow,
            motor1limitup,
            motor2limitlow,
            motor2limitup
        );

        write(connfd, reply, strlen(reply));

        char lockSecret[33];
        

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

            char *line = strtok(buffer, "\n");
            while(line != NULL)
            {
                char *ptr = line;
                uint8_t motor_id = 0;

                while(*ptr)
                {
                    if(strncmp(ptr, "lock:", 5) == 0)
                    { 
                        ptr += 5;
                        if(strtol(ptr, &ptr, 10) == 1)
                        {
                            // generate_secret(lockSecret, 32);
                            // write(connfd, lockSecret, sizeof(lockSecret));
                            for (int i = 0; i < ALL_MOTORS; i++)
                            {
                                setGoalSpeed(0, i, MOTOR_TYPE);
                            }
                            isLocked = true;
                            
                        }
                        else if(strtol(ptr, &ptr, 10) == 0)
                        {
                            // ptr += 1;
                            // if(strncmp(ptr, lockSecret, sizeof(lockSecret)))
                            // {
                            //     isLocked = false;
                            // }
                            isLocked = false;
                        }
                    }
                    if(strncmp(ptr, "motor:", 6) == 0)
                    {
                        ptr += 6;
                        motor_id = strtod(ptr, &ptr);
                    }
                    else if(strncmp(ptr, "angle:", 6) == 0)
                    {   
                        ptr += 6;
                        double  angle = strtod(ptr, &ptr);
                        if(angle > 180) angle -=360;
                        else if(angle < -180) angle +=360;
                        if(isLocked == false) rotateMotor(angle, motor_id, MOTOR_TYPE);                
                    }
                    else if(strncmp(ptr, "velocity:", 9) == 0)
                    {
                        ptr += 9;
                        double velocity = strtod(ptr, &ptr);
                        setProfileVelocity(velocity, motor_id, MOTOR_TYPE);
                    }
                    else if(strncmp(ptr, "san:", 4) == 0)
                    {
                        ptr += 4;
                        double san = strtod(ptr, &ptr);
                        speedAngel = san;
                        change_speed(globalSpeed, motor_id);      
                    }
                    else if(strncmp(ptr, "speed:", 6) == 0)
                    {
                        ptr += 6;
                        isSan = 1;
                        double speed = strtod(ptr, &ptr);
                        
                        if(isLocked == false) 
                        {
                            globalSpeed = speed;
                            change_speed(globalSpeed, motor_id);                        
                        }
                    }
                    else if (strncmp(ptr, "torque:", 7) == 0)
                    {
                        ptr += 7;

			            printf("torque started\n");
                        //char *end;
                        long torque = strtol(ptr, &ptr, 10);

                        //if (end == ptr) {
                          //  printf("Invalid torque value\n");
                            //return 1;
                        //}

                        if (torque == 1)
                        {
                            if (connect_to_all_motors() == 0)
                            {
                                printf("Torqued on\n");
                                write(connfd, "on\n", strlen("on\n"));
                            }
                        }
                        else if(torque == 0)
                        {
                            disconnect_all_motors();
                            printf("Torqued off\n");
                            write(connfd, "off\n", strlen("off\n"));
                        }
                        else 
                        {
                            printf("Invalid torque\n");
                        }
                    }
                    else if(strncmp(ptr, "prot:", 5) == 0)
                    {
                        ptr += 5;
                        isSan = 0;
                        double prot = strtod(ptr, &ptr);
                        prot = prot /45.0f;
                        float local_speed = prot * 5.0f;
                        setGoalSpeed(local_speed, motor_id, MOTOR_TYPE);
                        setGoalSpeed(local_speed, motor_id+1, MOTOR_TYPE);
                    }
                    else if(strncmp(ptr, "twodegree:", 10) == 0)
                    {
                        ptr += 10;
                        double td = strtod(ptr, &ptr);
                        double angle1 = -177.78 + td * 287.41;
                        double angle2 =  177.78 - td * 287.41;
			            printf("td: %f, ang1: %f, ang2: %f", td, angle1, angle2);
                        rotateMotor(angle1, motor_id, MOTOR_TYPE);
                        rotateMotor(angle2, motor_id + 1, MOTOR_TYPE);
                    }
                    else 
                    {
                        ptr++;
                    }
                }
                printf("came message %s \n", line);
                line = strtok(NULL, "\n");
            }            
            memset(buffer, 0, MAX);
        }
        printf("Closing client \n");
        close(connfd);
        disconnect_all_motors();
    }

    
    close(sockfd);
    disconnect_all_motors();

    closeMotorPort();
    return 0;
}
