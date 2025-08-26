#include "motor.h"
#include "dynamixel_sdk/dynamixel_sdk.h"
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>


#define ADDR_MX_TORQUE_ENABLE           64                 
#define ADDR_MX_GOAL_POSITION           116
#define ADDR_MX_PRESENT_POSITION        132  


#define PROTOCOL_VERSION                2.0     //  DONT FORGET TO CHANGE IF NEEDED

#define DXL_ID                          3                   // Dynamixel ID: 1
#define BAUDRATE                        1000000
#define DEVICENAME                      "/dev/ttyUSB0"      // Check which port is being used on your controller
     

#define TORQUE_ENABLE                   1                   // Value for enabling the torque
#define TORQUE_DISABLE                  0                   // Value for disabling the torque
#define DXL_MINIMUM_POSITION_VALUE      0                // Dynamixel will rotate between this value
#define DXL_MAXIMUM_POSITION_VALUE      4095               // and this value (note that the Dynamixel would not move when the position value is out of movable range. Check e-manual about the range of the Dynamixel you use.)
#define DXL_MOVING_STATUS_THRESHOLD     20                  // Dynamixel moving status threshold


#define DXL_RESOLUTION 4095
#define DXL_ANGLE_LIMIT 360

int port_num;
int motor_index;
int dxl_comm_result;
int dxl_goal_position[2] = {DXL_MINIMUM_POSITION_VALUE, DXL_MAXIMUM_POSITION_VALUE};
uint8_t dxl_error = 0;
uint16_t dxl_present_position = 0;



void printTxRxResult(int protocol_version, int dxl_comm_result) {
    printf("[TxRxResult] Error code: %d (protocol %d)\n", dxl_comm_result, protocol_version);
}

void printRxPacketError(int protocol_version, uint8_t dxl_error) {
    printf("[RxPacketError] Error code: %d (protocol %d)\n", dxl_error, protocol_version);
}

int connect()
{
    port_num = portHandler(DEVICENAME);
    packetHandler();

    dxl_comm_result = COMM_TX_FAIL;
    motor_index = 0;

    

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


void rotate(double angle)
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

    do {
        dxl_present_position = read4ByteTxRx(port_num, PROTOCOL_VERSION, DXL_ID, ADDR_MX_PRESENT_POSITION);
        if((dxl_comm_result = getLastTxRxResult(port_num, PROTOCOL_VERSION)) != COMM_SUCCESS)
        {
            printTxRxResult(PROTOCOL_VERSION, dxl_comm_result);
        }
        else if((dxl_error = getLastRxPacketError(port_num, PROTOCOL_VERSION)) != 0 )
        {
            printRxPacketError(PROTOCOL_VERSION, dxl_error);
        } 
        printf("[ID:%03d] Goal:%03d  Present:%03d\n", DXL_ID, goal_position, dxl_present_position);
    } while (abs(goal_position - dxl_present_position) > DXL_MOVING_STATUS_THRESHOLD);

}
int disconnect()
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

void handle_sigint(int sig)
{
    printf("Exitting \n");
    disconnect();
    exit(0);

}

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
