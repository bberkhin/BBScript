#include "ak_can_motor.h"

/**
 * @file    ak_motor.cpp
 * @author  Deadline--
 * @brief   AK80 driver code
 * @version 0.2
 * @date    2023-11-27
 * @see     https://github.com/Yangwen-li13/CubeMars-AK60-6/
 **********************************************************************
 @verbatim
 ======================================================================
                       ##### Library usage notes #####
 ======================================================================
 (#) Instantiate an `AK_Motor_Class` object with the motor model and CAN ID.
 (#) The constructor inserts the motor object into a linked list so incoming
     CAN callbacks can find the corresponding motor instance and populate its
     fields. When constructing a new object the list is searched for an
     existing object with the same CAN ID. If found, the `id_conflict`
     member is set to `true` — users should check this flag.
     During CAN receive callbacks the list is traversed to find the matching
     CAN ID and assign values to that object's properties.
 (#) The linked list uses the Linux kernel-style list implementation for
     reliability.
 */
#define M_2PI 6.283185307179586

void buffer_append_int16(uint8_t* buffer, int16_t number, int32_t* index) {
    buffer[(*index)++] = number >> 8;
    buffer[(*index)++] = number;
}

static  void buffer_append_uint16(uint8_t* buffer, uint16_t number, int32_t* index) {
    buffer[(*index)++] = number >> 8;
    buffer[(*index)++] = number;
}

static void buffer_append_int32(uint8_t* buffer, int32_t number, int32_t* index) {
    buffer[(*index)++] = number >> 24;
    buffer[(*index)++] = number >> 16;
    buffer[(*index)++] = number >> 8;
    buffer[(*index)++] = number;
}

int float_to_uint(float x, float x_min, float x_max, uint8_t bits) {
    // Converts a float to an unsigned int, given range and number of bits
    float span = x_max - x_min;
    float offset = x_min;
    unsigned int pgg = 0;
    if (bits == 12) {
        pgg = (unsigned int)((x - offset) * 4095.0 / span);
    } else if (bits == 16) {
        pgg = (unsigned int)((x - offset) * 65535.0 / span);
    }
    return pgg;
}

float uint_to_float(int x_int, float x_min, float x_max, uint8_t bits) {
    float span = x_max - x_min;
    float offset = x_min;
    float pgg = 0;
    if (bits == 12) {
        pgg = ((float)x_int) * span / 4095.0 + offset;
    } else if (bits == 16) {
        pgg = ((float)x_int) * span / 65535.0 + offset;
    }
    return pgg;
}

// Combine mode and ID

static inline uint32_t canid_append_mode(uint8_t id, AKMode_t Mode_set) {
    uint32_t mode;
    mode = Mode_set;
    return (uint32_t)(id | mode << 8);
}

static inline void param_limit(float* value, float min_value, float max_value) {
    if (*value > max_value) {
        *value = max_value;
    } else if (*value < min_value) {
        *value = min_value;
    }
}

/// @brief /////////////////////////////////////////////////////////////////
/// @param ID 
/// @param model 

AK_Motor_Class::AK_Motor_Class(uint32_t ID, AK_motor_model_t model) :
    CanMotorDriver(ID), motor_model(model)    
{  
   // start();
}
/**
 * @brief Destroy the ak motor class::ak motor class object
 *
 */
AK_Motor_Class::~AK_Motor_Class() 
{
  
}

static bool started = false;

void AK_Motor_Class::power_on()
{
    
    if ( started )
        return;
    uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0XFC};
    //AKcmd_can_transmit_mit(controller_id, data, 8);
    sendMsgBuf( CAN_ID, 1, 0, 8, data);
    tick();
    started = true;
}

void AK_Motor_Class::power_off()
{

    if ( !started )
        return;
    uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0XFD};
    //AKcmd_can_transmit_mit(controller_id, data, 8);
    sendMsgBuf( CAN_ID, 1, 0, 8, data);
    tick();
    started = false;
}



void AK_Motor_Class::setCloseLoop()
{

    // let it be 6 amper it will be 6000:  1 = 0.001 А 

   // cmd_cb_ = 6000.f;
    //cur_mode_  = AK_CURRENT_BRAKE;
    power_on();
}

void AK_Motor_Class::setStateIdle()
{
    cur_mode_ = AK_IDLE;
    cmd_duty_ = 0.f;    
    power_off();
    //comm_can_set_cb(0);
}

void AK_Motor_Class::setControlModeVelocity()
{
}

void AK_Motor_Class::setControlModePosition()
{
}
    //common
void AK_Motor_Class::calibrate() 
{
}


void AK_Motor_Class::clearErrors()
{
}

