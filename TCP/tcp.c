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
#include <stdatomic.h>
#include <pthread.h>


#include "../MoorControl/motor.h"
#include "logger.h"

#define MOTOR_TYPE 1                 // if MX it is 0 if Pro it is 1
#define ALL_MOTORS 4                 //change it here and MOTORS_QUANTITY in ../MoorController/motor.c

int connfd, sockfd;
#define PORT 5050
#define SPEED_SEND_PORT 5051
#define MAX 100
// #define TRUE 1
// #define FALSE 0

typedef enum {false, true} bool;

double speedAngel = 0;
double globalSpeed = 0;

bool torquedoff = 1;
bool isSan = 0;

atomic_bool is_running = ATOMIC_VAR_INIT(0);


int disconnect_all_motors();
void handle_sigint(int sig)
{
    log_msg("Exitting");
    disconnect_all_motors();    
    close(connfd);
    close(sockfd);
    closeMotorPort();
    log_close();
    exit(0);

}

// void emergancy_stop()
// {
//     setGoalSpeed(0, 2, MOTOR_TYPE);
//     setGoalSpeed(0, 3, MOTOR_TYPE);
//     disconnect_all_motors();
//     closeMotorPort();
//     printf("Emergency Stop");
//     exit(1);
// }
void emergancy_stop()
{
    setGoalSpeed(0, 2, MOTOR_TYPE);
    setGoalSpeed(0, 3, MOTOR_TYPE);
    disconnect_all_motors();
    printf("Emergency Stop");
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
    if (!isSan) return;

    double k = speedAngel / 100.0;
    if (k > 1) k = 1;
    if (k < -1) k = -1;

    double left  = speed * (1 - k);
    double right = speed * (1 + k);

    setGoalSpeed(left,  motor_id,     MOTOR_TYPE);
    setGoalSpeed(-right, motor_id + 1, MOTOR_TYPE);
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

void* control_threat(void* arg)
{
    bool isLocked = false;
    const char *SECRET = getenv("MOTOR_SECRET");
    if(!SECRET)
    {
        log_msg("Secret is not set");
        log_close();
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

    log_msg("TCP server started at port: %d", PORT);
    atomic_store(&is_running, 1);

    if((listen(sockfd,5)) != 0)
    {
        perror("Listen Failed\n");
        exit(EXIT_FAILURE);
    }
    else {
        log_msg("Server started listening");
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
            log_msg("Connected");
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
            log_msg("Wrong Secret: %s", buffer);
            write(connfd, "unauthorized\n", 13);
            close(connfd);
            continue;
        }

        log_msg("Client accepted");
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
                log_msg("Client Disconnected");
                break;
            }
            else if(n < 0 )
            {
                perror("Error receiving from client \n");
            }        buffer[n] = '\0';
            buffer[strcspn(buffer, "\r\n")] = 0;

            if(strcmp(buffer, "EXIT") == 0)
            {
                log_msg("EXITING");
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
                    if(strncmp(ptr, "ESTOP", 5) == 0)
                    {
                        emergancy_stop();
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

                        log_msg("torque started");
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
                                log_msg("Torqued on");
                                write(connfd, "on\n", strlen("on\n"));
                            }
                        }
                        else if(torque == 0)
                        {
                            disconnect_all_motors();
                            log_msg("Torqued off");
                            write(connfd, "off\n", strlen("off\n"));
                        }
                        else 
                        {
                            log_msg("Invalid torque");
                        }
                    }
                    else if(strncmp(ptr, "prot:", 5) == 0)
                    {
                        ptr += 5;
                        isSan = 0;
                        double prot = strtod(ptr, &ptr);
                        prot = prot /45.0f;
                        float local_speed = prot * 5.0f;
                        setGoalSpeed(-local_speed, motor_id, MOTOR_TYPE);
                        setGoalSpeed(-local_speed, motor_id+1, MOTOR_TYPE);
                    }
                    else if(strncmp(ptr, "twodegree:", 10) == 0)
                    {
                        ptr += 10;
                        double td = strtod(ptr, &ptr);
                        double angle1 = -177.78 + td * 287.41;
                        double angle2 =  177.78 - td * 287.41;
			            log_msg("td: %f, ang1: %f, ang2: %f", td, angle1, angle2);
                        rotateMotor(angle1, motor_id, MOTOR_TYPE);
                        rotateMotor(angle2, motor_id + 1, MOTOR_TYPE);
                    }
                    else 
                    {
                        ptr++;
                    }

                    //double v1 = getGoalSpeed(2, MOTOR_TYPE);
                    //double v2 = getGoalSpeed(3, MOTOR_TYPE);
                }
                log_msg("came message %s", line);
                line = strtok(NULL, "\n");
            }            
            memset(buffer, 0, MAX);
        }
        log_msg("Closing client");
        close(connfd);
        disconnect_all_motors();
    }
    atomic_store(&is_running, 0);
    close(sockfd);
    disconnect_all_motors();
    return NULL;  
}

