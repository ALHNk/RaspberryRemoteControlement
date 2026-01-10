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
#define ADDR_MX_GOAL_VELOCITY              552   //CHANGE
//END of MX ADDRESES

//PRO H42P-020-S300-R ADDRESSES
#define ADDR_PRO_TORQUE_ENABLE            512
#define ADDR_PRO_GOAL_POSITION            564
#define ADDR_PRO_PRESENT_POSITION         580 
#define ADDR_PRO_PROFILE_VELOCITY         560
#define ADDR_PRO_PROFILE_ACCELERATION     556
#define ADDR_PRO_PRESENT_PROFILE_VELOCITY 576
#define ADDR_PRO_GOAL_VELOCITY            552
//END PRO

#define PROTOCOL_VERSION                2.0                 //  DONT FORGET TO CHANGE IF NEEDED

#define MOTORS_QUANTITY           4
#define PRO_FIRST_ID                       1
#define PRO_SECOND_ID                       2
#define PRO_THIRD_ID                       3
#define PRO_FOURTH_ID                       4
#define MX_DXL_ID                          5                   
#define BAUDRATE                        1000000

     

#define TORQUE_ENABLE                   1                   // Value for enabling the torque
#define TORQUE_DISABLE                  0                   // Value for disabling the torque
#define DXL_MINIMUM_POSITION_VALUE      0                   // Dynamixel will rotate between this value
#define DXL_MAXIMUM_POSITION_VALUE      4095                // and this value (note that the Dynamixel would not move when the position value is out of movable range. Check e-manual about the range of the Dynamixel you use.)
#define DXL_MOVING_STATUS_THRESHOLD     20                  // Dynamixel moving status threshold

//position limits for pro
#define PRO_MAXIMUM_POSITION_VALUE_FIRST     185000
#define PRO_MINIMUN_POSITION_VALUE_FIRST     -300000
#define PRO_MINIMUN_POSITION_VALUE_SECOND     -300000
#define PRO_MAXIMUM_POSITION_VALUE_SECOND     185000

#define DEVICENAME "/dev/ttyUSB0"  // Check which port is being used on your controller


#define DXL_RESOLUTION 4095
#define DXL_ANGLE_LIMIT 360

#define PRO_RESOLUTION 607500

#define MOTOR_MX        0
#define MOTOR_PRO       1

int port_num;
// int motor_index;
int dxl_comm_result;
// int dxl_goal_position[2] = {DXL_MINIMUM_POSITION_VALUE, DXL_MAXIMUM_POSITION_VALUE};
uint8_t dxl_error = 0;
uint16_t dxl_present_position = 0;

int MOTORS[MOTORS_QUANTITY] = {PRO_FIRST_ID, PRO_SECOND_ID , PRO_THIRD_ID, PRO_FOURTH_ID};



void printTxRxResult(int protocol_version, int dxl_comm_result) {
    printf("[TxRxResult] Error code: %d (protocol %d)\n", dxl_comm_result, protocol_version);
}

void printRxPacketError(int protocol_version, uint8_t dxl_error) {
    printf("[RxPacketError] Error code: %d (protocol %d)\n", dxl_error, protocol_version);
}

int openMotorPort()
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
    return 0;
}

