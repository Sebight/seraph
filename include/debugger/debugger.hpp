#pragma once
#include <mutex>
#include <unordered_map>

namespace srph
{
class Engine;
}

namespace srph::debugger
{

class IDebugAdapter;

enum class StepMode : uint8_t
{
    None = 0,
    Over,
    In,
    Out
};
class Debugger
{
public:
    Debugger() = default;
    ~Debugger();
    Debugger(IDebugAdapter* adapter, Engine* engine);

    bool Started() const { return m_started; }

    void Start();
    void RegisterLineCallback();

    std::unordered_map<std::string, std::vector<int>>& GetBreakpoints() { return m_breakpoints; }
    void Continue();

private:
    void LineCallback(asIScriptContext* context);
    bool IsBreakpoint(const std::string& file, int line);

private:
    friend class DAP;
    friend class FunctionCaller;

    IDebugAdapter* m_adapter;
    Engine* m_engine;

    bool m_started = false;

    std::unordered_map<std::string, std::vector<int>> m_breakpoints;
    std::unordered_map<std::string, std::string> m_normalizedPaths;

    std::mutex m_resumeMutex;
    std::condition_variable m_resumeCV;
    bool m_resumed = false;
    bool m_shouldStop = false;

    asIScriptContext* m_currentContext = nullptr;
    std::string m_currentFile;
    int m_currentLine = 0;
    asUINT m_currentStackDepth = 0;

    StepMode m_stepMode = StepMode::None;
};
}  // namespace srph::debugger