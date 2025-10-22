#include "motor_can_odrv.h"
//#include <can_helpers.hpp>
//#include <Arduino.h>

#include "odrive\ODriveCAN.h"
#include "platform_util.h"
//#include <algorithm>

static const uint8_t rNodeIdShift = 5;
static const uint8_t rCmdIdBits = 0x1F;

enum ODriveCMD
{
  ODriveCMD_HEART       = 0x001,
  ODriveCMD_MTR_ERROR   = 0x003, // Get Motor Error`
  ODriveCMD_ENC_ERROR   = 0x004, // Get Encoder Error
  ODriveCMD_SNSL_ERROR  = 0x005, // Get Sensorless Error
  ODriveCMD_SETSTATE = 0x007,
  ODriveCMD_FEEDBACK    = 0x009, // Get Encoder Estimates
  ODriveCMD_ENC_COUNT   = 0x00A, // Get Encoder Count
  ODriveCMD_SETCTRLMODE = 0x00B,
  ODriveCMD_TEMPERATURE = 0x015, // Get Sensorless Estimates
  ODriveCMD_REBOOT      = 0x016, //  Reboot ODrive
  ODriveCMD_VBUS        = 0x017, // Get Vbus Voltage
  ODriveCMD_TORQUE      = 0x01C,
  ODriveCMD_SETPOSITION = 0x00C,
  ODriveCMD_SETVELOCITY = 0x00D,
  ODriveCMD_IQ          = 0x014,
  ODriveCMD_CLEARERROR = 0x018,
  ODriveCMD_SETZERO =    0x019

  
};


uint8_t getRTRBit(uint8_t cmd) 
{
  switch(cmd) {
    case ODriveCMD_MTR_ERROR: 
    case ODriveCMD_ENC_ERROR: 
    case ODriveCMD_SNSL_ERROR: 
    case ODriveCMD_FEEDBACK:
    case ODriveCMD_ENC_COUNT:
    case ODriveCMD_TEMPERATURE:
    //case ODriveCMD_REBOOT:
    case ODriveCMD_VBUS:
      return 1; // Remote frame
    default:
      return 0; // Default to data frame for unknown commands
  }
}


MotorCanOdrive::MotorCanOdrive(uint32_t id) : 
  CanMotorDriver(id)
{
 // odrv_ = std::make_shared<ODriveCAN>(*this->can_bus_, id); 
}

MotorCanOdrive::~MotorCanOdrive()
{
}


void MotorCanOdrive::setControlMode(uint8_t control_mode, uint8_t input_mode)
{

    uint8_t buf[8] = {};
    can_set_signal_raw<uint32_t>(buf, control_mode, 0, 32, true);
    can_set_signal_raw<uint32_t>(buf, input_mode, 32, 32, true);   
    sendMsgBuf((CAN_ID << rNodeIdShift) | ODriveCMD_SETCTRLMODE, 0, 0, 8, buf);
}


void MotorCanOdrive::setState( uint32_t requested_state )
{
  uint8_t buf[8] = {};
  can_set_signal_raw<uint32_t>(buf, requested_state, 0, 32, true);
  sendMsgBuf((CAN_ID << rNodeIdShift) | ODriveCMD_SETSTATE, 0, 0, 8, buf);

}

void MotorCanOdrive::setCloseLoop()
{
    setState( AXIS_STATE_CLOSED_LOOP_CONTROL );
}

void MotorCanOdrive::setStateIdle()
{
    setState(AXIS_STATE_IDLE);
}

void MotorCanOdrive::setControlModeVelocity()
{
  setControlMode(CONTROL_MODE_VELOCITY_CONTROL); 
}
void MotorCanOdrive::setControlModePosition()
{
  setControlMode(CONTROL_MODE_POSITION_CONTROL );
}
    //common
void MotorCanOdrive::calibrate() 
{
  setState(AXIS_STATE_FULL_CALIBRATION_SEQUENCE);
}


void MotorCanOdrive::clearErrors()
{
    uint8_t buf[1] = {};
    can_set_signal_raw<uint8_t>(buf, 0, 0, 8, true);
    sendMsgBuf((CAN_ID << rNodeIdShift) | ODriveCMD_CLEARERROR, 0, 0, 1, buf);
}

void MotorCanOdrive::setPosition(float position, float velocity_feedforward)
{
    uint8_t buf[8] = {};
    can_set_signal_raw<float>(buf, position, 0, 32, true);
    can_set_signal_raw<int16_t>(buf, velocity_feedforward, 32, 16, true, 0.001f, 0.0f);
    can_set_signal_raw<int16_t>(buf, 0.0, 48, 16, true, 0.001f, 0.0f); //torque_feedforward
    sendMsgBuf((CAN_ID << rNodeIdShift) | ODriveCMD_SETPOSITION, 0, 0, 8, buf);
}

void MotorCanOdrive::setVelocity(float velocity)
{
    uint8_t buf[8] = {};
    can_set_signal_raw<float>(buf, velocity, 0, 32, true);
    can_set_signal_raw<float>(buf, 0.0, 32, 32, true); //Input_Torque_FF
    sendMsgBuf((CAN_ID << rNodeIdShift) | ODriveCMD_SETVELOCITY, 0, 0, 8, buf);
}

void MotorCanOdrive::stop()
{
  setStateIdle();
  //setCloseLoop();
}

void MotorCanOdrive::setPosToZero() 
{  
  uint8_t buf[8] = {};
  float Position = 0;
  can_set_signal_raw<float>(buf, Position, 0, 32, true);
  sendMsgBuf((CAN_ID << rNodeIdShift) | ODriveCMD_SETZERO, 0, 0, 8, buf);  
}


float MotorCanOdrive::getPosition() const
{
    return Pos_Estimate; // [rev]
}

