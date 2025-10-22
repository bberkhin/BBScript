#pragma once

#include "minterfaces.h"
#include <string>
#include <vector>
#include <functional>
#include <rapidxml.hpp>




typedef std::vector<JointPtr> JointsList;

MotorDriverPtr createMotorDriver(const char *motor_type); 

class MRobot : public ICollaborativeRobot
{
    public:
        MRobot();
        MRobot( IRobotModel *model, IJointController *jointController );
        ~MRobot() override;
        void movep(const Pose& target) override;
        void movej(const JointsAngelses &target) override;
        void stop() override;
        void emergencyStop() override;
        IRobotModel* getModel() override;
        IJointController* getJointController() override;
        const JointsAngelses &getCurrentJointAngles() const override;
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
        JointsAngelses currentJointAngles_;
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
    void setJointTargets(const JointsAngelses &targets) override;
    void update() override;
    JointPtr addJoint(uint8_t id, const char *motor_type, const char *name, double ratio = 1) override;
    JointPtr getJointByMotor(IMotorDriver *p) override;    
    JointPtr getJointByIndex(uint8_t index) override;
    int getJointsCount() override;
    void requestAll( uint16_t what, uint16_t wait_ms ) override;
    // Iterate joints with a functor: non-const and const versions
    void for_each_joint(const JointPtrFunctor &fn);
  //  void for_each_joint_const(const JointPtrFunctor &fn) const;
private:
    JointsAngelses targetAngles_;
    JointsList joints_;
};

// Интерфейс сустава (joint)
class Joint : public IJoint
{   
public:
    Joint();
    Joint(uint8_t motor_id, const char *motor_type, const char *name, double ratio = 1);
    Joint(IMotorDriver* motor, const char *name, double ratio = 1);
    virtual ~Joint() = default;
    const std::string &getName() const override { return name_; }
    void setVelocity(float target) override;
    void setPosition(float target) override;
    bool getFeedback( FeedbackJoint *fb)  override;
    IMotorDriver* getMotorDriver() override;    
private:
    MotorDriverPtr motor_;
    double ratio_ = 1; // передаточное отношение редуктора
    std::string name_;    
};

