#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

int openMotorPort();
int connectMotor(uint8_t motor_index, uint8_t motor_model);
void rotateMotor(double angle,uint8_t motor_index, uint8_t motor_model);
int disconnectMotor(uint8_t motor_index, uint8_t motor_model);
void setProfileVelocity(double velcoity,uint8_t motor_index, uint8_t motor_model);
uint32_t getProfileVelocity(uint8_t motor_index, uint8_t motor_model);
void closeMotorPort();                                                                                                                                                                                                                                  

#endif