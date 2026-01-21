#pragma once

#include "../external/angelscript/include/angelscript.h"
#include "../external/angelscript/add_on/autowrapper/aswrappedcall.h"

#undef MAGIC_ENUM_RANGE_MIN
#undef MAGIC_ENUM_RANGE_MAX

#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 512
#include "magic_enum/magic_enum_all.hpp"
#include "tools/log.hpp"
#include "srph_verify.hpp"

namespace srph
{
class Engine;
}

namespace srph::TypeRegistration
{

enum class OperatorType
{
    Add,
    Sub,
    Mul,
    Div,
    AddAssign,
    SubAssign,
    MulAssign,
    DivAssign
};

template <typename T>
class Enum
{
public:
    Enum() = delete;
    Enum(Engine* engine) { m_engine = engine; }

    Enum& Name(std::string name)
    {
        m_name = std::move(name);
        return *this;
    }

    void Register()
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        std::string name = m_name.empty() ? std::string(magic_enum::enum_type_name<T>()) : m_name;
        SRPH_VERIFY(engine->RegisterEnum(name.c_str()), "Enum registration failed.")
        magic_enum::enum_for_each<T>(
            [&](auto val)
            {
                constexpr T value = val;
                SRPH_VERIFY(engine->RegisterEnumValue(name.c_str(),
                                                      std::string(magic_enum::enum_name(value)).c_str(),
                                                      static_cast<int>(value)),
                            "Enum value registration failed.")
            });
    }

private:
    Engine* m_engine;
    std::string m_name;
};

namespace generics
{
template <typename T>
void DefaultConstructor(void* memory)
{
    new (memory) T();
}

template <typename T>
void CopyConstructor(void* memory, const T& other)
{
    new (memory) T(other);
}

template <typename T>
std::conditional_t<std::is_pointer_v<T>, T, std::add_lvalue_reference_t<T>> CastFromGenericObject(asIScriptGeneric* generic)
{
    void* obj = generic->GetObject();
    if (std::is_pointer_v<T>)
    {
        return static_cast<T>(obj);
    }
    else
    {
        using PointerType = std::add_pointer_t<std::remove_reference_t<T>>;
        return *static_cast<PointerType>(obj);
    }
}
}  // namespace generics

enum class ClassType : uint8_t
{
    Value = 0,
    Reference
};

template <typename T, ClassType classType>
class Class
{
public:
    Class() = delete;

    Class(Engine* engine, std::string name, asEObjTypeFlags flags = {})
    {
        m_engine = engine;
        m_name = std::move(name);

        m_flags = flags;

        Register();
    }

    Class& BehavioursByTraits()
    {
        asUINT traits = asGetTypeTraits<T>();
        if (traits & (asOBJ_APP_CLASS_C))
        {
            if constexpr (std::is_constructible_v<T>)
            {
                DefaultConstructor();
            }
            else
            {
                assert(false && "No default constructor found.");
            }
        }

        if (traits & (asOBJ_APP_CLASS_D))
        {
            if constexpr (std::is_destructible_v<T>)
            {
                Destructor();
            }
            else
            {
                assert(false && "No destructor found.");
            }
        }

        if (traits & (asOBJ_APP_CLASS_A))
        {
            if constexpr (std::is_copy_assignable_v<T>)
            {
                OperatorAssign();
            }
            else
            {
                assert(false && "No assignment operator found.");
            }
        }
        if (traits & (asOBJ_APP_CLASS_K))
        {
            CopyConstructor();
        }

        return *this;
    }

    template <typename... Args>
    Class& Constructor(const char* decl)
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        SRPH_VERIFY(engine->RegisterObjectBehaviour(m_name.c_str(),
                                                    asBEHAVE_CONSTRUCT,
                                                    fmt::format("void f({})", decl).c_str(),
                                                    asFUNCTION(+[](Args... args, void* mem) { new (mem) T(args...); }),
                                                    asCALL_CDECL_OBJLAST),
                    "Constructor registration failed.")

        return *this;
    }
#define SRPH_OPERATOR(Op, Params, ReturnType) WRAP_OBJ_FIRST_PR(Op, Params, ReturnType)
#define SRPH_OPERATOR_MEMBER(Type, Op, Params, ReturnType) WRAP_MFN_PR(Type, Op, Params, ReturnType)

    Class& Operator(asSFuncPtr operatorFunc,
                    OperatorType op,
                    const char* returnType,
                    const char* param,
                    bool primitiveParam = false)
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();
        std::string opName = std::string(magic_enum::enum_name(op));
        std::string paramDecl = primitiveParam ? param : fmt::format("const {}&in", param);
        std::string fullName = fmt::format("{} op{}({})", returnType, opName.c_str(), paramDecl);

        SRPH_VERIFY(engine->RegisterObjectMethod(m_name.c_str(), fullName.c_str(), operatorFunc, asCALL_GENERIC),
                    "Operator registration failed.")
        return *this;
    }

    Class& Property(const std::string& name, size_t offset)
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        SRPH_VERIFY(engine->RegisterObjectProperty(m_name.c_str(), name.c_str(), static_cast<int>(offset)),
                    "Property registration failed.");

        return *this;
    }

    // Lambda
    template <typename Func>
    std::enable_if_t<!std::is_member_function_pointer_v<Func>, Class&> Method(const std::string& funcDecl, Func func)
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        auto fnPtr = +func;

        SRPH_VERIFY(engine->RegisterObjectMethod(m_name.c_str(), funcDecl.c_str(), asFUNCTION(fnPtr), asCALL_CDECL_OBJFIRST),
                    "Method registration by lambda failed.")

        return *this;
    }

    // Non-const
    template <typename R, typename... Args>
    Class& Method(const char* funcDecl, R (T::*method)(Args...))
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();
        SRPH_VERIFY(engine->RegisterObjectMethod(m_name.c_str(),
                                                 funcDecl,
                                                 asSMethodPtr<sizeof(void(T::*)())>::Convert(method),
                                                 asCALL_THISCALL),
                    "Method registration by non-const function pointer failed.")

        return *this;
    }

    // Const
    template <typename R, typename... Args>
    Class& Method(const char* funcDecl, R (T::*method)(Args...) const)
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();
        SRPH_VERIFY(engine->RegisterObjectMethod(m_name.c_str(),
                                                 funcDecl,
                                                 asSMethodPtr<sizeof(void(T::*)())>::Convert(method),
                                                 asCALL_THISCALL),
                    "Method registration by const function pointer failed.")

        return *this;
    }

