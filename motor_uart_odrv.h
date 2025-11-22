#include "minterfaces.h"
#include <cstdint>


// Интерфейс драйвера мотора
class ODriveUART;
typedef std::shared_ptr<ODriveUART> ODriveUARTPtr;

class MotorUARTOdrive : public IMotorDriver
{   
public:
    MotorUARTOdrive(uint32_t id = 0);    
   // uint8_t getId() const override;
    virtual ~MotorUARTOdrive();
    const char *getType() const { return "Odrive UART"; }
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
    void calibrate() override;
    void clearErrors() override;

private:
    ODriveUARTPtr odrv_;    
    uint32_t id_;
};