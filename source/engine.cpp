#include "srph_common.hpp"
#include "engine.hpp"
#include "debugger/dap.hpp"

#include "helpers.hpp"

#include <random>

#include "function_caller.hpp"
#include "debugger/debugger.hpp"

void srph::Engine::Initialize(EngineConfiguration configuration)
{
    Log::Info("Initializing Seraph.");
    m_engine = asCreateScriptEngine();
    m_configuration = configuration;

    if (!m_engine)
    {
        Log::Critical("Failed to create AngelScript engine.");
    }

    SRPH_VERIFY(m_engine->SetMessageCallback(asMETHOD(Engine, MessageCallback), this, asCALL_THISCALL),
                "Failed to set message callback")

    RegisterAddOns();

    SRPH_VERIFY(m_engine->RegisterGlobalFunction("void print(const string& in)",
                                                 asMETHOD(Engine, Print),
                                                 asCALL_THISCALL_ASGLOBAL,
                                                 this),
                "Failed to register print internal call.")

    m_context = m_engine->CreateContext();

    SRPH_VERIFY(m_context->SetLineCallback(asMETHOD(Engine, LineCallback), this, asCALL_THISCALL),
                "Could not set line callback.")
}

void srph::Engine::AttachDebugger()
{
    if (!m_debugger)
    {
        m_debugger = new debugger::Debugger(new debugger::DAP(), this);

        if (!m_debugger->Started())
        {
            m_debugger->Start();
        }
    }

    m_debugger->RegisterLineCallback();
}

void srph::Engine::RegisterLineCallback(const std::string& key, const std::function<void(asIScriptContext* context)>& f)
{
    m_lineCallbacks[key] = f;
}

void srph::Engine::RemoveLineCallback(const std::string& key) { m_lineCallbacks.erase(key); }

void srph::Engine::Shutdown()
{
    m_engine->DiscardModule("Game");
    for (auto& instance : m_instances)
    {
        instance.second->Release();
    }

    m_instances.clear();
    m_metadata.clear();

    for (auto& ctx : m_contexts)
    {
        ctx->Release();
    }

    m_functionCache.clear();
    m_moduleCache.clear();

    m_contexts.clear();
    m_context->Release();
    m_engine->Release();
}

void srph::Engine::RegisterAddOns() const
{
    RegisterStdString(m_engine);
    RegisterScriptArray(m_engine, true);
}

std::vector<srph::InstanceHandle> srph::Engine::GetInstances() const
{
    std::vector<InstanceHandle> out;
    out.reserve(m_instances.size());
    for (auto& instance : m_instances)
    {
        out.push_back(instance.first);
    }

    return out;
}

std::vector<std::string> srph::Engine::QueryDerivedClasses(const std::string& baseClass) const
{
    std::vector<std::string> out = {};

    // TODO(Seb): Read module from somewhere
    asIScriptModule* module = m_engine->GetModule("Game");
    asITypeInfo* info = module->GetTypeInfoByDecl(baseClass.c_str());

    if (info)
    {
        asUINT typeCount = module->GetObjectTypeCount();
        for (asUINT i = 0; i < typeCount; i++)
        {
            asITypeInfo* type = module->GetObjectTypeByIndex(i);
            if (info == type) continue;
            if (type->GetBaseType() == info)
            {
                out.emplace_back(type->GetName());
            }
        }
    }

    return out;
}

std::vector<std::string> srph::Engine::QueryImplementations(const std::string& interface) const
{
    std::vector<std::string> out = {};

    // TODO(Seb): Read module from somewhere
    asIScriptModule* module = m_engine->GetModule("Game");
    asITypeInfo* info = module->GetTypeInfoByDecl(interface.c_str());

    if (info)
    {
        asUINT typeCount = module->GetObjectTypeCount();
        for (asUINT i = 0; i < typeCount; i++)
        {
            asITypeInfo* type = module->GetObjectTypeByIndex(i);
            if (info == type) continue;
            if (type->Implements(info))
            {
                out.emplace_back(type->GetName());
            }
        }
    }

    return out;
}

// Uses a default constructor
srph::InstanceHandle srph::Engine::CreateInstance(const std::string& typeName)
{
    if (!m_built) return {};

    // TODO(Seb): Replace "Game"
    asIScriptModule* module = m_engine->GetModule("Game");
    asITypeInfo* type = module->GetTypeInfoByName(typeName.c_str());

    if (!type)
    {
        Log::Error("Type '{}' is not registered in module '{}'.", typeName, "Game");
        return {};
    }

    asIScriptFunction* factory = nullptr;
    for (asUINT i = 0; i < type->GetFactoryCount(); i++)
    {
        asIScriptFunction* f = type->GetFactoryByIndex(i);
        const char* decl = f->GetDeclaration();

        if (strstr(decl, "@") && strstr(decl, "()"))
        {
            factory = f;
            break;
        }
    }

    if (factory)
    {
        m_context->Prepare(factory);
        m_context->Execute();

        InstanceHandle handle = {RandomHandle()};
        m_instances[handle] = *static_cast<asIScriptObject**>(m_context->GetAddressOfReturnValue());
        SRPH_VERIFY(m_instances[handle]->AddRef(), "Could not AddRef() to the new class.")

        return handle;
    }

    return {};
}

// Constructs the instance using the provided factory
srph::InstanceHandle srph::Engine::CreateInstance(const std::string& /*typeName*/, srph::FunctionCaller& functionCall)
{
    if (!m_built) return {};
    FunctionResult result = functionCall.Call(ReturnType::Object);

    InstanceHandle handle = {RandomHandle()};
    m_instances[handle] = std::get<asIScriptObject*>(result.value);

    return handle;
}