int connectMotor(uint8_t motor_index, uint8_t motor_model)
{ 
    uint16_t addres = -1;
    switch(motor_model)
    {
        case MOTOR_MX:
            addres = ADDR_MX_TORQUE_ENABLE;
            break;
        case MOTOR_PRO:
            addres = ADDR_PRO_TORQUE_ENABLE;
            break;
        default:
            printf("NOT A MODEL ERRROR in connectMotor Function\n");
            return -1;
    }

    write1ByteTxRx(port_num, PROTOCOL_VERSION, MOTORS[motor_index], addres, TORQUE_ENABLE);
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

void setProfileVelocity(double velocity, uint8_t motor_index, uint8_t motor_model)
{
    int32_t addres = -1;
    switch(motor_model)
    {
        case MOTOR_MX:
            addres = ADDR_MX_PROFILE_VELOCITY;
            velocity = velocity/0.229;
            break;
        case MOTOR_PRO:
            addres = ADDR_PRO_PROFILE_VELOCITY;
            velocity *= 100;
            break;
        default:
            printf("NOT A MODEL\n");
            return ;
    }

    write4ByteTxRx(port_num, PROTOCOL_VERSION, MOTORS[motor_index], addres, (uint32_t) velocity);
}

void setGoalSpeed(double speed, uint8_t motor_index, uint8_t motor_model)
{
    int32_t addres = -1;
    switch(motor_model)
    {
        case MOTOR_MX:
            addres = ADDR_MX_GOAL_VELOCITY;
            break;
        case MOTOR_PRO:
            addres = ADDR_PRO_GOAL_VELOCITY;
            speed *= 100;
            break;
        default:
        printf("NOT A MODEL error in setGoalSpeed\n");
        return ;
    }

    write4ByteTxRx(port_num, PROTOCOL_VERSION, MOTORS[motor_index], addres, (int32_t) speed);
}

double getProfileVelocity(uint8_t motor_index, uint8_t motor_model)
{
    int32_t addres = -1;
    double multiplier = -1;
    switch(motor_model)
    {
        case MOTOR_MX:
            addres = ADDR_MX_PROFILE_VELOCITY;
            multiplier = 0.029;
            break;
        case MOTOR_PRO:
            addres = ADDR_PRO_PROFILE_VELOCITY;
            multiplier = 0.01;
            break;
        default:
        printf("NOT A MODEL merror in getProfileVelocity\n");
        return -1;
    }

    
    uint32_t velocity = read4ByteTxRx(port_num, PROTOCOL_VERSION, MOTORS[motor_index], addres);

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
    return velocity * multiplier;

}

double getLimitLow(uint8_t motor_index, uint8_t motor_model)
{
    double resolution = -1;
    switch(motor_model)
    {
        case MOTOR_MX:
            resolution = DXL_RESOLUTION;
            break;
        case MOTOR_PRO:
            resolution = PRO_RESOLUTION;
            break;
        default:
        printf("NOT A MODEL merror in getPosition\n");
        return -1;
    }

    if(motor_index == 0)
    {
        return PRO_MINIMUN_POSITION_VALUE_FIRST * DXL_ANGLE_LIMIT/resolution;
    }
    else if( motor_index == 1)
    {
        return PRO_MINIMUN_POSITION_VALUE_SECOND * DXL_ANGLE_LIMIT/resolution;
    }
    else
    {
        printf("NOT RIGHT INDEX OF MOTOR");
        return -1;
    }
}
double getLimitUp(uint8_t motor_index, uint8_t motor_model)
{
    double resolution = -1;
    switch(motor_model)
    {
        case MOTOR_MX:
            resolution = DXL_RESOLUTION;
            break;
        case MOTOR_PRO:
            resolution = PRO_RESOLUTION;
            break;
        default:
        printf("NOT A MODEL merror in getPosition\n");
        return -1;
    }

    if(motor_index == 0)
    {
        return PRO_MAXIMUM_POSITION_VALUE_FIRST * DXL_ANGLE_LIMIT/resolution;
    }
    else if( motor_index == 1)
    {
        return PRO_MAXIMUM_POSITION_VALUE_SECOND * DXL_ANGLE_LIMIT/resolution;
    }
    else
    {
        printf("NOT RIGHT INDEX OF MOTOR");
        return -1;
    }
}

double getPosition(uint8_t motor_index, uint8_t motor_model )
{
    int32_t addres = -1;
    double resolution = -1;
    switch(motor_model)
    {
        case MOTOR_MX:
            addres = ADDR_MX_PRESENT_POSITION;
            resolution = DXL_RESOLUTION;
            break;
        case MOTOR_PRO:
            addres = ADDR_PRO_PRESENT_POSITION;
            resolution = PRO_RESOLUTION;
            break;
        default:
        printf("NOT A MODEL merror in getPosition\n");
        return -1;
    }

    
    uint32_t position = read4ByteTxRx(port_num, PROTOCOL_VERSION, MOTORS[motor_index], addres);

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
    return position * DXL_ANGLE_LIMIT/resolution;
}


void rotateMotor(double angle, uint8_t motor_index, uint8_t motor_model)
{
    int32_t addres = -1;
    double resolution = -1;
    switch(motor_model)
    {
        case MOTOR_MX:
            addres = ADDR_MX_GOAL_POSITION;
            resolution = DXL_RESOLUTION;
            break;
        case MOTOR_PRO:
            addres = ADDR_PRO_GOAL_POSITION;
            resolution = PRO_RESOLUTION;
            break;
        default:
        printf("NOT A MODEL error in rotateMotor\n");
        return ;
    }

    int goal_position = (int)(angle * resolution / DXL_ANGLE_LIMIT );
    if(motor_index == 0 && (goal_position > PRO_MAXIMUM_POSITION_VALUE_FIRST || goal_position < PRO_MINIMUN_POSITION_VALUE_FIRST))
    {
        return;
    }
    if(motor_index ==1 && (goal_position > PRO_MAXIMUM_POSITION_VALUE_SECOND || goal_position < PRO_MINIMUN_POSITION_VALUE_SECOND))
    {
        return;
    }

    write4ByteTxRx(port_num, PROTOCOL_VERSION, MOTORS[motor_index], addres, goal_position);

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
    //     dxl_present_position = read4ByteTxRx(port_num, PROTOCOL_VERSION, MX_DXL_ID, ADDR_MX_PRESENT_POSITION);
    //     if((dxl_comm_result = getLastTxRxResult(port_num, PROTOCOL_VERSION)) != COMM_SUCCESS)
    //     {
    //         printTxRxResult(PROTOCOL_VERSION, dxl_comm_result);
    //     }
    //     else if((dxl_error = getLastRxPacketError(port_num, PROTOCOL_VERSION)) != 0 )
    //     {
    //         printRxPacketError(PROTOCOL_VERSION, dxl_error);
    //     } 
    //     printf("[ID:%03d] Goal:%03d  Present:%03d\n", MX_DXL_ID, goal_position, dxl_present_position);
    // } while (abs(goal_position - dxl_present_position) > DXL_MOVING_STATUS_THRESHOLD);

}
int disconnectMotor(uint8_t motor_index, uint8_t motor_model)
{
    int32_t addres = -1;
    switch(motor_model)
    {
        case MOTOR_MX:
            addres = ADDR_MX_TORQUE_ENABLE;
            break;
        case MOTOR_PRO:
            addres = ADDR_PRO_TORQUE_ENABLE;
            break;
        default:
        printf("NOT A MODEL\n");
        return -1;
    }

    write1ByteTxRx(port_num, PROTOCOL_VERSION, MOTORS[motor_index], addres, TORQUE_DISABLE);
    if ((dxl_comm_result = getLastTxRxResult(port_num, PROTOCOL_VERSION)) != COMM_SUCCESS)
    {
      printTxRxResult(PROTOCOL_VERSION, dxl_comm_result);
    }
    else if ((dxl_error = getLastRxPacketError(port_num, PROTOCOL_VERSION)) != 0)
    {
      printRxPacketError(PROTOCOL_VERSION, dxl_error);
    }

    return 0;
}

void closeMotorPort()
{
    closePort(port_num);
}

