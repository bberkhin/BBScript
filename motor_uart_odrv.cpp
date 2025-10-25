#include "motor_uart_odrv.h"
#include <cstdint>
#include "odrive\ODriveUART.h"
#include "platform_util.h"
#include <algorithm>



MotorUARTOdrive::MotorUARTOdrive(uint32_t id) : id_(id)
{
  SerialPort &com_port = *SerialPort::getGlobalPort();
  //odrv_ = std::make_unique<ODriveCAN>(com_port, id); 
  
  odrv_ = std::make_shared<ODriveUART>(com_port); 
  print_terminal(MSG_TYPE_INFO,"Starting ODriveUART Id = %d", id);
 
  /*
  print_terminal("Waiting for ODrive...");
  while (!odrv0_user_data.received_heartbeat) { 
    pumpEvents( com_port );
  }
*/
}
/*
uint8_t MotorUARTOdrive::getId() const
{
    return id_; 
}
*/
MotorUARTOdrive::~MotorUARTOdrive()
{
}

void MotorUARTOdrive::setPosition(float position, float velocity_feedforward)
{
    odrv_->setPosition(position, velocity_feedforward);
}

void MotorUARTOdrive::setVelocity(float velocity)
{
    odrv_->setVelocity(velocity );

}

void MotorUARTOdrive::stop()
{
    
}
 
void MotorUARTOdrive::setPosToZero() 
{
	
}

bool MotorUARTOdrive::request(uint16_t what, uint16_t wait_ms)
{
  return false;
}

float MotorUARTOdrive::getPosition() const
{
    return odrv_->getPosition();
}

float MotorUARTOdrive::getVelocity() const
{
    return odrv_->getVelocity();
}

/*
bool MotorUARTOdrive::getFeedback(double *pos, double *vel) const
{
    ODriveFeedback fb = odrv_->getFeedback();
    if ( pos )
        *pos = fb.pos;
    if ( vel )
        *vel = fb.vel;
    return true;
}
*/

float MotorUARTOdrive::getVoltage() const
{
  return odrv_->getParameterAsFloat("vbus_voltage");
}

float MotorUARTOdrive::getIqCurrent() const
{
    return odrv_->getParameterAsFloat("axis0.motor.current_control.Iq_measured");
}


void MotorUARTOdrive::setCloseLoop()
{
    odrv_->setState(AXIS_STATE_CLOSED_LOOP_CONTROL);
}

void MotorUARTOdrive::setStateIdle()
{
    odrv_->setState(AXIS_STATE_IDLE);
}

void MotorUARTOdrive::setControlModeVelocity()
{
    odrv_->setParameter("axis0.controller.config.control_mode ", std::to_string((int)CONTROL_MODE_VELOCITY_CONTROL)) ;
}

void MotorUARTOdrive::setControlModePosition()
{
    odrv_->setParameter("axis0.controller.config.control_mode ", std::to_string((int)CONTROL_MODE_POSITION_CONTROL)) ;
}
    
void MotorUARTOdrive::calibrate()
{
     odrv_->setState(AXIS_STATE_FULL_CALIBRATION_SEQUENCE);

}
void MotorUARTOdrive::clearErrors()
{
     odrv_->clearErrors();

}


