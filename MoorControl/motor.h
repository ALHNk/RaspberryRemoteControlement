#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

int openMotorPort();
int connectMotor(uint8_t motor_index, uint8_t motor_model);
void rotateMotor(double angle,uint8_t motor_index, uint8_t motor_model);
int disconnectMotor(uint8_t motor_index, uint8_t motor_model);
void setProfileVelocity(double velcoity,uint8_t motor_index, uint8_t motor_model);
void setGoalSpeed(double speed, uint8_t motor_index, uint8_t motor_model);
double getProfileVelocity(uint8_t motor_index, uint8_t motor_model);
void closeMotorPort();                                                                                                                                                                                                                                  
double getLimitLow(uint8_t motor_index, uint8_t motor_model);
double getLimitUp(uint8_t motor_index, uint8_t motor_model);
double getPosition(uint8_t motor_index, uint8_t motor_model );
#endif