public:
    Class& DefaultConstructor()
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        SRPH_VERIFY(engine->RegisterObjectBehaviour(m_name.c_str(),
                                                    asBEHAVE_CONSTRUCT,
                                                    "void f()",
                                                    asFUNCTION(+[](void* mem) { generics::DefaultConstructor<T>(mem); }),
                                                    asCALL_CDECL_OBJLAST),
                    "Default constructor registration failed.")

        return *this;
    }

    Class& CopyConstructor()
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        SRPH_VERIFY(engine->RegisterObjectBehaviour(
                        m_name.c_str(),
                        asBEHAVE_CONSTRUCT,
                        fmt::format("void f(const {} &in)", m_name).c_str(),
                        asFUNCTION(+[](void* mem, const T& other) { generics::CopyConstructor<T>(mem, other); }),
                        asCALL_CDECL_OBJFIRST),
                    "Copy constructor registration failed.")

        return *this;
    }

    Class& Destructor()
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        SRPH_VERIFY(
            engine->RegisterObjectBehaviour(m_name.c_str(),
                                            asBEHAVE_DESTRUCT,
                                            "void f()",
                                            asFUNCTION(+[](asIScriptGeneric* generic) -> void
                                                       { std::destroy_at(generics::CastFromGenericObject<T*>(generic)); }),
                                            asCALL_GENERIC),
            "Destructor registration failed.")

        return *this;
    }

    Class& OperatorAssign()
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        SRPH_VERIFY(engine->RegisterObjectMethod(m_name.c_str(),
                                                 fmt::format("{}& opAssign(const {}&in)", m_name, m_name).c_str(),
                                                 asFUNCTION(+[](asIScriptGeneric* generic) -> void
                                                            {
                                                                T* _self = static_cast<T*>(generic->GetObject());
                                                                T* _other = static_cast<T*>(generic->GetArgObject(0));
                                                                *_self = *_other;
                                                                generic->SetReturnAddress(_self);
                                                            }),
                                                 asCALL_GENERIC),
                    "Assign operator registration failed.")

        return *this;
    }

    // Note(Seb): This was done because of special cases where I cannot rely on C++ constructors doing the correct thing, and I
    // need extra logic when it comes to construction
    Class& Behaviour(asEBehaviours behaviour,
                     const char* decl,
                     const asSFuncPtr& funcPointer,
                     asDWORD callConv = asCALL_CDECL_OBJLAST)
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        SRPH_VERIFY(engine->RegisterObjectBehaviour(m_name.c_str(), behaviour, decl, funcPointer, callConv),
                    "Behaviour registration failed.")

        return *this;
    }

    void Register()
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        constexpr bool valueType = classType == ClassType::Value;
        asEObjTypeFlags typeFlag = valueType ? asOBJ_VALUE : asOBJ_REF;
        SRPH_VERIFY(
            engine->RegisterObjectType(m_name.c_str(), sizeof(T), typeFlag | m_flags | (valueType ? asGetTypeTraits<T>() : 0)),
            "Type registration failed.")
    }

private:
    Engine* m_engine;
    std::string m_name;
    asEObjTypeFlags m_flags;
};

class Global
{
public:
    Global(Engine* engine) { m_engine = engine; }

    // Lambda
    template <typename Func>
    Global& Function(const std::string& funcDecl, Func func)
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        auto fnPtr = +func;

        SRPH_VERIFY(engine->RegisterGlobalFunction(funcDecl.c_str(), asFUNCTION(fnPtr), asCALL_CDECL),
                    "Global function registration failed.")

        return *this;
    }

private:
    Engine* m_engine = nullptr;
};

class Interface
{
public:
    Interface() = delete;

    Interface(Engine* engine, std::string name)
    {
        m_engine = engine;
        m_name = std::move(name);

        Register();
    }

    Interface& Method(const char* methodDecl)
    {
        m_engine->Temp_GetEngine()->RegisterInterfaceMethod(m_name.c_str(), methodDecl);

        return *this;
    }

    void Register()
    {
        asIScriptEngine* engine = m_engine->Temp_GetEngine();

        SRPH_VERIFY(engine->RegisterInterface(m_name.c_str()), "Interface registration failed.")
    }

private:
    Engine* m_engine;
    std::string m_name;
};

}  // namespace srph::TypeRegistration

namespace srph
{
using ClassType = srph::TypeRegistration::ClassType;
}