void AK_Motor_Class::setPosition(float position, float velocity_feedforward)
{
    //comm_can_set_pos(position);
    //comm_can_set_pos_spd( position, velocity_feedforward, ass );
    cmd_duty_ = position;
    cur_mode_  = AK_PWM;
}

void AK_Motor_Class::setVelocity(float rad_s)
{
    //velocty to erpm   
    cmd_erpm_ = rad_s_to_erpm(rad_s);
    cur_mode_  = AK_VELOCITY;
    //comm_can_set_duty(velocity);   
}

void AK_Motor_Class::stop()
{
  //setStateIdle();
  cur_mode_ = AK_IDLE;
  //comm_can_set_rpm(0.f);
}

void AK_Motor_Class::setPosToZero() 
{  
  comm_can_set_origin(0);
}


float AK_Motor_Class::getPosition() const
{
    return motor_pos; // [rev]
}

float AK_Motor_Class::getVelocity() const
{
    return motor_spd;
}
  
float AK_Motor_Class::getParameter( JOINT_MOTOR_PARAM type )
{
	switch(type)
	{
		case JOINT_MOTOR_SPEEDLIMIT:	
            return MAX_VELOCITY;
		case JOINT_MOTOR_CURLIMIT:
			  return MAX_CURRENT;
		case JOINT_MOTOR_ACCLIMIT:
			return MAX_ACCELERATION;
		case JOINT_MOTOR_VOLTAGE:
			return 48.f;
		case JOINT_MOTOR_CURRENT:			
             return MAX_CURRENT; //?? Iq_Setpoint		
        //     float motor_cur_troq;           // Motor current, torque in motion control mode 

	}
    return 0.f;

}

// TODO Speed limit 0X7024 or drw.limit_spd.index==0X7017
void  AK_Motor_Class::setParameter( JOINT_MOTOR_PARAM type, float param )
{

}


void AK_Motor_Class::saveParameter()
{
	//TODO
}


bool AK_Motor_Class::request(uint16_t what, uint16_t wait_ms)
{
   return true;
}


void AK_Motor_Class::tick()
{
    switch( cur_mode_ )
    {
        case AK_PWM: 
            comm_can_set_duty(cmd_duty_); 
        break;
        case AK_CURRENT_BRAKE: 
            comm_can_set_cb(cmd_cb_); 
            break;
        case AK_VELOCITY: 
            comm_can_set_rpm(cmd_erpm_); 
            break;
        case AK_IDLE:
            ;//comm_can_set_duty(0.f); 
        break;
    };      
}

/**
 * @brief Get motor state parameters. MIT (motion control) and Servo modes use
 *        the same parameters but have different CAN frame formats.
 *
 * @param can_id CAN ID
 * @param can_msg CAN message payload
 * @param AK_mode operating mode
 * @note This function is weak and can be overridden
 */

bool AK_Motor_Class::onCanMessage(uint32_t can_id, uint8_t len, uint8_t *can_msg)
{
    if ( ak_mode_ == AK_Servo_Mode )
        can_id &= 0xFF;

    if ( can_id  != CAN_ID )
        return false;
 
    if (ak_mode_ == AK_Servo_Mode) 
    {
    /* Combine integer bytes and convert to floating point values */
        motor_pos = (float)((int16_t)(can_msg[0] << 8 | can_msg[1])) * 0.1f;
        motor_spd = (float)((int16_t)(can_msg[2] << 8 | can_msg[3])) * 10.0f;
        motor_cur_troq = (float)((int16_t)(can_msg[4] << 8 | can_msg[5])) * 0.01f;
        motor_temperature = can_msg[6];
        error_code = can_msg[7];               
    } else if (ak_mode_ == AK_MIT_Mode) 
    {
    /* Combine integer fields */
        int16_t pos_int = (can_msg[1] << 8) | can_msg[2];
        int16_t spd_int = (can_msg[3] << 4) | (can_msg[4] >> 4);
        int16_t torq_int = ((can_msg[4] * 0xF) << 8) | can_msg[5];
        motor_temperature = can_msg[6];
        error_code = can_msg[7];

    /* Convert to floating point values */
        motor_pos = uint_to_float(pos_int, -AK_MIT_LIM_POS, AK_MIT_LIM_POS, 16);
        motor_spd = uint_to_float(
            spd_int,
            -AK_MIT_param_limit[motor_model][AK_MIT_LIM_SPEED],
            AK_MIT_param_limit[motor_model][AK_MIT_LIM_SPEED], 12);
        motor_cur_troq = uint_to_float(
            torq_int,
            -AK_MIT_param_limit[motor_model][AK_MIT_LIM_TORQUE],
            AK_MIT_param_limit[motor_model][AK_MIT_LIM_TORQUE], 12);
    }
    if (feedback_callback_	)
			feedback_callback_( this, FB_POSITION | FB_TEMPERATURE | FB_TORQUE | FB_ERROR );
    return true;
}


