#include "minterfaces.h"
#include <cstdint>


// Интерфейс драйвера мотора
class ODriveUART;
typedef std::shared_ptr<ODriveUART> ODriveUARTPtr;

class MotorUARTOdrive : public IMotorDriver
{   
public:
    MotorUARTOdrive(uint32_t id = 0);
    uint8_t getId() const override;
    virtual ~MotorUARTOdrive();
    void setPosition(float position, float velocity_feedforward) override;
    void setVelocity(float velocity) override;
    void stop()  override;

    float getPosition() const override;
    float getVelocity() const override;
    float getVoltage() const override;
    float getIqCurrent() const override;
    bool request(uint16_t what, uint16_t wait_ms = 0) override;

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