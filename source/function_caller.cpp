#include "srph_common.hpp"
#include "function_caller.hpp"

#include <chrono>

#include "engine.hpp"
#include "debugger/debugger.hpp"

srph::FunctionCaller::FunctionCaller(Engine* engine)
{
    m_engine = engine;
    m_context = m_engine->GetContext();
}

srph::FunctionCaller& srph::FunctionCaller::Module(const std::string& moduleName)
{
    m_moduleName = moduleName;
    return *this;
}

srph::FunctionCaller& srph::FunctionCaller::Function(const std::string& functionSignature,
                                                     InstanceHandle instance,
                                                     FunctionPolicy policy)
{
    if (!m_engine->m_built) return *this;

    asIScriptModule* module = m_engine->GetModule(m_moduleName);

    asIScriptFunction* func = nullptr;
    asIScriptObject* self = nullptr;

    if (instance.Valid())
    {
        self = m_engine->m_instances.at(instance);
        asITypeInfo* type = self->GetObjectType();
        func = m_engine->GetMethod(type, functionSignature);
        m_instanceName = type->GetName();
    }
    else
    {
        func = m_engine->GetFunction(module, functionSignature);
    }

    if (func == nullptr)
    {
        if (policy == FunctionPolicy::Optional)
        {
            m_isOptional = true;
            return *this;
        }

        if (instance.Valid())
        {
            Log::Error("Method with signature {} was not on class {}.", functionSignature, m_engine->GetTypeName(instance));
        }
        else
        {
            Log::Error("Function with signature {} was not found in the module.", functionSignature);
        }
    }

    SRPH_VERIFY(m_context->Prepare(func), "Failed to prepare for function call.")

    if (instance.Valid())
    {
        m_context->SetObject(self);
    }

    m_functionSignature = functionSignature;

    return *this;
}

srph::FunctionCaller& srph::FunctionCaller::Factory(const std::string& factoryDecl, asITypeInfo* type)
{
    if (!m_engine->m_built) return *this;

    asIScriptFunction* factory = type->GetFactoryByDecl(factoryDecl.c_str());
    if (factory == nullptr)
    {
        Log::Error("Constructor with signature {} was not found on {}.", factoryDecl, type->GetName());
    }
    SRPH_VERIFY(m_context->Prepare(factory), "Failed to prepare for factory call.")

    return *this;
}

void srph::FunctionCaller::Call()
{
    if (!m_engine->m_built) return;

    if (m_isOptional)
    {
        Cleanup();
        return;
    }

    m_engine->m_currentFunctionCaller = this;

    m_startTime = std::chrono::steady_clock::now();
    m_timeoutMillis = m_engine->m_configuration.scriptTimeoutMillis;

    m_engine->RegisterLineCallback(m_functionSignature, [this](asIScriptContext* context) { LineCallback(context); });

    int result = m_context->Execute();
    if (result != asEXECUTION_FINISHED)
    {
        if (result == asEXECUTION_EXCEPTION)
        {
            const char* exceptionString = m_context->GetExceptionString();
            const char* sectionName;
            int columnNumber = 0;
            int lineNumber = m_context->GetExceptionLineNumber(&columnNumber, &sectionName);

            if (m_instanceName.empty())
            {
                Log::Error("Exception '{}' in {}:{},{} while calling function {}.",
                           exceptionString,
                           sectionName,
                           lineNumber,
                           columnNumber,
                           m_functionSignature);
            }
            else
            {
                Log::Error("Exception '{}' in {}:{},{} while calling method {}::{}.",
                           exceptionString,
                           sectionName,
                           lineNumber,
                           columnNumber,
                           m_instanceName,
                           m_functionSignature);
            }
        }
    }

    m_executionFinished = true;

    Cleanup();
}

srph::FunctionResult srph::FunctionCaller::Call(ReturnType type)
{
    if (!m_engine->m_built || m_isOptional) return {};

    m_engine->m_currentFunctionCaller = this;

    m_startTime = std::chrono::steady_clock::now();
    m_timeoutMillis = m_engine->m_configuration.scriptTimeoutMillis;

    m_engine->RegisterLineCallback(m_functionSignature, [this](asIScriptContext* context) { LineCallback(context); });

    int result = m_context->Execute();
    if (result != asEXECUTION_FINISHED)
    {
        if (result == asEXECUTION_EXCEPTION)
        {
            const char* exceptionString = m_context->GetExceptionString();
            const char* sectionName;
            int columnNumber = 0;
            int lineNumber = m_context->GetExceptionLineNumber(&columnNumber, &sectionName);

            if (m_instanceName.empty())
            {
                Log::Error("Exception '{}' in {}:{},{} while calling function {}.",
                           exceptionString,
                           sectionName,
                           lineNumber,
                           columnNumber,
                           m_functionSignature);
            }
            else
            {
                Log::Error("Exception '{}' in {}:{},{} while calling method {}::{}.",
                           exceptionString,
                           sectionName,
                           lineNumber,
                           columnNumber,
                           m_instanceName,
                           m_functionSignature);
            }
        }
    }

    m_executionFinished = true;

    FunctionResult res = {};
    if (type == ReturnType::Object)
    {
        // Note(Seb): Calling AddRef here makes the pointer valid after the release of the context. It is a bit dangerous, but I
        // keep the Release up to the user of this function.
        asIScriptObject* obj = *static_cast<asIScriptObject**>(m_context->GetAddressOfReturnValue());
        obj->AddRef();
        res.value = obj;
    }

    Cleanup();

    return res;
}

asIScriptContext* srph::FunctionCaller::GetContext() const { return m_context; }

void srph::FunctionCaller::LineCallback(asIScriptContext* context)
{
    if (m_executionFinished) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();
    if (elapsed > m_timeoutMillis)
    {
        Log::Info("Function {} timed out!", m_functionSignature);
        context->Abort();
        context->Unprepare();
        if (m_engine->m_timeoutCallback)
        {
            m_engine->m_timeoutCallback();
        }
    }
}

void srph::FunctionCaller::Cleanup()
{
    m_engine->RemoveLineCallback(m_functionSignature);
    m_engine->ReleaseContext(m_context);
}