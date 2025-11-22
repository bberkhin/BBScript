#pragma once

#include "minterfaces.h"
#include <string>
#include <vector>
#include <functional>
#include <rapidxml.hpp>
#include "feedbackhandler.h"



#define LIMIT_DEFAULT_VEL 2
typedef std::vector<JointPtr> JointsList;

class MRobot : public ICollaborativeRobot
{
    public:
        MRobot();
        MRobot( IRobotModel *model, IJointController *jointController );
        ~MRobot() override;
        void movep(const Pose& target) override;
        void movej( const JointsAngelses &target ) override;
        void enableManualTeachMode(bool enable = true) override;
        void stop() override;
        void emergencyStop() override;
        IRobotModel* getModel() override;
        IJointController* getJointController() override;
        FeedBackHandler * getFeedBackHandler() { return &fbhandler_; }
    public:
        bool Load(const char *urdfdata);
        bool Load( rapidxml::xml_node<>* nd);
        bool pumpEvents(ICanInterface *can);
        // Convenience wrappers to iterate joints through MRobot
        void for_each_joint(const JointPtrFunctor &fn);
      //  void for_each_joint_const(const JointPtrFunctor &fn) const;
      private:
        RobotModelPtr model_;
        JointControllerPtr jointController_;
        mutable JointsAngelses currentJointAngles_;
        FeedBackHandler fbhandler_;
};

// Интерфейс описания кинематики/механики робота
class RobotModel : public IRobotModel 
{ 
public:
    RobotModel();    
    virtual ~RobotModel() = default;
    // todo    virtual Pose& getEndEffectorPose() const = 0;
    // todo    virtual JointsAngelses &getJointAngles() const = 0;     
};

// Интерфейс контроллера суставов
class JointController : public IJointController
{   
public:
    JointController();
    virtual ~JointController() = default;
    JointPtr getJointByMotor(IMotorDriver *p) override;    
    JointPtr getJointByIndex(uint8_t index) override;
    int getJointsCount() override;
    void requestAll( uint16_t what, uint16_t wait_ms ) override;
    void tickAll()  override;
    void getCurrentJointAngles( JointsAngelses &ja)  override;
    // Iterate joints with a functor: non-const and const versions
public:    
    void for_each_joint(const JointPtrFunctor &fn);
  //  void for_each_joint_const(const JointPtrFunctor &fn) const;
    JointPtr addJoint(rapidxml::xml_node<>* nd);    
private:
    JointsAngelses targetAngles_;
    JointsList joints_;
};

// Интерфейс сустава (joint)
class Joint : public IJoint
{   
public:
    Joint();
    Joint(uint8_t index, rapidxml::xml_node<>* nd);
    Joint(uint8_t index, IMotorDriver* motor, const char *name, double ratio = 1);
    virtual ~Joint() = default;
    const std::string &getName() const override { return name_; }
     uint8_t getIndex() const  { return index_; }
    void setVelocity(float target) override;
    void setPosition(float target) override;
    IMotorDriver* getMotorDriver() override;    
    bool getFeedback( FeedbackJoint *fb) const  override;
    const JointLimit& getLimits() const override { return limits_; }
    void setLimits( JointLimit& l ) override;
    void moveSteps( int steps ) override;
    void setStep( float step_pos ) override;
    const float getStep() override { return step_pos_; } 
    float getParameter( JOINT_MOTOR_PARAM type ) override;
    bool  setParameter( JOINT_MOTOR_PARAM type, float param ) override;
    void  saveParameter() override;
    bool isAcceptable(JOINT_MOTOR_PARAM type, float param )  override;
private:
    float velocityJointToMotor(float vel_jnt);
    float velocityMotorToJoint(float vel_jnt);
    float positionJointToMotor(float pos_jnt);
    bool readLimits( rapidxml::xml_node<> *jnt_node );

private:
    MotorDriverPtr motor_;
    double ratio_ = 1; // передаточное отношение редуктора
    std::string name_;    
    uint8_t index_;
    JointLimit limits_;
    float step_pos_ = 0.1f;  
};

extern MRobot g_robot_;
