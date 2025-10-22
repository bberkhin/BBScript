#pragma once

#include "minterfaces.h"

MotorDriverPtr createMotorDriver(const char *motor_type, int canid = 0);