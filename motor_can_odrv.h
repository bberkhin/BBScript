#include "canmotorimpl.h"
#include "odrive\ODriveEnums.h"


class MotorCanOdrive : public CanMotorDriver
{   
public:
    MotorCanOdrive(uint32_t id = 0);
    virtual ~MotorCanOdrive();
    const char *getType() const { return "Odrive CAN"; }
    void setPosition(float position, float velocity_feedforward) override;
    void setVelocity(float velocity) override;
    void stop()  override;
    void setPosToZero()  override;



    float getPosition() const override;
    float getVelocity() const override;
    bool request(uint16_t what, uint16_t wait_ms = 0) override;

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
    bool onCanMessage(uint32_t id, uint8_t , uint8_t *DataFrame)  override;
private:
    void setControlMode(uint8_t control_mode, uint8_t input_mode = INPUT_MODE_PASSTHROUGH );
    void setState( uint32_t requested_state );
    bool awaitMsg(uint8_t req_msg_id, uint16_t timeout_ms);
private:
    //ODriveCANPtr odrv_;    
    float Position = 0.0f; // [rev] //Absolute_Position
    float Torque_Target = 0.0f; // [Nm]
    float Torque_Estimate = 0.0f; // [Nm] 
    float Bus_Voltage = 0.0f; // [V]
    float Bus_Current = 0.0f; // [A]
    float FET_Temperature = 0.0f; // [deg C]
    float Motor_Temperature = 0.0f; // [deg C]
    float Iq_Setpoint = 0.0f; // [A]
    float Iq_Measured = 0.0f; // [A]
    float Pos_Estimate = 0.0f; // [rev]
    float Vel_Estimate = 0.0f; // [rev/s]
    float limit_cur_ = 0.f;
    float limit_speed_ = 0.f;

    uint32_t Active_Errors = 0;
    uint32_t Disarm_Reason = 0;
    uint32_t Axis_Error = 0;
    uint8_t Axis_State = 0;
    uint8_t Procedure_Result = 0;
    uint8_t Trajectory_Done_Flag = 0;

};