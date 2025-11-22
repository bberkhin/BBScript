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
    double dt = 0;
    if (!trace_.empty()) {
        const TraceSegment& prev = trace_.back();
        dt = nowSec - prev.timestamp;
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
    print_terminal(MSG_TYPE_DEBUG, "Do recording jnt1 = %f vel1 = %f time %f", angles.angles[0], angles.velocity[0], dt);
    trace_.emplace_back(TraceSegment{state, nowSec});
}

const TraceSegment& TraceRecorder::getSegment(size_t idx) const  
{    
    return trace_[idx];
}

void TraceRecorder::playback()
{

    if ( isPlaying_ )
    {
        print_terminal(MSG_TYPE_DEBUG, "TraceRecorder already playing" );
        return;
    }

    if ( isRecording_ )
    {
        print_terminal(MSG_TYPE_DEBUG, "Can not satrt playing while recording" );
        return;
    }

    isPlaying_ = true;
    g_robot_.enableManualTeachMode(false);
    
    if ( trace_.empty() )
    {
        //debug
        TraceSegment tr;
        tr.state.angles[0] = 2.f;
        tr.state.velocity[0] = 3.f;
        trace_.push_back(tr);
        tr.state.angles[0] = -2.f;
        tr.state.velocity[0] = 3.f;
        trace_.push_back(tr);
    }

    print_terminal(MSG_TYPE_DEBUG, "Start playing!! Segments %d", trace_.size() );

    std::thread playigThread_ = std::thread([this]() {
        for (const auto& seg : trace_) {
            if ( !isPlaying_ )
                break;
            g_robot_.movej(seg.state);
            //sleep to get time to start
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            // wait until zero velocity
            while ( isPlaying_ ) // other process can stop
            {
                if ( !g_robot_.getFeedBackHandler()->isMoving() )
                    break;

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } 
        isPlaying_ = false;
        print_terminal(MSG_TYPE_DEBUG, "Stop playing!! Segments %d", trace_.size() );
    });
    
    playigThread_.detach();
}