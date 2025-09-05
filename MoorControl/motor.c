#include "motor.h"
#include "dynamixel_sdk/dynamixel_sdk.h"
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>


//MX with protocol 2.0 ADDRESES
#define ADDR_MX_TORQUE_ENABLE           64                 
#define ADDR_MX_GOAL_POSITION           116
#define ADDR_MX_PRESENT_POSITION        132  
#define ADDR_MX_PROFILE_VELOCITY        112
#define ADDR_MX_PROFILE_ACCELERATION    108
#define ADDR_MX_PRESENT_PROFILE_VELOCITY 128
//END of MX ADDRESES

#define PROTOCOL_VERSION                2.0                 //  DONT FORGET TO CHANGE IF NEEDED

#define DXL_ID                          3                   
#define BAUDRATE                        1000000
#define DEVICENAME                      "/dev/ttyUSB0"      // Check which port is being used on your controller
     

#define TORQUE_ENABLE                   1                   // Value for enabling the torque
#define TORQUE_DISABLE                  0                   // Value for disabling the torque
#define DXL_MINIMUM_POSITION_VALUE      0                   // Dynamixel will rotate between this value
#define DXL_MAXIMUM_POSITION_VALUE      4095                // and this value (note that the Dynamixel would not move when the position value is out of movable range. Check e-manual about the range of the Dynamixel you use.)
#define DXL_MOVING_STATUS_THRESHOLD     20                  // Dynamixel moving status threshold


#define DXL_RESOLUTION 4095
#define DXL_ANGLE_LIMIT 360

int port_num;
// int motor_index;
int dxl_comm_result;
// int dxl_goal_position[2] = {DXL_MINIMUM_POSITION_VALUE, DXL_MAXIMUM_POSITION_VALUE};
uint8_t dxl_error = 0;
uint16_t dxl_present_position = 0;



void printTxRxResult(int protocol_version, int dxl_comm_result) {
    printf("[TxRxResult] Error code: %d (protocol %d)\n", dxl_comm_result, protocol_version);
}

void printRxPacketError(int protocol_version, uint8_t dxl_error) {
    printf("[RxPacketError] Error code: %d (protocol %d)\n", dxl_error, protocol_version);
}

int connectMotor()
{
    port_num = portHandler(DEVICENAME);
    packetHandler();

    dxl_comm_result = COMM_TX_FAIL;
    // motor_index = 0;

    

    if(openPort(port_num))
    {
        printf("Succeed to open! \n");
    }
    else
    {
        printf("Error to open port \n");
        return 1;
    }

    if(setBaudRate(port_num, BAUDRATE))
    {
        printf("Baudreagte set ! \n");
    }
    else
    {
        printf("Error to set baudrate \n");
        return 1;
    }
    
    write1ByteTxRx(port_num, PROTOCOL_VERSION, DXL_ID, ADDR_MX_TORQUE_ENABLE, TORQUE_ENABLE);
    if ((dxl_comm_result = getLastTxRxResult(port_num, PROTOCOL_VERSION)) != COMM_SUCCESS)
    {
      printTxRxResult(PROTOCOL_VERSION, dxl_comm_result);
    }
    else if ((dxl_error = getLastRxPacketError(port_num, PROTOCOL_VERSION)) != 0)
    {
      printRxPacketError(PROTOCOL_VERSION, dxl_error);
    }
    else
    {
      printf("Dynamixel has been successfully connected \n");
    }
    return 0;
}

void setProfileVelocity(double velocity)
{
    write4ByteTxRx(port_num, PROTOCOL_VERSION, DXL_ID, ADDR_MX_PROFILE_VELOCITY, velocity);
}

uint32_t getProfileVelocity()
{
    uint32_t velocity = read4ByteTxRx(port_num, PROTOCOL_VERSION, DXL_ID, ADDR_MX_PROFILE_VELOCITY);

    if ((dxl_comm_result = getLastTxRxResult(port_num, PROTOCOL_VERSION)) != COMM_SUCCESS)
    {
        printTxRxResult(PROTOCOL_VERSION, dxl_comm_result);
        return -1;
    }
    else if ((dxl_error = getLastRxPacketError(port_num, PROTOCOL_VERSION)) != 0)
    {
        printRxPacketError(PROTOCOL_VERSION, dxl_error);
        return -1;
    }
    return velocity;

}

