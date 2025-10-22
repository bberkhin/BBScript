#include <stdio.h>
#include <iostream>

#include "mrobotimpl.h"
#include <cstring>
#include "platform_util.h"
#include "motors_facoty.h"
#include "canmotorimpl.h"



MRobot::MRobot()
{
    model_ = std::make_shared<RobotModel>();
    jointController_ = std::make_shared<JointController>();
} 

MRobot::MRobot(IRobotModel *model, IJointController *jointController)
{
    model_.reset(model);
    jointController_.reset(jointController);    
}

MRobot::~MRobot()
{
}



bool MRobot::Load(const char *urdfdata)
{
    std::vector<char> xml_buffer;
    xml_buffer.assign(urdfdata, urdfdata + strlen(urdfdata) + 1);
    rapidxml::xml_document<> doc;   

    doc.parse<0>(&xml_buffer[0]);
    // Находим корневой элемент <robot>
    auto* robot_node = doc.first_node("robot");

    if (!robot_node) {
        throw rapidxml::parse_error("XML can not contain <robot>", nullptr);
    }
    return Load( robot_node );
}


bool MRobot::Load(rapidxml::xml_node<> *robot_node)
{
    std::string joint_name;
    std::string joint_motor;
    double ratio;
    uint8_t id = 0;
    // Выводим имя робота
    if (auto* name = robot_node->first_attribute("name")) {
        print_terminal ("Robot name: %s\n", name->value());
    }

    
    // Пробегаемся по элементам <joint>
    for (auto* joint_node = robot_node->first_node("joint"); joint_node;joint_node = joint_node->next_sibling("joint")) {
        
        if (auto* xjoint_name = joint_node->first_attribute("name")) 
            joint_name = xjoint_name->value();
        else
            joint_name = "joint";
        if (auto* xjoint_m = joint_node->first_attribute("motor")) 
            joint_motor = xjoint_m->value();
        else
            joint_motor = "can_odrv";
        if (auto* xjoint_r = joint_node->first_attribute("ratio")) 
            ratio = atof(xjoint_r->value());
        else
            ratio = 1;
    
        if (auto* xjoint_id = joint_node->first_attribute("id")) 
            id = atoi(xjoint_id->value());
        else
            id++;

        print_terminal ("Joint: %s type: %s ratio: %f \n",joint_name.c_str(), joint_motor.c_str(), ratio );
        jointController_->addJoint(id, joint_motor.c_str(),joint_name.c_str(), ratio);
    }
    return true;
}

bool MRobot::pumpEvents(ICanInterface *can)
{
    static int max_events = 10;
    static uint32_t id;
    static uint8_t buffer[8];
    static uint8_t len;

    bool ret = false;
      // max_events prevents an infinite loop if messages come at a high rate
    while (  can->readMsgBufID(&id, &len, buffer) && max_events--) {       
        IJointController* jctrl = getJointController();
        for( int i=0; i<= jctrl->getJointsCount(); i++ )
        {
            JointPtr jnt = jctrl->getJointByIndex(i);
            if ( jnt )
            {
                CanMotorDriver *motor = dynamic_cast<CanMotorDriver *>( jnt->getMotorDriver()) ;
                if ( motor && motor->onCanMessage(id, len, buffer ) )
                    ret = true;
            }
        }        
    }
    return ret;
}


//todo: реализовать методы
void MRobot::movep(const Pose & )///target)
{
}

void MRobot::movej(const JointsAngelses &target)
{
    if(jointController_)
    {
        jointController_->setJointTargets(target);
        jointController_->update();
        currentJointAngles_ = target;
    }
}
void MRobot::stop()
{
    // Stop all motors by iterating joints and calling stop() on each motor driver
    for_each_joint([](JointPtr &j, uint8_t ){
        if (!j) return;
        IMotorDriver *m = j->getMotorDriver();
        if (m) m->stop();
    });
}

void MRobot::emergencyStop()
{
    stop();
}   

