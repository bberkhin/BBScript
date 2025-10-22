
#ifndef ODriveUART_h
#define ODriveUART_h

#include <string>
#include "ODriveEnums.h"
#include "../platform_util.h"

struct ODriveFeedback {
    float pos;
    float vel;
};

typedef SerialPort ODStream;
typedef std::string String;

class ODriveUART {
public:
    /**
     * @brief Constructs an ODriveUART instance that will communicate over
     * the specified serial port.
     */
    ODriveUART(ODStream& serial);

    /**
     * @brief Clears the error status of the ODrive and restarts the brake
     * resistor if it was disabled due to an error.
     */
    void clearErrors();

    /**
     * @brief Sends a new position setpoint.
     */
    void setPosition(float position);

    /**
     * @brief Sends a new position setpoint with a velocity feedforward term.
     */
    void setPosition(float position, float velocity_feedforward);

    /**
     * @brief Sends a new position setpoint with velocity and torque feedforward terms.
     */
    void setPosition(float position, float velocity_feedforward, float torque_feedforward);

    /**
     * @brief Sends a new velocity setpoint.
     */
    void setVelocity(float velocity);

    /**
     * @brief Sends a new velocity setpoint with a torque feedforward term.
     */
    void setVelocity(float velocity, float torque_feedforward);

    /**
     * @brief Sends a new torque setpoint.
     */
    void setTorque(float torque);

    /**
     * @brief Puts the ODrive into trapezoidal trajectory mode and sends a new
     * position setpoint.
     */
    void trapezoidalMove(float position);

    /**
     * @brief Requests the latest position and velocity estimates.
     *
     * Returns pos = 0.0 and vel = 0.0 in case of a communication error.
     */
    ODriveFeedback getFeedback();

    /**
     * @brief Requests the latest position estimate.
     *
     * Returns 0.0 in case of a communication error.
     */
    float getPosition() { return getFeedback().pos; }

    /**
     * @brief Requests the latest velocity estimate.
     *
     * Returns 0.0 in case of a communication error.
     */
    float getVelocity() { return getFeedback().vel; }

    // Generic parameter access
    String getParameterAsString(const String& path);
    long getParameterAsInt(const String& path) { return std::atoi( getParameterAsString( path ).c_str()  ); }
    float getParameterAsFloat(const String& path) { return std::atof( getParameterAsString( path).c_str() ); }
    void setParameter(const String& path, const String& value);
    void setParameter(const String& path, long value) { setParameter(path,  std::to_string(value)); }

    /**
     * @brief Tells the ODrive to change its axis state.
     */
    void setState(ODriveAxisState requested_state);

    /**
     * @brief Requests the current axis state from the ODrive.
     *
     * Returns AXIS_STATE_UNDEFINED in case of a communication error.
     */
    ODriveAxisState getState();

private:
    String readLine(unsigned long timeout_ms = 10);
    ODStream& serial_;
};

#endif // ODriveUART_h
