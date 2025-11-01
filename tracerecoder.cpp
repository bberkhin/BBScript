#include "tracerecoder.h"
#include "platform_util.h"
#include "mrobotimpl.h"

using Clock = std::chrono::steady_clock;

TraceRecorder *TraceRecorder::getInstance()
{
    static TraceRecorder recoder;
    return &recoder;

}

TraceRecorder::TraceRecorder() : 
    isRecording_(false) 
{

}

void TraceRecorder::startRecording()  
{
    if ( isRecording_ )
    {
        print_terminal(MSG_TYPE_DEBUG, "TraceRecorder already recording" );
        return;
    }
    isRecording_ = true;
    trace_.clear();
    // run thread fo reading data
    g_robot_.enableManualTeachMode(true);

    recordingThread_ = std::thread([this]() {
        JointsAngelses angles;
        while (isRecording_) 
        {
            g_robot_.getJointController()->getCurrentJointAngles( angles ); 
            recordSegment(angles);
            print_terminal(MSG_TYPE_DEBUG, "Do recording joint1 = %f joint2 = %f", angles.angles[0], angles.angles[1]);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        });

}

void TraceRecorder::stopRecording()  
{
    if ( !isRecording_ )
        return;
    isRecording_ = false;      
    if (recordingThread_.joinable())
        recordingThread_.join();

}

void TraceRecorder::clear()  
{    
    trace_.clear();
}

void TraceRecorder::recordSegment(const JointsAngelses& angles) 
{
    if( !isRecording_  )
        return;

    double nowSec = std::chrono::duration<double>(Clock::now().time_since_epoch()).count();

    JointsAngelses state = angles;

    if (!trace_.empty()) {
        const TraceSegment& prev = trace_.back();
        double dt = nowSec - prev.timestamp;
        if (dt > 0.0) {
            for (size_t i = 0; i < JOINTS_COUNT; ++i) {
                state.velocity[i] = (angles.angles[i] - prev.state.angles[i]) / dt;
            }
        }
    } else {
        for (size_t i = 0; i < JOINTS_COUNT; ++i) {
            state.velocity[i] = 0.0;
        }
    }

    trace_.emplace_back(TraceSegment{state, nowSec});
}

const TraceSegment& TraceRecorder::getSegment(size_t idx) const  
{    
    return trace_[idx];
}

void TraceRecorder::playback()
{
    for (const auto& seg : trace_) {
        g_robot_.movej(seg.state);
    }
}