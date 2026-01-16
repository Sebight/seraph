#pragma once

namespace srph::debugger
{

class Debugger;

class IDebugAdapter
{
public:
    IDebugAdapter() = default;
    virtual ~IDebugAdapter() = default;

    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void AttachDebugger(Debugger* debugger) = 0;

    virtual void OnBreakpointHit(std::string file, int line) = 0;
};
}  // namespace srph::debugger