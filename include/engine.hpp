#pragma once

#include "instance_handle.hpp"
#include "script_reflection.hpp"

#include <unordered_map>

#include "engine_configuration.hpp"

class asIScriptEngine;
struct asSMessageInfo;
class asIScriptContext;
class asIScriptObject;
class asITypeInfo;
class asIScriptModule;
class asIScriptFunction;

namespace srph
{
namespace debugger
{
class Debugger;
class DAP;
}  // namespace debugger

namespace TypeRegistration
{
enum class ClassType : uint8_t;
template <typename T>
class Enum;
template <typename T>
class Enum;

template <typename T, ClassType>
class Class;

class Global;
class Interface;
}  // namespace TypeRegistration

class FunctionCaller;

struct CachedMethodKey
{
    const void* owner;
    std::string signature;

    bool operator==(const CachedMethodKey& o) const { return owner == o.owner && signature == o.signature; }
};

struct CachedMethodKeyHash
{
    size_t operator()(const CachedMethodKey& k) const
    {
        size_t h1 = std::hash<const void*>{}(k.owner);
        size_t h2 = std::hash<std::string>{}(k.signature);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

class Engine
{
public:
    Engine() = default;
    ~Engine() = default;
    void Initialize(EngineConfiguration configuration);
    void Shutdown();

    // Attaches a debugger if not already attached
    void AttachDebugger();
    void StopDebugger();

    void RegisterTimeoutCallback(const std::function<void()>& f) { m_timeoutCallback = f; }
    void RegisterLineCallback(const std::string& key, const std::function<void(asIScriptContext* context)>& f);
    void RemoveLineCallback(const std::string& key);

    const EngineConfiguration& GetConfiguration() const { return m_configuration; }
    bool Built() const { return m_built; }
    std::vector<InstanceHandle> GetInstances() const;

    std::vector<std::string> QueryDerivedClasses(const std::string& baseClass, const std::string& moduleName) const;
    std::vector<std::string> QueryImplementations(const std::string& interface, const std::string& moduleName) const;
    InstanceHandle CreateInstance(const std::string& typeName, const std::string& moduleName);
    InstanceHandle CreateInstance(srph::FunctionCaller& functionCall);

    std::string GetTypeName(InstanceHandle handle) const;

    asIScriptObject* GetNativeObject(InstanceHandle handle) const { return m_instances.at(handle); }

    std::vector<ReflectedProperty> Reflect(InstanceHandle handle) const;
    std::vector<ReflectedProperty> Reflect(InstanceHandle handle, const std::string& metadata) const;

    // TODO(Seb): Support more complex metadata? [Header], [Separator], [Range(1,100)]
    std::vector<std::string> GetMetadata(const std::string& typeName, const std::string& propertyName) const;

    void Namespace(const std::string& ns) const;

    void GeneratePredefined(const std::string& path);

private:
    asIScriptEngine* m_engine = nullptr;
    asIScriptContext* m_context = nullptr;
    std::vector<asIScriptContext*> m_contexts = {};
    std::unordered_map<InstanceHandle, asIScriptObject*> m_instances;
    Metadata m_metadata;

    std::function<void()> m_timeoutCallback = nullptr;
    std::unordered_map<std::string, std::function<void(asIScriptContext* context)>> m_lineCallbacks;

    std::unordered_map<std::string, asIScriptModule*> m_moduleCache;
    std::unordered_map<CachedMethodKey, asIScriptFunction*, CachedMethodKeyHash> m_functionCache;

    EngineConfiguration m_configuration;
    debugger::Debugger* m_debugger = nullptr;
    FunctionCaller* m_currentFunctionCaller = nullptr;
    bool m_built = false;

private:
    friend class ScriptLoader;
    friend class FunctionCaller;
    friend class debugger::Debugger;

    template <typename T>
    friend class TypeRegistration::Enum;
    template <typename T, TypeRegistration::ClassType>
    friend class TypeRegistration::Class;
    friend class TypeRegistration::Global;
    friend class TypeRegistration::Interface;

    void RegisterAddOns() const;

    void MessageCallback(const asSMessageInfo* msg) const;
    void LineCallback(asIScriptContext* context) const;
    void Print(const std::string& str) const;

    asIScriptEngine* GetEngine() const { return m_engine; }
    asIScriptContext* GetContext();
    void ReleaseContext(asIScriptContext* ctx);

    asIScriptModule* GetModule(const std::string& moduleName);
    asIScriptFunction* GetMethod(asITypeInfo* type, const std::string& methodDecl);
    asIScriptFunction* GetFunction(asIScriptModule* module, const std::string& functionDecl);

    InstanceHandle RandomHandle() const;
};
}  // namespace srph