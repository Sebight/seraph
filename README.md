# Seraph

Seraph is a C++ wrapper library for AngelScript that streamlines type registration, function binding, and script-to-native interoperability.

## Features

- Builder-pattern APIs for type registration and function calling
- Support for value types, reference types, enums, interfaces, and globals
- Property reflection with metadata/attribute filtering
- Script loading and compilation management

## Quick Start

### Initialize the Engine

```cpp
#include <seraph/seraph.hpp>

srph::Engine scripting;
```

### Register Types

**Value Type (stack-allocated, copied)**
```cpp
srph::TypeRegistration::Class<glm::vec3, srph::ClassType::Value>(scripting, "vec3")
    .BehavioursByTraits()
    .Constructor<float, float, float>("float x, float y, float z")
    .Property("float x", offsetof(glm::vec3, x))
    .Property("float y", offsetof(glm::vec3, y))
    .Property("float z", offsetof(glm::vec3, z));
```

**Reference Type (heap-allocated, handle-capable)**
```cpp
srph::TypeRegistration::Class<Transform, srph::ClassType::Reference>(scripting, "Transform", asOBJ_NOCOUNT)
    .Method("vec3 GetPosition()", &Transform::GetPosition)
    .Property("vec3 position", offsetof(Transform, position));
```

**Enum (automatic via magic_enum)**
```cpp
srph::TypeRegistration::Enum<KeyCode>(scripting).Register();
```

**Global Functions**
```cpp
srph::TypeRegistration::Global(scripting)
    .Function("float sin(float)", [](float x) { return std::sin(x); })
    .Function("float cos(float)", [](float x) { return std::cos(x); });
```

**Interfaces**
```cpp
srph::TypeRegistration::Interface(scripting, "IUpdatable")
    .Method("void Update(float dt)");
```

### Load and Build Scripts

```cpp
srph::ScriptLoader loader(scripting);
loader.Module("Game");
loader.LoadScript("assets/scripts/player.as");

if (loader.Build()) {
    // Scripts compiled successfully
}
```

### Create Instances

```cpp
srph::InstanceHandle handle = scripting.CreateInstance("Player", "Game");
```

### Call Script Functions

```cpp
srph::FunctionCaller(&scripting)
    .Module("Game")
    .Function("void Update(float)", handle)
    .Push(deltaTime)
    .Call();
```

### Reflect Properties

```cpp
auto fields = scripting.Reflect(handle, "Serialize");
for (auto& field : fields) {
    // field.type, field.name, field.data
}
```

## Example Script

```cpp
class Player : IUpdatable {
    [Serialize]
    float speed = 5.0f;
    
    void Update(float dt) {
        // Access registered C++ types directly
        vec3 movement = vec3(1.0f, 0.0f, 0.0f) * speed * dt;
    }
}
```

## License

MIT