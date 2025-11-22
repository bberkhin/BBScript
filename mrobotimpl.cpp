#include <stdio.h>
#include <iostream>

#include "mrobotimpl.h"
#include <cstring>
#include "platform_util.h"
#include "motors_facoty.h"
#include "canmotorimpl.h"



MRobot::MRobot() : fbhandler_(this)
{
    model_ = std::make_shared<RobotModel>();
    jointController_ = std::make_shared<JointController>();
} 

MRobot::MRobot(IRobotModel *model, IJointController *jointController) : fbhandler_(this)
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
        print_terminal (MSG_TYPE_DEBUG,"Robot name: %s", name->value());
    }

    JointController * jc = dynamic_cast<JointController *>(jointController_.get());
    
    // Пробегаемся по элементам <joint>
    for (auto* joint_node = robot_node->first_node("joint"); joint_node;joint_node = joint_node->next_sibling("joint")) {        
        jc->addJoint(joint_node);        
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
    for_each_joint([&target](JointPtr &j, uint8_t index ){
        if (!j || isnan(target.angles[index])  )
            return;
        j->setPosition( target.angles[index] );
        print_terminal(MSG_TYPE_DEBUG, "move %s to %f", j->getName().c_str(), target.angles[index] );
        } );

}
void MRobot::enableManualTeachMode(bool enable)
{
        // setidlemode for each motor driver
    for_each_joint([enable](JointPtr &j, uint8_t ){
        if (!j) return;
        IMotorDriver *m = j->getMotorDriver();
        if ( m )
        {
            if ( enable ) { 
                m->setStateIdle(); 
            }
            else {
                m->setCloseLoop();
                m->setControlModePosition();
            }
        }
    });
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


void JointController::getCurrentJointAngles( JointsAngelses &ja) {

     for_each_joint([&ja](JointPtr &j, uint8_t index ){
        if (!j) return;
        FeedbackJoint fb;
        j->getFeedback(&fb);
        ja.angles[index] = fb.pos;
        ja.velocity[index] = fb.vel;
        } );

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
JointPtr JointController::addJoint(rapidxml::xml_node<>* nd)
{
    JointPtr jnt = std::make_shared<Joint>(int8_t(joints_.size()), nd );
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

void JointController::tickAll()
{
    for ( auto jnt : joints_ )
    {
        IMotorDriver *mtr = jnt->getMotorDriver();
        if ( mtr )
            mtr->tick();
    }
}


//////////////////////////////////////////////////////
// Joint implementation

Joint::Joint()
{
}

Joint::Joint(uint8_t index, rapidxml::xml_node<>* joint_node) : 
     index_(index)
{

    std::string motor_type;
    static int motor_id = 0;
    if (auto* xjoint_name = joint_node->first_attribute("name")) 
        name_ = xjoint_name->value();
    else
        name_ = "joint";
    if (auto* xjoint_m = joint_node->first_attribute("motor")) 
        motor_type = xjoint_m->value();
    else
        motor_type = "can_odrv";
    if (auto* xjoint_r = joint_node->first_attribute("ratio")) 
        ratio_ = atof(xjoint_r->value());
    else
        ratio_ = 1;

    if (auto* xjoint_id = joint_node->first_attribute("id")) 
        motor_id = atoi(xjoint_id->value());
    else
        motor_id++;

     if (auto* xjoint_s = joint_node->first_attribute("step")) 
        step_pos_ = atof(xjoint_s->value());

    // read limits
    readLimits( joint_node );

    if ( !motor_type.empty()  )
        motor_ = createMotorDriver(motor_type.c_str(), motor_id); 
    
    print_terminal (MSG_TYPE_DEBUG, "Joint: %s type: %s ratio: %f",name_.c_str(), motor_type.c_str(), ratio_ );
}

Joint::Joint( uint8_t index, IMotorDriver *motor, const char *name, double ratio) : 
    index_(index), name_(name && *name ? name : "joint"), ratio_(ratio)
{
    motor_.reset(motor);    
}

bool Joint::readLimits( rapidxml::xml_node<> *jnt_node )
{
//   <limit lower="-10" upper="10" effort="10" velocity="5"/>
    auto* limit_node = jnt_node->first_node("limit");
    if ( !limit_node )
        return false;

    rapidxml::xml_attribute<> *a = limit_node->first_attribute("lower");
    if ( a ) 
        limits_.pos_min_ = atof( a->value() );
   
    a = limit_node->first_attribute("upper");
    if ( a ) 
        limits_.pos_max_ = atof( a->value() ) ;
   
        a = limit_node->first_attribute("effort");
    if ( a ) 
        limits_.torq_max_ = atof( a->value() );
   
        a = limit_node->first_attribute("velocity");
    if ( a ) 
        limits_.vel_max_ = atof( a->value() );
    
        a = limit_node->first_attribute("acc");   
    if ( a )     
        limits_.acc_max_ = atof( a->value());

    a = limit_node->first_attribute("current");
    if ( a ) 
        limits_.cur_max_ = atof( a->value());

    return true;      
}


float Joint::velocityJointToMotor(float veljnt)
{
    return veljnt * ratio_;
}

float Joint::velocityMotorToJoint(float vel_motor)
{
    return vel_motor / ratio_;
}


float Joint::positionJointToMotor(float pos_jnt)
{
    return pos_jnt * ratio_;
}

void Joint::setVelocity(float veljnt)
{
    if(motor_)
    {
        if ( limits_.vel_max_ > 0.0f)
            veljnt = std::min( veljnt, limits_.vel_max_ );
        motor_->setVelocity(velocityJointToMotor(veljnt));
    }
}

void Joint::setPosition(float target)
{
    if(motor_)
    {
        float actual_vel = ( limits_.vel_max_ > 0.0f) ? limits_.vel_max_ : LIMIT_DEFAULT_VEL;
        if ( limits_.pos_min_ != 0.0f)
            target = ( target < limits_.pos_min_) ? limits_.pos_min_ : target;
        if ( limits_.pos_max_ != 0.0f)
            target = ( target > limits_.pos_max_) ? limits_.pos_max_ : target;
        motor_->setPosition( positionJointToMotor(target), actual_vel );
    }
}

IMotorDriver* Joint::getMotorDriver()
{
    return motor_.get();
}


bool Joint::getFeedback( FeedbackJoint *fb) const
{
    fb->index = index_;
    if ( motor_  )
    {
        fb->pos = motor_->getPosition() / ratio_;
        fb->vel = motor_->getVelocity() / ratio_;
        return true;
    }
    else
    {
        fb->pos = 0.;
        fb->vel = 0.;
    }
    return false;
}

void Joint::setLimits(JointLimit &l)
{
    limits_ = l;
}



void Joint::moveSteps( int steps ) 
{
    if ( !motor_ || steps == 0 )
        return;
    FeedbackJoint fb;    
    getFeedback( &fb);
    setPosition( fb.pos + step_pos_*steps );
}

void Joint::setStep( float step_pos )
{
    if ( step_pos != 0.f)
        step_pos_ = fabs(step_pos);
}


float Joint::getParameter( JOINT_MOTOR_PARAM type )
{
    if ( !motor_ )
        return 0.f;
    float param = motor_->getParameter(type);
    if ( type == JOINT_MOTOR_SPEEDLIMIT || type == JOINT_MOTOR_ACCLIMIT )
        param = velocityMotorToJoint(param);
    return param;

}
bool  Joint::setParameter( JOINT_MOTOR_PARAM type, float param )
{
    if ( !motor_ || !isAcceptable(type, param) )
        return false;

    if ( type == JOINT_MOTOR_SPEEDLIMIT || type == JOINT_MOTOR_ACCLIMIT )
        param = velocityJointToMotor(param);
    motor_->setParameter(type, param);
    return true;

}

void Joint::saveParameter()
{
    if ( motor_ )
        motor_->saveParameter();
}

bool Joint::isAcceptable(JOINT_MOTOR_PARAM type, float param)
{
    switch( type )
    {
        case JOINT_MOTOR_SPEEDLIMIT: 
            return ( param >= 0.1 && param <= limits_.vel_max_ );
        case JOINT_MOTOR_ACCLIMIT: 
            return ( param >= 0.1 && param <= limits_.acc_max_ );
        case JOINT_MOTOR_CURLIMIT:
            return ( param >= 0.1 && param <= limits_.cur_max_ );
    }
    return true;
}

