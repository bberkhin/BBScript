
#include "motors_facoty.h"
#include "motor_can_odrv.h"
#include "motor_uart_odrv.h"
#include "robstride/Robstride.h"
#include <cstring>

MotorDriverPtr createMotorDriver(const char *motor_type, int canId )
{
    if ( motor_type == nullptr || *motor_type == 0 )
        return nullptr;     
    if ( strcmp(motor_type, "can_odrv") == 0 )
        return std::make_shared<MotorCanOdrive>(canId);     
    if ( strcmp(motor_type, "uart_odrv") == 0 )
        return std::make_shared<MotorUARTOdrive>(canId);     
    if ( strcmp(motor_type, "can_robstride") == 0 )
        return std::make_shared<RobStride_Motor>(canId);    
    
    return nullptr;
}

