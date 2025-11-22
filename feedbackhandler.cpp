#include "feedbackhandler.h"
#include <mutex>
#include "pcanimpl.h"
#include "mrobotimpl.h"



void get_feedback_callback(IMotorDriver *motor,  uint16_t type)
{ 

    FeedBackHandler *fbh = g_robot_.getFeedBackHandler();
    IJointController* jctrl = g_robot_.getJointController();   
    JointPtr jnt = jctrl->getJointByMotor(motor);

    if ( jnt && (type&FB_POSITION) )
    {
        FeedbackJoint data;
        jnt->getFeedback(&data);
        fbh->setFeedback(data);
    }
}


void feedback_reader_thread( uint8_t timeout ) 
{
    IJointController* jctrl = g_robot_.getJointController();    
    PCanImpl *can = PCanImpl::getGlobalCan();
    double pos = 0, vel = 0;
    while (true) {       
        
        if ( can->isOpen() )
                g_robot_.pumpEvents(can);
        jctrl->requestAll(FB_POSITION, 0);  

        jctrl->tickAll();

        // Чтобы уменьшить нагрузку
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
    }
}


FeedBackHandler::FeedBackHandler(MRobot *rb) :
    robot_(rb)
{
    FeedbackJoint fbInit;
    fblist_.assign(JOINTS_COUNT, fbInit);
}

void FeedBackHandler::start()
{
    IMotorDriver::setFeedbackCallback( get_feedback_callback );
    std::thread reader(feedback_reader_thread, tick_timeout_ ); 
    reader.detach();
}

bool FeedBackHandler::isMoving()
{
    std::lock_guard<std::mutex> lock(mtx_);
    for ( const auto &fb : fblist_ )
    {
        if ( fabs( fb.vel ) > 0.1  )
            return true;
    }
    return false;
}

bool FeedBackHandler::setFeedback(const FeedbackJoint &cb )
{
    if ( cb.index < 0 || cb.index >= robot_->getJointController()->getJointsCount() )
        return false;

    std::lock_guard<std::mutex> lock(mtx_);    
    fblist_[cb.index] = cb;
    return true;
}

bool FeedBackHandler::getFeedback(int index, FeedbackJoint *cb )
{
    if ( index < 0 || index >= robot_->getJointController()->getJointsCount() )
        return false;
    
    std::lock_guard<std::mutex> lock(mtx_);
    *cb = fblist_[index];
    return false;
}
