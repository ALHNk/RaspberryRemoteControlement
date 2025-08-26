#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

int connect();
void rotate(double angle);
int disconnect();

#endif