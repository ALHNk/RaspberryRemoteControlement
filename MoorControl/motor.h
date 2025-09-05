#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

int connectMotor();
void rotateMotor(double angle);
int disconnectMotor();
void setProfileVelocity(double velcoity);
uint32_t getProfileVelocity();                                                                                                                                                                                                                                  

#endif