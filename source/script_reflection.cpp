#include "srph_common.hpp"
#include "script_reflection.hpp"

std::vector<srph::ReflectedProperty> srph::reflection::ReflectProperties(asIScriptObject* obj, const asIScriptEngine* engine)
{
    const asUINT props = obj->GetPropertyCount();

    std::vector<ReflectedProperty> out;
    out.reserve(props);

    for (asUINT i = 0; i < props; i++)
    {
        int typeId = obj->GetPropertyTypeId(i);
        asITypeInfo* type = engine->GetTypeInfoById(typeId);

        ReflectedProperty property = {};
        property.name = std::string(obj->GetPropertyName(i));

        if (type)
        {
            // Application registered time
            property.type = std::string(type->GetName());
        }
        else
        {
            std::string typeName = GetTypename(typeId, engine);

            property.type = typeName;
        }

        property.data = obj->GetAddressOfProperty(i);

        out.emplace_back(property);
    }

    return out;
}

std::string srph::reflection::GetValue(int typeId, void* value, const asIScriptEngine* engine)
{
    if (!value) return "null";

    int baseTypeId = typeId & ~(asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST);

    switch (baseTypeId)
    {
        case asTYPEID_VOID:
            return "void";
        case asTYPEID_BOOL:
            return *static_cast<bool*>(value) ? "true" : "false";
        case asTYPEID_INT8:
            return std::to_string(*static_cast<int8_t*>(value));
        case asTYPEID_INT16:
            return std::to_string(*static_cast<int16_t*>(value));
        case asTYPEID_INT32:
            return std::to_string(*static_cast<int32_t*>(value));
        case asTYPEID_INT64:
            return std::to_string(*static_cast<int64_t*>(value));
        case asTYPEID_UINT8:
            return std::to_string(*static_cast<uint8_t*>(value));
        case asTYPEID_UINT16:
            return std::to_string(*static_cast<uint16_t*>(value));
        case asTYPEID_UINT32:
            return std::to_string(*static_cast<uint32_t*>(value));
        case asTYPEID_UINT64:
            return std::to_string(*static_cast<uint64_t*>(value));
        case asTYPEID_FLOAT:
            return std::to_string(*static_cast<float*>(value));
        case asTYPEID_DOUBLE:
            return std::to_string(*static_cast<double*>(value));
        default:
            if (typeId & asTYPEID_MASK_OBJECT)
            {
                asITypeInfo* typeInfo = engine->GetTypeInfoById(typeId);
                if (typeInfo)
                {
                    if (strcmp(typeInfo->GetName(), "string") == 0)
                    {
                        if (typeId & asTYPEID_OBJHANDLE)
                        {
                            std::string* str = *static_cast<std::string**>(value);
                            return str ? "\"" + *str + "\"" : "null";
                        }
                        else
                        {
                            return "\"" + *static_cast<std::string*>(value) + "\"";
                        }
                    }

                    return std::string(typeInfo->GetName()) + "@" + std::to_string(reinterpret_cast<uintptr_t>(value));
                }
            }
            return "<unknown type>";
    }
}

std::string srph::reflection::GetTypename(int typeId, const asIScriptEngine* engine)
{
    std::string typeName;
    switch (typeId & ~(asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST))
    {
        case asTYPEID_VOID:
            typeName = "void";
            break;
        case asTYPEID_BOOL:
            typeName = "bool";
            break;
        case asTYPEID_INT8:
            typeName = "int8";
            break;
        case asTYPEID_INT16:
            typeName = "int16";
            break;
        case asTYPEID_INT32:
            typeName = "int";
            break;
        case asTYPEID_INT64:
            typeName = "int64";
            break;
        case asTYPEID_UINT8:
            typeName = "uint8";
            break;
        case asTYPEID_UINT16:
            typeName = "uint16";
            break;
        case asTYPEID_UINT32:
            typeName = "uint";
            break;
        case asTYPEID_UINT64:
            typeName = "uint64";
            break;
        case asTYPEID_FLOAT:
            typeName = "float";
            break;
        case asTYPEID_DOUBLE:
            typeName = "double";
            break;
        default:
            if (typeId & asTYPEID_MASK_OBJECT)
            {
                asITypeInfo* typeInfo = engine->GetTypeInfoById(typeId);
                if (typeInfo) typeName = typeInfo->GetName();
            }
            break;
    }

    if (typeId & asTYPEID_HANDLETOCONST) typeName = "const " + typeName;
    if (typeId & asTYPEID_OBJHANDLE) typeName += typeName + "@";

    return typeName;
}