float MotorCanOdrive::getVelocity() const
{
    return Vel_Estimate;
}

float MotorCanOdrive::getVoltage() const
{
  return Bus_Voltage;
}


float MotorCanOdrive::getIqCurrent() const
{
  return Iq_Measured; //?? Iq_Setpoint
}


bool MotorCanOdrive::request(uint16_t what, uint16_t wait_ms)
{
  if  ( !can_bus_ )
    return false;
  
	if (what & FB_POSITION ) 
	{
    can_bus_->sendMsgBuf( (this->CAN_ID << rNodeIdShift) | ODriveCMD_FEEDBACK, 0, getRTRBit( ODriveCMD_FEEDBACK ),0, nullptr);
		awaitMsg(ODriveCMD_FEEDBACK, wait_ms);
	}
	if ( (what & FB_VBUS) | (what & FB_BUSCURRENT) )
	{    
		can_bus_->sendMsgBuf( (this->CAN_ID << rNodeIdShift) | ODriveCMD_VBUS, 0, getRTRBit( ODriveCMD_VBUS ),0, nullptr);
    awaitMsg(ODriveCMD_VBUS, wait_ms);
	}
  if (what & FB_TEMPERATURE) 
  {
		can_bus_->sendMsgBuf( (this->CAN_ID << rNodeIdShift) | ODriveCMD_TEMPERATURE, 0, getRTRBit( ODriveCMD_TEMPERATURE ),0, nullptr);
		awaitMsg(ODriveCMD_TEMPERATURE, wait_ms);
	}
  if ( what & FB_TORQUE )  
  {
		can_bus_->sendMsgBuf( (this->CAN_ID << rNodeIdShift) | ODriveCMD_TORQUE, 0, getRTRBit( ODriveCMD_TORQUE ),0, nullptr);
		awaitMsg(ODriveCMD_TORQUE, wait_ms);
	}
  if ( what & FB_IQCURRENT )  
  {
		can_bus_->sendMsgBuf( (this->CAN_ID << rNodeIdShift) | ODriveCMD_IQ, 0, getRTRBit( ODriveCMD_IQ ),0, nullptr);
  	awaitMsg(ODriveCMD_IQ, wait_ms);
	}
  return true;
}

bool MotorCanOdrive::awaitMsg(uint8_t req_msg_id, uint16_t timeout_ms) {
    if ( timeout_ms == 0 )
      return false;

    static uint32_t id;
    static uint8_t buffer[8];
    static uint8_t len;
    uint64_t start_time = millis();
    while((millis() - start_time) < timeout_ms)
    {
      if (  can_bus_->readMsgBufID(&id, &len, buffer) ) {
        if ( onCanMessage(id, len, buffer) )
        {
          if( (id & 0x1F) == req_msg_id )
            return true;
        }
      }
    }
    return false;
}



bool MotorCanOdrive::onCanMessage(uint32_t id, uint8_t , uint8_t *buf)
{
  int node_id = (id >> rNodeIdShift);
  int msg_id = (id & 0x001F);
  if (this->CAN_ID != node_id) //(id >> rNodeIdShift))
    return false;
  switch ( msg_id ) {
      case ODriveCMD_FEEDBACK: {
          Pos_Estimate = can_get_signal_raw<float>(buf, 0, 32, true);
          Vel_Estimate = can_get_signal_raw<float>(buf, 32, 32, true);    
          if (feedback_callback_	)
            feedback_callback_( this, FB_POSITION );
          break;
      }
      case ODriveCMD_TORQUE: {
          Torque_Target = can_get_signal_raw<float>(buf, 0, 32, true);
          Torque_Estimate = can_get_signal_raw<float>(buf, 32, 32, true);
          if (feedback_callback_)
              feedback_callback_(this, FB_TORQUE);
          break;
      }
      case ODriveCMD_HEART: {
          Axis_Error = can_get_signal_raw<uint32_t>(buf, 0, 32, true);
          Axis_State = can_get_signal_raw<uint8_t>(buf, 32, 8, true);
          Procedure_Result = can_get_signal_raw<uint8_t>(buf, 40, 8, true);
          Trajectory_Done_Flag = can_get_signal_raw<uint8_t>(buf, 48, 1, true);
          if (feedback_callback_	)
            feedback_callback_( this, FB_HEARTBEAT );
          break;
      } 
      case ODriveCMD_TEMPERATURE: {
          FET_Temperature = can_get_signal_raw<float>(buf, 0, 32, true);
          Motor_Temperature = can_get_signal_raw<float>(buf, 32, 32, true);  
          if (feedback_callback_	)
            feedback_callback_( this, FB_TEMPERATURE );
          break;
      }
      case ODriveCMD_VBUS: {
        Bus_Voltage = can_get_signal_raw<float>(buf, 0, 32, true);
        Bus_Current = can_get_signal_raw<float>(buf, 32, 32, true);
        if (feedback_callback_	)
          feedback_callback_( this, FB_VBUS | FB_BUSCURRENT);
        break;
      }
      case ODriveCMD_IQ: {
          Iq_Setpoint = can_get_signal_raw<float>(buf, 0, 32, true);
          Iq_Measured = can_get_signal_raw<float>(buf, 32, 32, true);
          if (feedback_callback_	)
            feedback_callback_( this, FB_IQCURRENT );
          break;
      }
      case ODriveCMD_MTR_ERROR: {
          Active_Errors = can_get_signal_raw<uint32_t>(buf, 0, 32, true);
          Disarm_Reason = can_get_signal_raw<uint32_t>(buf, 32, 32, true);
              if (feedback_callback_	)
            feedback_callback_( this, FB_ERROR );
          break;
      }
      default:
        return false;
  }
	return true;
}


