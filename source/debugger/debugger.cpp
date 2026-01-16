#include "srph_common.hpp"
#include "debugger/debugger.hpp"

#include "engine.hpp"
#include "function_caller.hpp"
#include "helpers.hpp"
#include "debugger/debug_adapter.hpp"

namespace srph
{
class Engine;
}

srph::debugger::Debugger::~Debugger()
{
    m_adapter->Stop();
    delete m_adapter;
}

srph::debugger::Debugger::Debugger(IDebugAdapter* adapter, Engine* engine)
{
    m_adapter = adapter;
    m_adapter->AttachDebugger(this);
    m_engine = engine;
}

void srph::debugger::Debugger::Start()
{
    m_started = true;
    m_adapter->Start();
}

void srph::debugger::Debugger::RegisterLineCallback()
{
    m_engine->RegisterLineCallback("debugger", [this](asIScriptContext* context) { LineCallback(context); });
}

void srph::debugger::Debugger::Continue()
{
    std::lock_guard<std::mutex> lock(m_resumeMutex);
    m_resumed = true;
    m_resumeCV.notify_one();
}

void srph::debugger::Debugger::LineCallback(asIScriptContext* context)
{
    const char* scriptSection;
    int line = context->GetLineNumber(0, nullptr, &scriptSection);

    std::string sectionStr = scriptSection;
    auto it = m_normalizedPaths.find(sectionStr);
    if (it == m_normalizedPaths.end())
    {
        std::string normalized = sectionStr;
        std::transform(normalized.begin(),
                       normalized.end(),
                       normalized.begin(),
                       [](char c) { return static_cast<char>(c == '\\' ? '/' : std::tolower(static_cast<unsigned char>(c))); });
        it = m_normalizedPaths.emplace(std::move(sectionStr), std::move(normalized)).first;
    }

    const std::string& normalized = it->second;

    if (m_stepMode == StepMode::Over)
    {
        if (m_currentContext->GetCallstackSize() <= m_currentStackDepth)
        {
            m_shouldStop = true;
            m_stepMode = StepMode::None;
        }
    }
    else if (m_stepMode == StepMode::In)
    {
        m_shouldStop = true;
        m_stepMode = StepMode::None;
    }
    else if (m_stepMode == StepMode::Out)
    {
        if (m_currentContext->GetCallstackSize() <= m_currentStackDepth)
        {
            m_shouldStop = true;
            m_stepMode = StepMode::None;
        }
    }
    else if (IsBreakpoint(normalized, line))
    {
        m_shouldStop = true;
    }

    if (m_shouldStop)
    {
        m_shouldStop = false;

        m_currentContext = context;
        m_currentFile = scriptSection;
        m_currentLine = line;

        m_adapter->OnBreakpointHit(scriptSection, line);

        std::unique_lock<std::mutex> lock(m_resumeMutex);
        m_resumed = false;
        m_resumeCV.wait(lock, [this] { return m_resumed; });

        // Note(Seb): This is a bit of an oddity, but it makes sense... I need to reset the function timeout timer, since it has
        // definitely timed out after hitting a breakpoint
        m_engine->m_currentFunctionCaller->m_startTime = std::chrono::steady_clock::now();
    }
}

bool srph::debugger::Debugger::IsBreakpoint(const std::string& file, int line)
{
    if (m_breakpoints.find(file) == m_breakpoints.end()) return false;

    auto& lines = m_breakpoints.at(file);

    return std::find(lines.begin(), lines.end(), line) != lines.end();
}