void rotateMotor(double angle)
{
    int goal_position = (int)(angle * DXL_RESOLUTION / DXL_ANGLE_LIMIT );

    write4ByteTxRx(port_num, PROTOCOL_VERSION, DXL_ID, ADDR_MX_GOAL_POSITION, goal_position);

    if ((dxl_comm_result = getLastTxRxResult(port_num, PROTOCOL_VERSION)) != COMM_SUCCESS)
    {
        printTxRxResult(PROTOCOL_VERSION, dxl_comm_result);
    }
    else if ((dxl_error = getLastRxPacketError(port_num, PROTOCOL_VERSION)) != 0)
    {
        printRxPacketError(PROTOCOL_VERSION, dxl_error);
    }
    else
    {
        printf("Rotate to %f° (pos %d)\n", angle, goal_position);
    }

    // do {
    //     dxl_present_position = read4ByteTxRx(port_num, PROTOCOL_VERSION, DXL_ID, ADDR_MX_PRESENT_POSITION);
    //     if((dxl_comm_result = getLastTxRxResult(port_num, PROTOCOL_VERSION)) != COMM_SUCCESS)
    //     {
    //         printTxRxResult(PROTOCOL_VERSION, dxl_comm_result);
    //     }
    //     else if((dxl_error = getLastRxPacketError(port_num, PROTOCOL_VERSION)) != 0 )
    //     {
    //         printRxPacketError(PROTOCOL_VERSION, dxl_error);
    //     } 
    //     printf("[ID:%03d] Goal:%03d  Present:%03d\n", DXL_ID, goal_position, dxl_present_position);
    // } while (abs(goal_position - dxl_present_position) > DXL_MOVING_STATUS_THRESHOLD);

}
int disconnectMotor()
{
    write1ByteTxRx(port_num, PROTOCOL_VERSION, DXL_ID, ADDR_MX_TORQUE_ENABLE, TORQUE_DISABLE);
    if ((dxl_comm_result = getLastTxRxResult(port_num, PROTOCOL_VERSION)) != COMM_SUCCESS)
    {
      printTxRxResult(PROTOCOL_VERSION, dxl_comm_result);
    }
    else if ((dxl_error = getLastRxPacketError(port_num, PROTOCOL_VERSION)) != 0)
    {
      printRxPacketError(PROTOCOL_VERSION, dxl_error);
    }

    // Close port
    closePort(port_num);

    return 0;
}

// int connfd, sockfd;

// void handle_sigint(int sig)
// {
//     printf("Exitting \n");
//     disconnectMotor();
//     close(connfd);
//     close(sockfd);
//     exit(0);

// }

// #define PORT 5050
// #define MAX 100
// int main()
// { 
//     signal(SIGINT, handle_sigint);
//     if(connectMotor() != 0 )
//     {
//         return 1;
//     }
//     const char *SECRET = getenv("MOTOR_SECRET");
//     if(!SECRET)
//     {
//         fprintf(stderr, "Secret is not set\n");
//         exit(1);
//     }
//     char buffer[MAX];
    
//     socklen_t len;

//     struct sockaddr_in servaddr, cliaddr;
//     memset(&servaddr, 0, sizeof(servaddr));

//     sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if(sockfd < 0)
//     {
//         perror("Socket creation failed");
//         exit(EXIT_FAILURE);
//     }
//     servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
//     servaddr.sin_port = htons(PORT);
//     servaddr.sin_family = AF_INET;

//     int opt = 1;
//     if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
//          perror("setsockopt(SO_REUSEADDR) failed");
//          exit(EXIT_FAILURE);
//      }

//     if(bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
//     {
//         perror("bind failed");
//         close(sockfd);
//         exit(EXIT_FAILURE);
//     }

//     printf("TCP server started at port: %d \n", PORT);

//     if((listen(sockfd,5)) != 0)
//     {
//         perror("Listen Failed\n");
//         exit(EXIT_FAILURE);
//     }
//     else {
//         printf("Server started listenning \n");
//     }
//     len = sizeof(cliaddr);
//     while(1)
//     {
//         connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &len);
//         if(connfd < 0)
//         {
//             perror("Accept failed");
//             continue;
//         }
//         else{
//             printf("Connected\n");
//             // write(connfd, "connected\n", 11);
//         }
//         int n = read(connfd, buffer, sizeof(buffer));
//         if( n <=0)
//         {
//             perror("Client failed to send secret\n");
//             close(connfd);
//             continue;
//         }

//         buffer[strcspn(buffer, "\r\n")] = 0;

//         if(strcmp(buffer, SECRET) != 0)
//         {
//             printf("Wrong Secret\n%s\n", buffer);
//             write(connfd, "unauthorized\n", 13);
//             close(connfd);
//             continue;
//         }

//         printf("Client accepted\n");
//         uint32_t velosityStored = getProfileVelocity();
//         char reply[50];
//         snprintf(reply,sizeof(reply),"accepted\nvelocity:%d\n",velosityStored);
//         write(connfd, reply, strlen(reply));

//         while(1)
//         {
//             int n = read(connfd, buffer, sizeof(buffer));
//             if(n == 0 )
//             {
//                 printf("Client Disconnected ! \n");
//                 break;
//             }
//             else if(n < 0 )
//             {
//                 perror("Error receiving from client \n");
//             }        buffer[n] = '\0';
//             buffer[strcspn(buffer, "\r\n")] = 0;

//             if(strcmp(buffer, "EXIT") == 0)
//             {
//                 printf("EXITING \n");
//                 break;
//             }

//             char *ptr = buffer;

//             while(*ptr)
//             {
//                 if(strncmp(ptr, "angle:", 6) == 0)
//                 {
//                     ptr += 6;
//                     double  angle = strtod(ptr, &ptr);
//                     rotateMotor(angle);                
//                 }
//                 else if(strncmp(ptr, "velocity:", 9) == 0)
//                 {
//                     ptr += 9;
//                     double velocity = strtod(ptr, &ptr);
//                     setProfileVelocity(velocity);
//                 }
//                 else 
//                 {
//                     ptr++;
//                 }
//             }
//             printf("came message %s \n", buffer);
//             memset(buffer, 0, MAX);
//         }
//         printf("Closing client \n");
//         close(connfd);
//     }

    
//     close(sockfd);
//     disconnectMotor();
//     return 0;
// }

// int main ()
// {
//     signal(SIGINT, handle_sigint);
//     if(connect() != 0 )
//     {
//         return 1;
//     }
//     rotate(180);
//     disconnect();
//     return 0;
// }