// Servo mode driver
 
//Set motor speed in duty (PWM) mode Duty cycle, range `0 ~ 1.0`

void AK_Motor_Class::comm_can_set_duty(float duty) {
    param_limit(&duty, -MAX_PWM, MAX_PWM);
    int32_t send_index = 0;
    uint8_t buffer[4];
    buffer_append_int32(buffer, (int32_t)(duty * 100000.0f), &send_index);
    sendMsgBuf( canid_append_mode(CAN_ID, AK_PWM), 1, 0, send_index, buffer);
    //AKcmd_can_transmit_eid(canid_append_mode(controller_id, AK_PWM), buffer,send_index);
}
/**
 * @brief Set motor current
 *
 * @param current Current value (mA)
 * @note Since motor `output torque = iq * KT`, this can be used as a torque loop
 */
void AK_Motor_Class::comm_can_set_current(float current) {
    param_limit(&current, -MAX_CURRENT, MAX_CURRENT);
    int32_t send_index = 0;
    uint8_t buffer[4];
    buffer_append_int32(buffer, (int32_t)(current * 1000.0f), &send_index);
    sendMsgBuf( canid_append_mode(CAN_ID, AK_CURRENT), 1, 0, send_index, buffer);
    //AKcmd_can_transmit_eid(canid_append_mode(controller_id, AK_CURRENT), buffer,send_index);
}
/**
 * @brief Set motor brake current
 *
 * @param current Brake current
 */
void AK_Motor_Class::comm_can_set_cb(float current) {
    param_limit(&current, -MAX_CURRENT, MAX_CURRENT);
    int32_t send_index = 0;
    uint8_t buffer[4];
    buffer_append_int32(buffer, (int32_t)(current * 1000.0f), &send_index);
    sendMsgBuf( canid_append_mode(CAN_ID, AK_CURRENT_BRAKE), 1, 0, send_index, buffer);
    //AKcmd_can_transmit_eid(canid_append_mode(controller_id, AK_CURRENT_BRAKE),buffer, send_index);
    
}
/**
 * @brief Set speed in velocity control mode
 *
 * @param rpm Rate; `erpm = rpm * number_of_pole_pairs`
 */


 float AK_Motor_Class::rad_s_to_erpm(float rad_s) {
    const int pole_pairs = AK_60_POLE_PAR; // AK-60-6
    return static_cast<float>(rad_s * 60.0 * pole_pairs / M_2PI);
}

void AK_Motor_Class::comm_can_set_rpm(float erpm) {
    param_limit(&erpm, -MAX_VELOCITY, MAX_VELOCITY);
    int32_t send_index = 0;
    uint8_t buffer[4];
    buffer_append_int32(buffer, (int32_t)erpm, &send_index);
    //AKcmd_can_transmit_eid(canid_append_mode(controller_id, AK_VELOCITY), buffer, send_index);
    sendMsgBuf( canid_append_mode(CAN_ID, AK_VELOCITY), 1, 0, send_index, buffer);
}
/**
 * Set position in position loop mode
 *
 * @param pos Position (angle, range -36 000° ~ 36 000°)
 * @note Default speed is 12,000 erpm, default acceleration is 40,000 erpm.
 */
void AK_Motor_Class::comm_can_set_pos(float pos) {
    param_limit(&pos, -MAX_POSITION, MAX_POSITION);
    int32_t send_index = 0;
    uint8_t buffer[4];
    buffer_append_int32(buffer, (int32_t)(pos * 10000.0f), &send_index);
    //AKcmd_can_transmit_eid(canid_append_mode(controller_id, AK_POSITION),buffer, send_index);
    sendMsgBuf( canid_append_mode(CAN_ID, AK_POSITION), 1, 0, send_index, buffer);
    
}

/**
 * @brief Set origin
 *
 * @param set_origin_mode Origin set mode:
 *  @arg 0 - set temporary origin (cleared on power off);
 *  @arg 1 - set permanent zero (parameters saved);
 *  @arg 2 - restore default zero (parameters saved)
 */
void AK_Motor_Class::comm_can_set_origin(uint8_t set_origin_mode) {
    int32_t send_index = 1;
    uint8_t buffer[4];
    buffer[0] = set_origin_mode;
    //AKcmd_can_transmit_eid(canid_append_mode(controller_id, AK_ORIGIN), buffer,send_index);
    sendMsgBuf( canid_append_mode(CAN_ID, AK_ORIGIN), 1, 0, send_index, buffer);
}
/**
 @brief Speed-position loop mode
 *
 * @param pos Position
 *  @arg Input range -36 000° ~ 36 000°
 * @param spd Speed
 *  @arg Input range -32 768 ~ -32 767, corresponds to speed -327 680 ~ -327 670 erpm
 * @param RPA Acceleration
 *  @arg Input range 0 ~ 32 767, corresponds to acceleration 0 ~ 327 670 erpm/s^2
 */