std::string srph::Engine::GetTypeName(InstanceHandle handle) const
{
    if (!m_built) return "";
    return m_instances.at(handle)->GetObjectType()->GetName();
}

asITypeInfo* srph::Engine::GetTypeInfo(const std::string& typeName) const
{
    if (!m_built) return nullptr;

    // TODO(Seb): Replace "Game"
    asIScriptModule* module = m_engine->GetModule("Game");
    return module->GetTypeInfoByName(typeName.c_str());
}

std::vector<srph::ReflectedProperty> srph::Engine::Reflect(const InstanceHandle handle) const
{
    if (!m_built) return {};

    std::vector<ReflectedProperty> data = srph::reflection::ReflectProperties(m_instances.at(handle), m_engine);
    return data;
}

std::vector<srph::ReflectedProperty> srph::Engine::Reflect(InstanceHandle handle, const std::string& metadata) const
{
    if (!m_built) return {};

    std::vector<ReflectedProperty> data = srph::reflection::ReflectProperties(m_instances.at(handle), m_engine);

    std::string typeName = GetTypeName(handle);

    std::vector<ReflectedProperty> out;
    for (ReflectedProperty& property : data)
    {
        auto meta = GetMetadata(typeName, property.name);
        if (std::find(meta.begin(), meta.end(), metadata) != meta.end())
        {
            out.emplace_back(property);
        }
    }

    return out;
}

std::vector<std::string> srph::Engine::GetMetadata(const std::string& typeName, const std::string& propertyName) const
{
    if (m_metadata.find(typeName) == m_metadata.end()) return {};
    if (m_metadata.at(typeName).find(propertyName) == m_metadata.at(typeName).end()) return {};

    return m_metadata.at(typeName).at(propertyName);
}

void srph::Engine::Namespace(const std::string& ns) const
{
    SRPH_VERIFY(m_engine->SetDefaultNamespace(ns.c_str()), "Failed to set namespace.")
}

void srph::Engine::GeneratePredefined(const std::string& path) { GenerateScriptPredefined(m_engine, path); }

void srph::Engine::StopDebugger() { delete m_debugger; }

void srph::Engine::MessageCallback(const asSMessageInfo* msg) const
{
    const char* typeStr = msg->type == asMSGTYPE_ERROR ? "ERROR" : msg->type == asMSGTYPE_WARNING ? "WARNING" : "INFO";

    std::string location =
        msg->section ? fmt::format("{}:{}:{}", msg->section, msg->row, msg->col) : fmt::format("{}:{}", msg->row, msg->col);

    if (msg->type == asMSGTYPE_ERROR)
    {
        Log::Error("[{}] {}: {}", typeStr, location, msg->message);
    }
    else if (msg->type == asMSGTYPE_WARNING)
    {
        Log::Warn("[{}] {}: {}", typeStr, location, msg->message);
    }
    else
    {
        // Log::Info("[{}] {}: {}", typeStr, location, msg->message);
    }
}

void srph::Engine::LineCallback(asIScriptContext* context) const
{
    for (auto& entry : m_lineCallbacks)
    {
        entry.second(context);
    }
}

void srph::Engine::Print(const std::string& str) const { Log::ScriptInfo("{}", str); }

asIScriptContext* srph::Engine::GetContext()
{
    // TODO(Seb): Implement context pooling
    asIScriptContext* ctx = m_engine->CreateContext();
    m_contexts.push_back(ctx);

    SRPH_VERIFY(ctx->SetLineCallback(asMETHOD(Engine, LineCallback), this, asCALL_THISCALL), "Could not set line callback.")

    return m_contexts.back();
}

void srph::Engine::ReleaseContext(asIScriptContext* ctx)
{
    m_contexts.erase(std::find(m_contexts.begin(), m_contexts.end(), ctx));
    SRPH_VERIFY(ctx->Release(), "Failed to release context.")
}

asIScriptModule* srph::Engine::GetModule(const std::string& moduleName)
{
    if (m_moduleCache.find(moduleName) == m_moduleCache.end())
    {
        asIScriptModule* module = m_engine->GetModule(moduleName.c_str());
        if (module)
        {
            m_moduleCache[moduleName] = module;
        }

        return module;
    }

    return m_moduleCache.at(moduleName);
}

asIScriptFunction* srph::Engine::GetMethod(asITypeInfo* type, const std::string& methodDecl)
{
    CachedMethodKey key = {static_cast<void*>(type), methodDecl};
    if (m_functionCache.find(key) == m_functionCache.end())
    {
        asIScriptFunction* func = type->GetMethodByDecl(methodDecl.c_str());
        m_functionCache[key] = func;
        return func;
    }

    return m_functionCache.at(key);
}

asIScriptFunction* srph::Engine::GetFunction(asIScriptModule* module, const std::string& functionDecl)
{
    CachedMethodKey key = {static_cast<void*>(module), functionDecl};
    if (m_functionCache.find(key) == m_functionCache.end())
    {
        asIScriptFunction* func = module->GetFunctionByDecl(functionDecl.c_str());
        m_functionCache[key] = func;
        return func;
    }

    return m_functionCache.at(key);
}

srph::InstanceHandle srph::Engine::RandomHandle() const
{
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<uint64_t> distribution;

    return {static_cast<InstanceID>(distribution(generator))};
}