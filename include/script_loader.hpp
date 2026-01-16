#pragma once

namespace srph
{
class Engine;

struct ScriptError
{
    std::string message;
};

class ScriptLoader
{
public:
    ScriptLoader(Engine* engine);

    ScriptLoader& Module(const std::string& moduleName);
    ScriptLoader& LoadScript(const std::string& path);
    bool Build();

private:
    std::string m_moduleName = "";
    std::vector<std::string> m_scripts = {};

    Engine* m_engine = nullptr;
};
}  // namespace srph