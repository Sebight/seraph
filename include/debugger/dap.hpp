#pragma once
#include <thread>
#include <unordered_map>
#include <optional>
#include <asio/asio.hpp>
#include <nlohmann/json.hpp>

#include "debug_adapter.hpp"

namespace srph::debugger
{
class Debugger;

struct ObjectReference
{
    int typeId;
    void* ptr;
};

class DAP : public IDebugAdapter
{
public:
    DAP() = default;
    void Start() override;
    void Stop() override;
    void AttachDebugger(Debugger* debugger) override { m_debugger = debugger; }
    void OnBreakpointHit(std::string file, int line) override;

private:
    void ServerLoop();

    void ClientSession();

    std::optional<nlohmann::json> ReadMessage();
    bool SendMessage(const nlohmann::json& message);
    bool SendEvent(const std::string& event, const nlohmann::json& body = {});

    std::optional<nlohmann::json> HandleCommand(const nlohmann::json& request);

    std::optional<nlohmann::json> HandleInitialize(const nlohmann::json& request);
    nlohmann::json HandleAttach(const nlohmann::json& request);
    nlohmann::json HandleSetBreakpoints(const nlohmann::json& request);
    nlohmann::json HandleConfigurationDone(const nlohmann::json& request);
    nlohmann::json HandleThreads(const nlohmann::json& request);
    nlohmann::json HandleContinue(const nlohmann::json& request);
    nlohmann::json HandleStackTrace(const nlohmann::json& request);
    nlohmann::json HandleScopes(const nlohmann::json& request);
    nlohmann::json HandleVariables(const nlohmann::json& request);
    nlohmann::json HandleNext(const nlohmann::json& request);
    nlohmann::json HandleStepIn(const nlohmann::json& request);
    nlohmann::json HandleStepOut(const nlohmann::json& request);
    std::optional<nlohmann::json> HandleDisconnect(const nlohmann::json& request);

    void CloseSocket();

private:
    static constexpr short DEFAULT_PORT = 5050;

    std::unique_ptr<asio::ip::tcp::socket> m_socket;
    std::unique_ptr<asio::ip::tcp::acceptor> m_acceptor;
    std::unique_ptr<asio::io_context> m_io;

    std::thread m_thread;
    std::mutex m_socketMutex;

    Debugger* m_debugger = nullptr;

    std::unordered_map<int, ObjectReference> m_objectReferences;
    std::atomic<int> m_seqCounter{1};
    std::atomic<bool> m_running{true};
};

}  // namespace srph::debugger