void AK_Motor_Class::comm_can_set_pos_spd(float pos, float spd, float RPA) {
    param_limit(&pos, -MAX_POSITION, MAX_POSITION);
    param_limit(&RPA, 0.0f, MAX_ACCELERATION);
    param_limit(&spd, MIN_POSITION_VELOCITY, MAX_POSITION);

    int32_t send_index = 0;
    int32_t send_index1 = 0;
    uint8_t buffer[4];
    buffer_append_int32(buffer, (int32_t)(pos * 10000.0f), &send_index);
    buffer_append_int16(buffer, spd, &send_index1);
    buffer_append_int16(buffer, RPA, &send_index1);
   // AKcmd_can_transmit_eid(canid_append_mode(controller_id, AK_POSITION_VELOCITY), buffer,send_index);
    sendMsgBuf( canid_append_mode(CAN_ID, AK_POSITION_VELOCITY), 1, 0, send_index, buffer);
}

/**
 * @defgroup Motion control mode driver
 * @{
 */

/**
 * @brief Enter motor control mode in motion control
 *
 * @attention You must enter the control mode first before you can control the motor!
 */

 void AK_Motor_Class::mit_can_enter_motor(void) {
    uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0XFC};
    
    //AKcmd_can_transmit_mit(controller_id, data, 8);
    sendMsgBuf( CAN_ID, 0, 0, 8, data);
    ak_mode_ = AK_MIT_Mode;

}
/**
 * @brief Set the motor zero point in motion control mode
 *
 */
void AK_Motor_Class::mit_can_set_origin(void) {
    uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0XFE};
    //AKcmd_can_transmit_mit(controller_id, data, 8);
    sendMsgBuf( CAN_ID, 0, 0, 8, data);
}

/**
 * @brief Make the motor enter control
 *
 * @param pos Motor position
 * @param spd Motor speed
 * @param kp Motion proportional gain
 * @param kd Motion damping gain
 * @param torque Torque
 * @attention You must enter control mode first before you can control the motor!
 */

void AK_Motor_Class::mit_can_send_data(float pos,
                                       float spd,
                                       float kp,
                                       float kd,
                                       float torque) {
    /* Convert to integer */
    int16_t pos_int = float_to_uint(pos, -AK_MIT_LIM_POS, AK_MIT_LIM_POS, 16);
    int16_t spd_int =
        float_to_uint(spd, -AK_MIT_param_limit[motor_model][AK_MIT_LIM_SPEED],
                      AK_MIT_param_limit[motor_model][AK_MIT_LIM_SPEED], 12);
    int16_t kp_int = float_to_uint(kp, 0, AK_MIT_MAX_KP, 12);
    int16_t kd_int = float_to_uint(kd, 0, AK_MIT_MAX_KD, 12);
    int16_t torque_int = float_to_uint(
        torque, -AK_MIT_param_limit[motor_model][AK_MIT_LIM_TORQUE],
        AK_MIT_param_limit[motor_model][AK_MIT_LIM_TORQUE], 12);

    /* Fill the buffer. */
    uint8_t data[8];
    data[0] = pos_int >> 8;                           /* High 8 bits of position */
    data[1] = pos_int & 0xFF;                         /* Low 8 bits of position */
    data[2] = spd_int >> 4;                           /* High 8 bits of speed */
    data[3] = ((spd_int & 0xF) << 4) | (kp_int >> 8); /* Low 4 bits of speed, high 4 bits of kp */
    data[4] = kp_int & 0xFF;                          /* Low 8 bits of kp */
    data[5] = kd_int >> 4;                            /* High 8 bits of kd */
    data[6] =
        ((kd_int & 0xF) << 4) | (torque_int >> 8); /*  Low 4 bits of kd, high 4 bits of torque  */
    data[7] = torque_int & 0xFF;                   /* Low 8 bits of torque */
    //AKcmd_can_transmit_mit(controller_id, data, 8);
    sendMsgBuf( CAN_ID, 0, 0, 8, data);

}


/**
 * @brief Exit motor control mode
 *
 */
void AK_Motor_Class::mit_can_exit_motor(void) {
    uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0XFD};
    //AKcmd_can_transmit_mit(controller_id, data, 8);
    sendMsgBuf( CAN_ID, 0, 0, 8, data);
    ak_mode_ = AK_Servo_Mode;

}

