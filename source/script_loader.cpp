#include "srph_common.hpp"
#include "script_loader.hpp"

#include "engine.hpp"

srph::ScriptLoader::ScriptLoader(Engine* engine) { m_engine = engine; }

srph::ScriptLoader& srph::ScriptLoader::Module(const std::string& moduleName)
{
    m_moduleName = moduleName;
    return *this;
}

srph::ScriptLoader& srph::ScriptLoader::LoadScript(const std::string& path)
{
    m_scripts.push_back(path);
    return *this;
}

bool srph::ScriptLoader::Build()
{
    m_engine->m_built = false;
    CScriptBuilder builder;
    SRPH_VERIFY(builder.StartNewModule(m_engine->GetEngine(), m_moduleName.c_str()), "Failed to create module.")
    for (auto& script : m_scripts)
    {
        builder.AddSectionFromFile(script.c_str());
    }

    int r = builder.BuildModule();
    if (r != 0)
    {
        return false;
    }
    m_engine->m_built = true;
    asIScriptModule* module = m_engine->m_engine->GetModule(m_moduleName.c_str());
    for (asUINT i = 0; i < module->GetObjectTypeCount(); i++)
    {
        asITypeInfo* type = module->GetObjectTypeByIndex(i);
        std::string typeName = type->GetName();
        int typeId = type->GetTypeId();

        for (asUINT j = 0; j < type->GetPropertyCount(); j++)
        {
            const char* propName = "";
            type->GetProperty(j, &propName);
            std::string propNameStr = std::string(propName);

            std::vector<std::string> propertyMetadata = builder.GetMetadataForTypeProperty(typeId, j);
            if (!propertyMetadata.empty())
            {
                m_engine->m_metadata[typeName][propNameStr] = propertyMetadata;
            }
        }
    }
    return true;
}