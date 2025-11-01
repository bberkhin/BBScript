// Интерфейс манипулятора (головного объекта)
#pragma once
#include <memory>
#include <cstdint>
#include <string>
#include <cstring>
#include <functional>
#include <math.h>

#define JOINTS_COUNT 6

enum FEEDBACK_TYPE {
  FB_NONE = 0,
  FB_HEARTBEAT  = 0x1,
  FB_POSITION   = 0x2,
  FB_VBUS       = 0x4,
  FB_BUSCURRENT	 = 0x8,
  FB_TORQUE	 = 0x10,
  FB_TEMPERATURE = 0x20,
  FB_ERROR = 0x40,
  FB_IQCURRENT  = 0x80
 };


class ICanInterface
{
public:
    virtual bool isOpen() = 0;
    virtual bool readMsgBufID(uint32_t *id, uint8_t *len, uint8_t *buf) = 0; // true if ok false if no msg
	virtual uint8_t sendMsgBuf(uint32_t id, uint8_t ext, uint8_t rtrBit, uint8_t len, const uint8_t *buf, bool wait_sent = true) = 0;                                 // send buf
};


//forward declaration
class IRobotModel;
class IJointController;
class IMotorDriver;
class IJoint;
class ICanInterface;

typedef std::shared_ptr<IRobotModel> RobotModelPtr;
typedef std::shared_ptr<IJointController> JointControllerPtr;
typedef std::shared_ptr<IMotorDriver> MotorDriverPtr;
typedef std::shared_ptr<IJoint> JointPtr;
typedef std::shared_ptr<ICanInterface> CanInterfacePtr;
typedef std::function<void(JointPtr&,uint8_t)> JointPtrFunctor;

struct Pose 
{
    Pose(double x, double y, double z, double roll, double pitch, double yaw)
        : x(x), y(y), z(z), roll(roll), pitch(pitch), yaw(yaw) {}
    Pose() : x(0), y(0), z(0), roll(0), pitch(0), yaw(0) {}
   
    double x;
    double y;
    double z;
    double roll;
    double pitch;
    double yaw;
};

struct JointsAngelses
{
    JointsAngelses(const float &a1=0, const float &a2=0, const float &a3=0, const float &a4=0, const float &a5=0, const float &a6=0) 
    {
        angles[0] = a1;
        angles[1] = a2;
        angles[2] = a3;
        angles[3] = a4;
        angles[4] = a5;
        angles[5] = a6;
        memset(velocity, 0, sizeof(velocity) );
    }
    JointsAngelses(float *pos, float vel) {
        memcpy(angles, pos, sizeof(float)*JOINTS_COUNT);
        if ( isnan(vel) ) {
            memset(velocity, 0, sizeof(velocity) );
        }
        else {
            for (int i = 0; i < JOINTS_COUNT; i++) {
                velocity[i] = vel;
            }
        }
     }

    float angles[JOINTS_COUNT];
    float velocity[JOINTS_COUNT];
    
};

struct FeedbackJoint
{
    int index;
    float pos = 0;
    float vel = 0;
};


class ICollaborativeRobot {
public:
    virtual ~ICollaborativeRobot() = default;
    virtual void movep(const Pose& target) = 0;
    virtual void movej(const JointsAngelses &target) = 0;
    virtual void enableManualTeachMode(bool enable = true) = 0;
    virtual void stop() = 0;
    virtual void emergencyStop() = 0;
    virtual IRobotModel* getModel() = 0;
    virtual IJointController* getJointController() = 0;
    
// todo    virtual IMotionPlanner* getMotionPlanner() = 0;
// todo    virtual ISensorSuite* getSensors() = 0;
// todo    virtual IEndEffector* getTool() = 0;
};

// Интерфейс описания кинематики/механики робота
class IRobotModel {
public:
    virtual ~IRobotModel() = default;
// todo    virtual Pose& getEndEffectorPose() const = 0;
// todo    virtual JointsAngelses &getJointAngles() const = 0;
};

