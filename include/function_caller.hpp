#pragma once
#include <chrono>

#include "../external/angelscript/include/angelscript.h"
#include <string>
#include <variant>

#include "instance_handle.hpp"

namespace srph
{
namespace debugger
{
class Debugger;
}

class Engine;

// If the FunctionPolicy is Optional, the call will nor throw an error or proceed when the function is not found.
enum class FunctionPolicy
{
    Required = 0,
    Optional = 1
};
enum class ReturnType
{
    Byte,
    Word,
    QWord,
    DWord,
    Float,
    Double,
    Object
};

struct FunctionResult
{
    using ReturnType = std::variant<asBYTE, asWORD, asQWORD, asDWORD, float, double, asIScriptObject*>;
    ReturnType value;
};

class FunctionCaller
{
public:
    FunctionCaller(Engine* engine);
    ~FunctionCaller() = default;

    FunctionCaller& Module(const std::string& moduleName);
    FunctionCaller& Function(const std::string& functionSignature,
                             InstanceHandle instance = {},
                             FunctionPolicy policy = FunctionPolicy::Required);
    FunctionCaller& Factory(const std::string& factoryDecl, asITypeInfo* type);

    template <typename T>
    FunctionCaller& Push(T value)
    {
        asIScriptContext* context = GetContext();
        if constexpr (std::is_same<T, float>())
        {
            context->SetArgFloat(m_argIdx++, value);
        }
        else if constexpr (std::is_same<T, unsigned long>())
        {
            context->SetArgDWord(m_argIdx++, value);
        }
        else
        {
            context->SetArgObject(m_argIdx++, const_cast<void*>(static_cast<const void*>(&value)));
        }
        return *this;
    }

    void Call();
    [[nodiscard]] FunctionResult Call(ReturnType type);

    asIScriptContext* GetContext() const;

private:
    void LineCallback(asIScriptContext* context);

    // Context release, etc
    // Because we can early-out if the FunctionPolicty is Optional.
    void Cleanup();

private:
    friend class debugger::Debugger;
    Engine* m_engine = nullptr;
    asIScriptContext* m_context = nullptr;

    std::string m_moduleName;
    uint32_t m_argIdx = 0;

    std::string m_functionSignature;
    std::string m_instanceName;
    bool m_executionFinished = false;
    bool m_isOptional = false;

    std::chrono::steady_clock::time_point m_startTime;
    float m_timeoutMillis;
};
}  // namespace srph