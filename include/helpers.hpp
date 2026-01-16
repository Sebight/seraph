#pragma once

#include <cassert>
#include <chrono>
#include <fstream>
#include <string>
#include <string_view>

#include "angelscript.h"

namespace
{
template <class Stream>
void printEnumList(const asIScriptEngine* engine, Stream& stream)
{
    const int enumCount = engine->GetEnumCount();
    for (int i = 0; i < enumCount; i++)
    {
        const auto e = engine->GetEnumByIndex(i);
        if (not e) continue;
        const std::string_view ns = e->GetNamespace();
        if (not ns.empty()) stream << fmt::format("namespace {} {{\n", ns);
        stream << fmt::format("enum {} {{\n", e->GetName());
        const int valueCount = e->GetEnumValueCount();
        for (int j = 0; j < valueCount; ++j)
        {
            stream << fmt::format("\t{}", e->GetEnumValueByIndex(j, nullptr));
            if (j < valueCount - 1) stream << ",";
            stream << "\n";
        }
        stream << "}\n";
        if (not ns.empty()) stream << "}\n";
    }
}

template <class Stream>
void printClassTypeList(const asIScriptEngine* engine, Stream& stream)
{
    const int objectTypeCount = engine->GetObjectTypeCount();
    for (int i = 0; i < objectTypeCount; i++)
    {
        const auto t = engine->GetObjectTypeByIndex(i);
        if (not t) continue;

        const std::string_view ns = t->GetNamespace();
        if (not ns.empty()) stream << fmt::format("namespace {} {{\n", ns);

        stream << fmt::format("class {}", t->GetName());
        const asUINT subTypeCount = t->GetSubTypeCount();
        if (subTypeCount > 0)
        {
            stream << "<";
            for (asUINT sub = 0; sub < subTypeCount; ++sub)
            {
                if (sub > 0) stream << ", ";
                const auto st = t->GetSubType(sub);
                stream << st->GetName();
            }

            stream << ">";
        }

        stream << "{\n";
        const asUINT behaviourCount = t->GetBehaviourCount();
        for (asUINT j = 0; j < behaviourCount; ++j)
        {
            asEBehaviours behaviours;
            const auto f = t->GetBehaviourByIndex(static_cast<int>(j), &behaviours);
            if (behaviours == asBEHAVE_CONSTRUCT || behaviours == asBEHAVE_DESTRUCT)
            {
                stream << fmt::format("\t{};\n", f->GetDeclaration(false, true, true));
            }
        }
        const asUINT methodCount = t->GetMethodCount();
        for (asUINT j = 0; j < methodCount; ++j)
        {
            const auto m = t->GetMethodByIndex(static_cast<int>(j));
            stream << fmt::format("\t{};\n", m->GetDeclaration(false, true, true));
        }
        const asUINT propertyCount = t->GetPropertyCount();
        for (asUINT j = 0; j < propertyCount; ++j)
        {
            stream << fmt::format("\t{};\n", t->GetPropertyDeclaration(static_cast<int>(j), true));
        }
        const asUINT funcdefCount = t->GetChildFuncdefCount();
        for (asUINT j = 0; j < funcdefCount; ++j)
        {
            stream << fmt::format("\tfuncdef {};\n",
                                  t->GetChildFuncdef(static_cast<int>(j))->GetFuncdefSignature()->GetDeclaration(false));
        }
        stream << "}\n";
        if (not ns.empty()) stream << "}\n";
    }
}

template <class Stream>
void printGlobalFunctionList(const asIScriptEngine* engine, Stream& stream)
{
    const int functionCount = engine->GetGlobalFunctionCount();
    for (int i = 0; i < functionCount; i++)
    {
        const auto f = engine->GetGlobalFunctionByIndex(i);
        if (not f) continue;
        const std::string_view ns = f->GetNamespace();
        if (not ns.empty()) stream << fmt::format("namespace {} {{ ", ns);
        stream << fmt::format("{};", f->GetDeclaration(false, false, true));
        if (not ns.empty()) stream << " }";
        stream << "\n";
    }
}

template <class Stream>
void printGlobalPropertyList(const asIScriptEngine* engine, Stream& stream)
{
    const int propertyCount = engine->GetGlobalPropertyCount();
    for (int i = 0; i < propertyCount; i++)
    {
        const char* name;
        const char* ns0;
        int type;
        engine->GetGlobalPropertyByIndex(i, &name, &ns0, &type, nullptr, nullptr, nullptr, nullptr);

        const std::string t = engine->GetTypeDeclaration(type, true);
        if (t.empty()) continue;

        const std::string_view ns = ns0;
        if (not ns.empty()) stream << fmt::format("namespace {} {{ ", ns);

        stream << fmt::format("{} {};", t, name);
        if (not ns.empty()) stream << " }";
        stream << "\n";
    }
}

template <class Stream>
void printGlobalTypedef(const asIScriptEngine* engine, Stream& stream)
{
    const int typedefCount = engine->GetTypedefCount();
    for (int i = 0; i < typedefCount; ++i)
    {
        const auto type = engine->GetTypedefByIndex(i);
        if (not type) continue;
        const std::string_view ns = type->GetNamespace();
        if (not ns.empty()) stream << fmt::format("namespace {} {{\n", ns);
        stream << fmt::format("typedef {} {};\n", engine->GetTypeDeclaration(type->GetTypedefTypeId()), type->GetName());
        if (not ns.empty()) stream << "}\n";
    }
}
}  // namespace

/// @brief Generate 'as.predefined' file, which contains all defined symbols in C++. It is used by the language server.
inline void GenerateScriptPredefined(const asIScriptEngine* engine, const std::string& path)
{
    std::ofstream stream{path};

    printEnumList(engine, stream);

    printClassTypeList(engine, stream);

    printGlobalFunctionList(engine, stream);

    printGlobalPropertyList(engine, stream);

    printGlobalTypedef(engine, stream);
}

namespace srph
{
struct Timer
{
    Timer() { Reset(); }

    /// Returns the elapsed time since start in milliseconds.
    float Elapsed() const
    {
        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        return static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(t2 - start).count());
    }

    float ElapsedUs() const
    {
        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        return static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(t2 - start).count());
    }

    void Reset() { start = std::chrono::high_resolution_clock::now(); }
    std::chrono::high_resolution_clock::time_point start;
};

struct ScopedTimer
{
    Timer t;
    const char* name;

    ScopedTimer(const char* funcName) { name = funcName; }

    ~ScopedTimer() { Log::Info("{} took {}ms.", name, t.Elapsed()); }
};

struct PreciseScopedTimer
{
    Timer t;
    const char* name;

    PreciseScopedTimer(const char* funcName) { name = funcName; }

    ~PreciseScopedTimer() { Log::Info("{} took {}us.", name, t.ElapsedUs()); }
};

#define SCOPED_TIMER() ScopedTimer timer(__FUNCTION__)
#define PRECISE_SCOPED_TIMER() PreciseScopedTimer timer(__FUNCTION__)
}  // namespace srph