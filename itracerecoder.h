#pragma once

#include "minterfaces.h" // JointsAngelses и типы интерфейсов

struct TraceSegment
{
    JointsAngelses state;    // angles[] и velocity[] для всех узлов
    double timestamp;        // время записи в секундах (от @steady_clock::now())
};

class ITraceRecorder
{
public:
    virtual ~ITraceRecorder() = default;
    virtual void startRecording() = 0;
    virtual void stopRecording() = 0;
    virtual void clear() = 0;
    virtual size_t size() const = 0;
    virtual void recordSegment(const JointsAngelses& angles) = 0; // только positions
    virtual const TraceSegment& getSegment(size_t idx) const = 0;
    virtual void playback() = 0;
};
