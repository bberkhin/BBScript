#pragma once
#include "minterfaces.h"
#include <vector>
#include <thread>
#include <mutex>

class MRobot; // forward declaration to allow pointer usage without full type here
// this class get beedback from motor each tick_timeout_ , and call tick function of motors each tick_timeout_
class FeedBackHandler
{
    typedef std::vector<FeedbackJoint> FeedbackListData;
public:    
    FeedBackHandler(MRobot *rb);
    void start();
    bool isMoving();
    bool getFeedback(int index, FeedbackJoint *cb);
    bool setFeedback(const FeedbackJoint &cb );
    void setTimeouts( uint8_t fb_timeout, uint8_t tick_timeout ) { tick_timeout_ = tick_timeout;  }
private:
    std::mutex mtx_;
    FeedbackListData  fblist_;
    MRobot *robot_ = nullptr;
    uint8_t tick_timeout_ = 25;
};