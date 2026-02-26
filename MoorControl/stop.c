#include <stdio.h>
#include "dynamixel_sdk/dynamixel_sdk.h"

#define DEVICENAME "/dev/serial/dynamixel"
#define BAUDRATE 115200

#define PROTOCOL_VERSION 2.0
#define ADDR_TORQUE_ENABLE 512
#define TORQUE_DISABLE 0

#define MOTOR1_ID 1
#define MOTOR2_ID 2
#define MOTOR3_ID 3
#define MOTOR4_ID 4

int main()
{
    int port_num = portHandler(DEVICENAME);
    packetHandler();

    if (!openPort(port_num))
    {
        printf("Failed to open port\n");
        return 1;
    }

    if (!setBaudRate(port_num, BAUDRATE))
    {
        printf("Failed to set baudrate\n");
        return 1;
    }

    write1ByteTxRx(port_num, PROTOCOL_VERSION, MOTOR1_ID, ADDR_TORQUE_ENABLE, TORQUE_DISABLE);
    write1ByteTxRx(port_num, PROTOCOL_VERSION, MOTOR1_ID, ADDR_TORQUE_ENABLE, TORQUE_DISABLE);
    write1ByteTxRx(port_num, PROTOCOL_VERSION, MOTOR3_ID, ADDR_TORQUE_ENABLE, TORQUE_DISABLE);
    write1ByteTxRx(port_num, PROTOCOL_VERSION, MOTOR4_ID, ADDR_TORQUE_ENABLE, TORQUE_DISABLE);

    closePort(port_num);

    printf("Torque OFF sent\n");
    return 0;
}