void* send_speed_threat(void* arg)
{
    int send_speed_fd, speed_client_fd;

    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;

    log_msg("Starting send speed thread on port %d", SPEED_SEND_PORT);

    send_speed_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(send_speed_fd < 0)
    {
        perror("Cannot open send speed socket");
        return NULL;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SPEED_SEND_PORT);
    servaddr.sin_family = AF_INET;

    int opt = 1;
    if(setsockopt(send_speed_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("SEND SPEED setsockopt failed");
        close(send_speed_fd);
        return NULL;
    }

    if(bind(send_speed_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("SEND SPEED BIND FAILED");
        close(send_speed_fd);
        return NULL;
    }

    if(listen(send_speed_fd, 5) != 0)
    {
        perror("SEND SPEED LISTEN FAILED");
        close(send_speed_fd);
        return NULL;
    }

    log_msg("SEND SPEED started listening");

    while(1)
    {
        len = sizeof(cliaddr);  
        int send_speed_client_fd = accept(send_speed_fd, (struct sockaddr*)&cliaddr, &len);  

        if(send_speed_client_fd < 0 )
        {
            if(atomic_load(&is_running))  // Only log if still running
                perror("Error accepting speed client");
            continue;
        }
        log_msg("SEND SPEED CLIENT ACCEPTED");
        while(atomic_load(&is_running))
        {
            double goal_speed_1 = getGoalSpeed(2, MOTOR_TYPE);
            double goal_speed_2 = getGoalSpeed(3, MOTOR_TYPE);

            char buffer[256];
            // FIX 2: Add the actual values to snprintf
            snprintf(buffer, sizeof(buffer), 
                    "{\"motor2\":%.3f,\"motor3\":%.3f,\"timestamp\":%ld}\n",
                    goal_speed_1, goal_speed_2, time(NULL));

            ssize_t sent = write(send_speed_client_fd, buffer, strlen(buffer));
            if(sent <= 0) {
                log_msg("Send speed client disconnected");
                break;
            }
            
            usleep(100000);  // 100ms = 10Hz
        }

        close(send_speed_client_fd);
    }

    close(send_speed_fd);
    return NULL;
}

int main()
{ 
    signal(SIGSEGV, emergancy_stop);
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, emergancy_stop);


    log_init("server.log");

    openMotorPort();
    log_msg("OpenedMotors");

    // connect_to_all_motors();
    // rotateMotor(-177.77, 0, MOTOR_TYPE);
    // rotateMotor(178.53, 1, MOTOR_TYPE);
    // disconnect_all_motors();

    pthread_t control_td, speed_send_td;

    // FIX 1: Add & to pthread_create and use correct variable name
    if(pthread_create(&control_td, NULL, control_threat, NULL) != 0)
    {
        perror("Creating a control thread failed");
        log_close();
        exit(1);
    }
    //if(pthread_create(&speed_send_td, NULL, send_speed_threat, NULL) != 0)
    //{
    //    perror("Creating a send speed thread failed");
    //    log_close();
    //    exit(1);
    //}

    log_msg("Both threads are created");

    pthread_join(control_td, NULL);
   // pthread_join(speed_send_td, NULL);  

    disconnect_all_motors();

    closeMotorPort();

    log_close();
    return 0;
}