IRobotModel* MRobot::getModel()
{
    return model_.get();
}

IJointController* MRobot::getJointController()
{
    return jointController_.get();
}
const JointsAngelses &MRobot::getCurrentJointAngles() const
{
    return currentJointAngles_;
}

// MRobot convenience wrappers that forward to the JointController
void MRobot::for_each_joint(const JointPtrFunctor &fn)
{
    if (!jointController_) 
        return;
    jointController_->for_each_joint(fn);
}
/*
void MRobot::for_each_joint_const(const JointPtrFunctor &fn) const
{
    if (!jointController_) 
        return;
    jointController_->for_each_joint_const(fn);
    
}
*/
//////////////////////////////////////////////////////
// RobotModel implementation

RobotModel::RobotModel()
{
}

//////////////////////////////////////////////////////
// JointController implementation

JointController::JointController()
{
}

void JointController::setJointTargets(const JointsAngelses &targets)
{
for_each_joint([&targets](JointPtr &j, uint8_t index ){
        if (!j) return;
        IMotorDriver *m = j->getMotorDriver();
        if (m) m->setPosition(targets.angles[index], targets.velocity[index] );  } );
}

void JointController::update()
{
}

void JointController::for_each_joint(const JointPtrFunctor &fn)
{
    if (!fn) return;
    for( uint8_t index = 0; index < joints_.size(); index++  )
         fn( joints_[index], index);
}
/*
void JointController::for_each_joint_const(const JointPtrFunctor &fn) const
{
    if (!fn) return;
    for (const auto &j : joints_) {
        fn(j);
    }
}

*/
JointPtr JointController::addJoint( uint8_t id,  const char *motor_type, const char *name, double ratio)
{
    JointPtr jnt = std::make_shared<Joint>(id, motor_type, name, ratio);
    joints_.push_back(jnt);
    return jnt;    
}


JointPtr JointController::getJointByIndex(uint8_t index)
{
    if ( index >= 0 && index < joints_.size() )    
        return joints_[index];
    return JointPtr();
}

int JointController::getJointsCount()
{
    return joints_.size();
}

JointPtr JointController::getJointByMotor( IMotorDriver *p )
{
    for ( auto jnt : joints_ )
    {
        if ( jnt->getMotorDriver() == p )
            return jnt;
    }
    return JointPtr();
}



void JointController::requestAll( uint16_t what, uint16_t wait_ms )
{
    for ( auto jnt : joints_ )
    {
        IMotorDriver *mtr = jnt->getMotorDriver();
        if ( mtr )
            mtr->request(what, wait_ms);
    }
}

//////////////////////////////////////////////////////
// Joint implementation

Joint::Joint()
{
}

Joint::Joint(uint8_t motor_id, const char *motor_type, const char *name, double ratio) : 
     name_(name && *name ? name : "joint"), ratio_(ratio)
{
    if ( motor_type != nullptr && *motor_type != 0 )
        motor_ = createMotorDriver(motor_type, motor_id); 
}

Joint::Joint( IMotorDriver *motor, const char *name, double ratio) : 
    name_(name && *name ? name : "joint"), ratio_(ratio)
{
    motor_.reset(motor);    
}


void Joint::setVelocity(float target)
{
    if(motor_)
    {
        motor_->setVelocity(target * ratio_);
    }
}

void Joint::setPosition(float target)
{
    if(motor_)
    {
        motor_->setPosition(target * ratio_, 10);
    }
}

IMotorDriver* Joint::getMotorDriver()
{
    return motor_.get();
}


bool Joint::getFeedback( FeedbackJoint *fb) 
{
    if ( motor_  )
    {
        fb->id = motor_->getId();
        fb->pos = motor_->getPosition() / ratio_;
        fb->vel = motor_->getVelocity() / ratio_;
        return true;
    }
    else
    {
        fb->id = 0;
        fb->pos = 0.;
        fb->vel = 0.;
    }
    return false;
}
