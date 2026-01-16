#pragma once
#include <functional>

namespace srph
{
enum class InstanceID : uint64_t
{
    Invalid = 0
};

struct InstanceHandle
{
    InstanceID id = InstanceID::Invalid;
    bool Valid() const { return id != InstanceID::Invalid; }

    bool operator==(const InstanceHandle& other) const noexcept { return id == other.id; }
};
}  // namespace srph

namespace std
{
template <>
struct hash<srph::InstanceHandle>
{
    size_t operator()(const srph::InstanceHandle& h) const noexcept
    {
        return std::hash<uint64_t>()(static_cast<uint64_t>(h.id));
    }
};
}  // namespace std