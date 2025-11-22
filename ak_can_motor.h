#pragma once
#include <cstdint>
#include <atomic>
#include "platform_util.h"
#include "canmotorimpl.h"


/**
 * @brief Control mode definitions
 */

typedef enum {
    AK_Servo_Mode = 0,  /*!< Servo mode */
    AK_MIT_Mode,        /*!< Motion control mode */
} AK_Ctrlmode_t;

/**
 * @brief Servo mode threshold control
 */

#define MAX_PWM               0.95F      //  Maximum duty cycle 
#define MAX_CURRENT           60000.0F  //  Maximum current 
#define MAX_VELOCITY          100000.0F //  Maximum speed 
#define MAX_POSITION          36000.0F  //  Maximum position 
#define MAX_POSITION_VELOCITY 32767.0F  //  Maximum rotational speed 
#define MIN_POSITION_VELOCITY -32768.0F //  Minimum rotational speed 
#define MAX_ACCELERATION      32767.0F  //  Maximum acceleration 
#define AK_60_POLE_PAR        14//число пар полюсов
/**
 * @brief Motor mode, encapsulated as CAN ID
 */

typedef enum {
    AK_PWM = 0,
    AK_CURRENT,
    AK_CURRENT_BRAKE,
    AK_VELOCITY,
    AK_POSITION,
    AK_ORIGIN,
    AK_POSITION_VELOCITY,
    AK_IDLE
} AKMode_t;

/**
 * @defgroup Motion control mode driver
 * @{
 */

/**
 * @brief Motion control mode threshold control
*/

#define AK_MIT_LIM_POS 12.5F  // Maximum position 
#define AK_MIT_MAX_KP  500.0F // Maximum KP 
#define AK_MIT_MAX_KD  5.0F   // Maximum KD 


/**
 * @brief Model definitions, different models correspond to different parameters
 *
 */

typedef enum {
    AK10_9 = 0,
    AK60_6,
    AK70_10,
    AK80_6,
    AK80_9,
    AK80_80_64,
    AK80_8
} AK_motor_model_t;
/** 
 * @brief AK_MIT_param_limit array two-dimensional index
 * @note Different motor models only differ in speed and torque, the rest are the same
 */
enum {
    AK_MIT_LIM_SPEED = 0, // Maximum speed, index definition is not the actual value! 
    AK_MIT_LIM_TORQUE     // Maximum torque, index definition is not the actual value!    
};

/* Motor thresholds, first dimension is model, second dimension is parameter (speed, torque) */

static const float AK_MIT_param_limit[7][2] = {
    {50.0f, 65.0f}, {45.0f, 15.0f}, {50.0f, 25.0f}, {76.0f, 12.0f},
    {50.0f, 18.0f}, {8.0f, 144.0f}, {37.5f, 32.0f}
};



class AK_Motor_Class : public CanMotorDriver
{
   public:
    AK_Motor_Class(uint32_t ID, AK_motor_model_t model = AK60_6);
    virtual ~AK_Motor_Class();
   
    const char *getType() const { return "can_cubemars"; }
    void setPosition(float position, float velocity_feedforward) override;
    void setVelocity(float velocity) override;
    void stop()  override;
    void setPosToZero()  override;



    float getPosition() const override;
    float getVelocity() const override;
    bool  request(uint16_t what, uint16_t wait_ms = 0) override;
    void  tick() override;


    float getParameter( JOINT_MOTOR_PARAM type )  override;
    void  setParameter( JOINT_MOTOR_PARAM type, float param )  override;
    void  saveParameter () override;

    //states
    void setCloseLoop() override;
    void setStateIdle() override;
    void setControlModeVelocity() override;
    void setControlModePosition() override;
    //common
    void calibrate()  override;
    void clearErrors() override;
    bool onCanMessage(uint32_t id, uint8_t len, uint8_t *DataFrame) override;
private:    
    float rad_s_to_erpm(float rsd_s);
    void power_on();
    void power_off();
    /* Servo mode methods */
    void comm_can_set_duty(float duty);
    void comm_can_set_current(float current);
    void comm_can_set_cb(float current);
    void comm_can_set_rpm(float rpm);
    void comm_can_set_pos(float pos);
    void comm_can_set_origin(uint8_t set_origin_mode);
    void comm_can_set_pos_spd(float pos, float spd, float RPA);

    /* Motion control mode methods */
    void mit_can_enter_motor(void);
    void mit_can_set_origin(void);
    void mit_can_send_data(float pos,
                           float spd,
                           float kp,
                           float kd,
                           float torque);
    void mit_can_exit_motor(void);

private:
    AK_motor_model_t motor_model;   // Motor model 
    float motor_pos;                // Motor position 
    float motor_spd;                // Motor speed 
    float motor_cur_troq;           // Motor current, torque in motion control mode 
    int8_t motor_temperature;       // Motor temperature 
    uint8_t error_code;             // Motor error code 
    AK_Ctrlmode_t ak_mode_ = AK_Servo_Mode;
    std::atomic<AKMode_t> cur_mode_ = AK_IDLE;
    float cmd_duty_ = 0.f;
    float cmd_cb_ = 6000.f;
    std::atomic<float> cmd_erpm_ = 0.f;
};



