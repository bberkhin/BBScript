#include <stdio.h>
#include <string>
#include <stdbool.h>
#include <cstdarg>
#include <stdlib.h>
#include "mrobotimpl.h"
#include "script_bind.h"
#include "platform_util.h"

extern MRobot g_robot_;

static IMotorDriver *getMotor(int index)
{
    IJointController* jctrl = g_robot_.getJointController();
    JointPtr jnt = jctrl->getJointByIndex(index); // jusr numver [1....n]
    if ( !jnt )
        return nullptr;
    return jnt->getMotorDriver();
}

static JointPtr getJointByIndex(int index)
{
    IJointController* jctrl = g_robot_.getJointController();
    return jctrl->getJointByIndex(index); 
}

extern "C" void mr_stop() {
    g_robot_.stop();
}

extern "C" double mr_getvoltag(int id) {
    // Function implementation
    IMotorDriver *motor = getMotor(id);
    if ( !motor )
    return 0.0;
    motor->request( FB_VBUS,  100 );
    return motor->getVoltage();
}



extern "C" void mr_setvel(int index, double v) {    
    JointPtr jnt = getJointByIndex(index);
    if (  jnt )
        jnt->setVelocity(v);
}

extern "C" void mr_calibrate( int index ) {
    IMotorDriver *motor = getMotor(index);
    if (  motor )
        motor->calibrate();
} 

extern "C" void mr_clearerrors(int index) {
    IMotorDriver *motor = getMotor(index);
    if (  motor )
        motor->clearErrors();
} 

extern "C" void mr_closeloop(int index) {
    IMotorDriver *motor = getMotor(index);
    if (  motor )
        motor->setCloseLoop();
} 

extern "C" void mr_setidle(int index) {
    IMotorDriver *motor = getMotor(index);
    if (  motor )
        motor->setStateIdle();
}       

extern "C" void mr_setmodepos(int index) {
     IMotorDriver *motor = getMotor(index);
    if (  motor )
        motor->setControlModePosition();
}

extern "C" void mr_setmodevel(int index) {
    IMotorDriver *motor = getMotor(index);
    if (  motor )
        motor->setControlModeVelocity();        
}

extern "C" void mr_setpos(int index, double pos) {
    JointPtr jnt = getJointByIndex(index);
    if (  jnt )
        jnt->setPosition(pos);
} 


extern "C" double mr_getpos(int index) {
    FeedbackJoint fb;
    JointPtr jnt = getJointByIndex(index);
    if (  jnt )
    {
        jnt->getFeedback(&fb);
        return fb.pos;
    }
    return  0.;
}      

extern "C" double mr_getvel(int index) {
    FeedbackJoint fb;
    JointPtr jnt = getJointByIndex(index);
    if (  jnt )
    {
        jnt->getFeedback(&fb);
        return fb.vel;
    }
    return  0.;
} 

extern "C" void mr_delay_ms(int time_ms) {
    delay_ms(time_ms);
}   




