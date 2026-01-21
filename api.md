# Seraph API Reference

## Table of Contents

- [Engine](#engine)
- [Type Registration](#type-registration)
  - [Value Classes](#value-classes)
  - [Reference Classes](#reference-classes)
  - [Enums](#enums)
  - [Interfaces](#interfaces)
  - [Global Functions](#global-functions)
- [Script Loading](#script-loading)
- [Function Calling](#function-calling)
- [Reflection](#reflection)
- [Instance Management](#instance-management)

---

## Engine

Core class managing AngelScript initialization, contexts, and instance tracking.

### Initialization

```cpp
srph::Engine scripting;
srph::EngineConfiguration config;
config.scriptTimeoutMillis = 1000.0f;
scripting.Initialize(config);
```

### Methods

| Method | Description |
|--------|-------------|
| `void Initialize(EngineConfiguration config)` | Initialize AngelScript engine with configuration |
| `void Shutdown()` | Release all resources, modules, and instances |
| `void AttachDebugger()` | Attach VSCode-compatible DAP debugger |
| `void StopDebugger()` | Detach debugger |
| `bool Built() const` | Returns true if scripts compiled successfully |
| `void Namespace(const std::string& ns)` | Set default namespace for subsequent registrations |
| `void GeneratePredefined(const std::string& path)` | Generate `as.predefined` for LSP autocompletion |
| `void RegisterTimeoutCallback(std::function<void()> f)` | Callback invoked when script execution times out |

---

## Type Registration

All registration uses builder pattern via `srph::TypeRegistration::*` classes.

### Value Classes

Stack-allocated types that are copied. Cannot use handles.

```cpp
srph::TypeRegistration::Class<glm::vec3, srph::ClassType::Value>(engine, "vec3")
    .BehavioursByTraits()                                    // Auto-register ctor/dtor/copy/assign
    .Constructor<float>("float scalar")                      // Custom constructor
    .Constructor<float, float, float>("float x, float y, float z")
    .Property("float x", offsetof(glm::vec3, x))
    .Property("float y", offsetof(glm::vec3, y))
    .Property("float z", offsetof(glm::vec3, z))
    .Operator(SRPH_OPERATOR(glm::operator+, (const vec3&, const vec3&), vec3),
              srph::TypeRegistration::OperatorType::Add, "vec3", "vec3")
    .Operator(SRPH_OPERATOR_MEMBER(vec3, operator+=, (const vec3&), vec3&),
              srph::TypeRegistration::OperatorType::AddAssign, "vec3", "vec3");
```

### Reference Classes

Heap-allocated types supporting handles (`@`).

```cpp
srph::TypeRegistration::Class<Transform, srph::ClassType::Reference>(engine, "Transform", asOBJ_NOCOUNT)
    .OperatorAssign()
    .Method("vec3 GetPosition()", &Transform::GetPosition)
    .Method("void SetPosition(const vec3&in)", &Transform::SetPosition)
    .Property("vec3 position", offsetof(Transform, position));
```

### Class Builder Methods

| Method | Description |
|--------|-------------|
| `BehavioursByTraits()` | Auto-register default ctor, dtor, copy ctor, assign based on C++ type traits |
| `DefaultConstructor()` | Register default constructor |
| `CopyConstructor()` | Register copy constructor |
| `Destructor()` | Register destructor |
| `OperatorAssign()` | Register assignment operator |
| `Constructor<Args...>(const char* decl)` | Register constructor with specified parameter types |
| `Property(const std::string& decl, size_t offset)` | Register member variable |
| `Method(const char* decl, MemberFnPtr method)` | Register member function |
| `Method(const std::string& decl, Lambda func)` | Register method via lambda (receives `T&` as first arg) |
| `Operator(asSFuncPtr, OperatorType, returnType, paramType)` | Register operator overload |
| `Behaviour(asEBehaviours, const char* decl, asSFuncPtr, asDWORD callConv)` | Register custom behaviour |

### Operator Macros

```cpp
// Free function operator
SRPH_OPERATOR(glm::operator+, (const vec3&, const vec3&), vec3)

// Member function operator
SRPH_OPERATOR_MEMBER(vec3, operator+=, (const vec3&), vec3&)
```

### Operator Types

`Add`, `Sub`, `Mul`, `Div`, `AddAssign`, `SubAssign`, `MulAssign`, `DivAssign`

### Enums

Automatic registration via `magic_enum`.

```cpp
// Uses enum type name
srph::TypeRegistration::Enum<KeyCode>(engine).Register();

// Custom name
srph::TypeRegistration::Enum<KeyCode>(engine).Name("Key").Register();
```

With namespaces:

```cpp
engine->Namespace("Input");
srph::TypeRegistration::Enum<Input::KeyCode>(engine).Register();
// Registers as Input::KeyCode
```

### Interfaces

Define required methods for script classes.

```cpp
srph::TypeRegistration::Interface(engine, "IAngelBehaviour")
    .Method("void Start()")
    .Method("void Update(float dt)");
```

Script implementation:

```angelscript
class Player : IAngelBehaviour {
    void Start() { }
    void Update(float dt) { }
}
```

### Global Functions

Register free functions callable from any script.

```cpp
engine->Namespace("Math");
srph::TypeRegistration::Global(engine)
    .Function("float sin(float)", [](float x) { return std::sin(x); })
    .Function("float cos(float)", [](float x) { return std::cos(x); })
    .Function("float lerp(float, float, float)", 
              [](float a, float b, float t) { return a + t * (b - a); });
```

---

## Script Loading

```cpp
srph::ScriptLoader loader(&engine);
loader.Module("Game")
      .LoadScript("scripts/player.as")
      .LoadScript("scripts/enemy.as");

if (loader.Build()) {
    // Compilation successful
} else {
    // Errors logged via MessageCallback
}
```

### ScriptLoader Methods

| Method | Description |
|--------|-------------|
| `Module(const std::string& name)` | Set target module name |
| `LoadScript(const std::string& path)` | Add script file to compilation |
| `bool Build()` | Compile all added scripts, returns success |

---

## Function Calling

Builder pattern for invoking script functions and methods.

### Calling Instance Methods

```cpp
srph::FunctionCaller(&engine)
    .Module("Game")
    .Function("void Update(float)", instanceHandle)
    .Push(deltaTime)
    .Call();
```

### Calling Global Functions

```cpp
srph::FunctionCaller(&engine)
    .Module("Game")
    .Function("void Initialize()")
    .Call();
```

### Optional Functions

```cpp
srph::FunctionCaller(&engine)
    .Module("Game")
    .Function("void OnCollision()", handle, srph::FunctionPolicy::Optional)
    .Call();
// No error if function doesn't exist
```

### Getting Return Values

```cpp
srph::FunctionResult result = srph::FunctionCaller(&engine)
    .Module("Game")
    .Factory("Enemy@ Enemy(const vec3&in)", typeInfo)
    .Push(spawnPosition)
    .Call(srph::ReturnType::Object);

asIScriptObject* obj = std::get<asIScriptObject*>(result.value);
```

### Return Types

`Byte`, `Word`, `DWord`, `QWord`, `Float`, `Double`, `Object`

### FunctionCaller Methods

| Method | Description |
|--------|-------------|
| `Module(const std::string& name)` | Set module to search |
| `Function(const std::string& sig, InstanceHandle, FunctionPolicy)` | Prepare function/method call |
| `Factory(const std::string& decl, const std::string& typeName)` | Prepare factory (constructor) call |
| `Push<T>(T value)` | Push argument (primitives and registered types) |
| `void Call()` | Execute without return value |
| `FunctionResult Call(ReturnType)` | Execute and retrieve return value |

---

## Reflection

Retrieve property information from script instances at runtime.

### Basic Reflection

```cpp
std::vector<srph::ReflectedProperty> props = engine.Reflect(handle);
for (auto& prop : props) {
    // prop.type  - "int", "float", "vec3", etc.
    // prop.name  - property name
    // prop.data  - void* to actual data (read/write)
}
```

### Filtered Reflection (by Metadata)

```cpp
std::vector<srph::ReflectedProperty> serialized = engine.Reflect(handle, "Serialize");
std::vector<srph::ReflectedProperty> editorVisible = engine.Reflect(handle, "ShowInInspector");
```

Script with metadata:

```angelscript
class Player : IAngelBehaviour {
    [Serialize]
    int health = 100;
    
    [ShowInInspector]
    float speed = 5.0f;
    
    [Serialize] [ShowInInspector]
    vec3 position;
    
    int internalState;  // Not reflected with metadata filter
}
```

### ReflectedProperty Structure

```cpp
struct ReflectedProperty {
    std::string type;  // AngelScript type name
    std::string name;  // Property name
    void* data;        // Pointer to value (cast to appropriate type)
};
```

### Type Handling Example

```cpp
for (auto& field : engine.Reflect(handle, "ShowInInspector")) {
    if (field.type == "int") {
        int* val = static_cast<int*>(field.data);
        // Read or write *val
    } else if (field.type == "float") {
        float* val = static_cast<float*>(field.data);
    } else if (field.type == "vec3") {
        glm::vec3* val = static_cast<glm::vec3*>(field.data);
    }
}
```

### Metadata Retrieval

```cpp
std::vector<std::string> meta = engine.GetMetadata("Player", "health");
// Returns {"Serialize"} for the example above
```

---

## Instance Management

### Creating Instances

Default constructor:

```cpp
srph::InstanceHandle handle = engine.CreateInstance("Player", "Game");
```

Custom constructor via factory:

```cpp
srph::FunctionCaller factory(&engine);
factory.Module("Game").Factory("Enemy@ Enemy(const vec3&in)", "Enemy").Push(spawnPos);
srph::InstanceHandle handle = engine.CreateInstance(factory);
```

### Native Object Access

```cpp
asIScriptObject* obj = engine.GetNativeObject(handle);
```

Returns the underlying AngelScript object pointer. Use with caution, because caller is responsible for `AddRef()`/`Release()` if retaining the pointer.

### InstanceHandle

```cpp
struct InstanceHandle {
    InstanceID id;
    bool Valid() const;
};
```

### Instance Queries

```cpp
// Get type name
std::string name = engine.GetTypeName(handle);

// Get all active instances
std::vector<srph::InstanceHandle> instances = engine.GetInstances();

// Query classes implementing interface
std::vector<std::string> behaviours = engine.QueryImplementations("IAngelBehaviour", "Game");

// Query derived classes
std::vector<std::string> enemies = engine.QueryDerivedClasses("BaseEnemy", "Game");
```

### Cross-Script Communication

Register method to retrieve scripts from entities:

```cpp
srph::TypeRegistration::Class<UUID, srph::ClassType::Value>(engine, "Entity", asOBJ_POD)
    .Method("IAngelBehaviour@ GetScript(const string&in)",
            [](const UUID& entity, const std::string& scriptName) -> asIScriptObject* {
                // Lookup and return script instance
                // Must call obj->AddRef() before returning
            });
```

Script usage:

```angelscript
Entity target = GetTarget();
Health@ health = cast<Health>(target.GetScript("Health"));
health.Damage(10);
```

---

## Error Handling

### Compilation Errors

Errors reported via `MessageCallback` during `ScriptLoader::Build()`:

```
[ERROR] scripts/player.as:15,8: 'speed' is not declared
```

### Runtime Exceptions

Exceptions logged automatically with file, line, column:

```
Exception 'Null pointer access' in scripts/enemy.as:42,12 while calling method Enemy::Update
```

### Script Timeouts

Configure via `EngineConfiguration::scriptTimeoutMillis`. Register callback:

```cpp
engine.RegisterTimeoutCallback([]() {
    // Handle timeout (e.g., stop game, show error)
});
```

---

## Debugging

### VSCode Integration

Generate autocompletion data:

```cpp
engine.GeneratePredefined("path/to/as.predefined");
```

Attach DAP debugger:

```cpp
engine.AttachDebugger();
// Debugger listens for VSCode connection
```

Requires [AngelScript LSP extension](https://marketplace.visualstudio.com/items?itemName=sashi0034.angel-lsp) and custom debug adapter.