#pragma once

#include "itracerecoder.h"
#include <vector>
#include <cstddef>
#include <chrono>
#include <atomic>
#include <thread>

using Clock = std::chrono::steady_clock;

class TraceRecorder : public ITraceRecorder
{
public:
    TraceRecorder();
    static TraceRecorder *getInstance();
    void startRecording() override;
    void stopRecording() override;
    void clear() override;
    size_t size() const override  { return trace_.size(); }
    void recordSegment(const JointsAngelses& angles) override;
    const TraceSegment& getSegment(size_t idx) const override;
    void playback() override;
    bool isRunning() { return isRecording_; }
protected:
    std::atomic<bool> isRecording_ = false;
    std::atomic<bool> isPlaying_ = false;
    std::vector<TraceSegment> trace_;
    std::thread recordingThread_;


};