// Интерфейс контроллера суставов
class IJointController {
public:
    virtual ~IJointController() = default;
    virtual void setJointTargets(const JointsAngelses &targets) = 0;
    virtual void update() = 0;
    virtual JointPtr getJointByMotor( IMotorDriver *p ) = 0;
    virtual JointPtr getJointByIndex(uint8_t index) = 0;
    virtual int getJointsCount() = 0;
    virtual void requestAll( uint16_t what, uint16_t wait_ms )  = 0;
    virtual void getCurrentJointAngles( JointsAngelses &ja)   = 0;
    // Iterate joints with a functor: non-const and const versions
    virtual void for_each_joint(const JointPtrFunctor &fn) = 0;
   // virtual void for_each_joint_const(const JointPtrFunctor &fn) const = 0;
};


// Интерфейс сустава (joint)
struct JointLimit
{
    JointLimit() {}
    float pos_min_ = 0.f;
    float pos_max_ = 0.f;
    float vel_max_ = 8.f;
    float torq_max_ = 7.f;
    float acc_max_ = 6.f;
    float cur_max_ = 10.f;
};

enum JOINT_MOTOR_PARAM {
    //read write
    JOINT_MOTOR_SPEEDLIMIT = 0,
    JOINT_MOTOR_ACCLIMIT,
    JOINT_MOTOR_CURLIMIT,
    //JOINT_MOTOR_RADONLY,
    JOINT_MOTOR_VOLTAGE,
    JOINT_MOTOR_CURRENT
};

class IJoint {
public:
    virtual ~IJoint() = default;
    virtual const std::string &getName() const = 0;
    virtual uint8_t getIndex() const = 0;
    virtual void setVelocity(float velocity) = 0;
    virtual void setPosition(float position) = 0;
    virtual IMotorDriver* getMotorDriver() = 0;
    virtual bool getFeedback( FeedbackJoint *fb) const = 0; // joint feedback considering ratio
    virtual const JointLimit& getLimits() const = 0;
    virtual void setLimits( JointLimit& ) = 0;
    virtual void moveSteps( int steps ) = 0;
    virtual void setStep( float step_pos ) = 0;
    virtual const float getStep() = 0;
    virtual float getParameter( JOINT_MOTOR_PARAM type ) = 0;
    virtual bool setParameter( JOINT_MOTOR_PARAM type, float param ) = 0;
    virtual bool isAcceptable(JOINT_MOTOR_PARAM type, float param )  = 0;
};


class IMotorDriver {
public:
    virtual ~IMotorDriver() = default;
    //virtual uint8_t getId() const = 0;
    virtual const char *getType() const = 0;

    // Управление мотором
    virtual void setPosition(float position, float velocity_feedforward) = 0;
    virtual void setVelocity(float velocity) = 0;
    virtual void stop() = 0;
    virtual void setPosToZero() = 0;


    // feedback
    virtual float getPosition() const = 0;
    virtual float getVelocity() const = 0;    
    virtual  bool request(uint16_t what, uint16_t wait_ms = 0) = 0; // FB_types

    // virtual bool   getFeedback(double *pos, double *vel) const = 0;
    virtual float getParameter( JOINT_MOTOR_PARAM type ) = 0;
    virtual void  setParameter( JOINT_MOTOR_PARAM type, float param ) = 0;

    //states
    virtual void setCloseLoop()  = 0;
    virtual void setStateIdle()  = 0;
    virtual void setControlModeVelocity() = 0;
    virtual void setControlModePosition()  = 0;
    //common
    virtual void calibrate()  = 0;
    virtual void clearErrors()  = 0;



   	static void setFeedbackCallback( void (*callback)(IMotorDriver *p ,  uint16_t type) ) { feedback_callback_ = callback; }  		
protected:
	inline  static void (*feedback_callback_)(IMotorDriver *p,  uint16_t type);
    //static std::function<void(IMotorDriver *,  uint16_t)> feedback_callback_;

};



// TODO: добавить интерфейсы для планировщика движения, сенсорной системы и рабочего инструмента
// Интерфейс планировщика движения
/*
class IMotionPlanner {
public:
    virtual ~IMotionPlanner() = default;
    virtual ITrajectory* planMotion(const IPose& start, const IPose& goal) = 0;
};

// Интерфейс сенсорной системы
class ISensorSuite {
public:
    virtual ~ISensorSuite() = default;
    virtual void updateSensors() = 0;
    virtual IForceSensor* getForceSensor() = 0;
    virtual IVisionSensor* getVisionSensor() = 0;
};

// Интерфейс рабочего инструмента
class IEndEffector {
public:
    virtual ~IEndEffector() = default;
    virtual void activate() = 0;
    virtual void deactivate() = 0;
};
*/