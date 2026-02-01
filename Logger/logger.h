#ifndef LOGGER_H
#define LOGGER_H

#pragma once
#include <stdio.h>

int log_init(const char *path);
void log_msg(const char *fmt, ...);
void log_close();

#endif