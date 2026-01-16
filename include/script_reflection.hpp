#pragma once
#include <unordered_map>

class asIScriptObject;
class asIScriptEngine;

namespace srph
{
struct ReflectedProperty
{
    std::string type;
    std::string name;
    void* data;
};

using Metadata = std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>>;

namespace reflection
{
std::vector<ReflectedProperty> ReflectProperties(asIScriptObject* obj, const asIScriptEngine* engine);

std::string GetValue(int typeId, void* value, const asIScriptEngine* engine);

std::string GetTypename(int typeId, const asIScriptEngine* engine);
}  // namespace reflection

}  // namespace srph