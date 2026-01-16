#include "srph_common.hpp"
#include "debugger/dap.hpp"
#include "debugger/debugger.hpp"
#include "script_reflection.hpp"

#include <filesystem>

using json = nlohmann::json;

namespace srph::debugger
{

void DAP::Start()
{
    m_running.store(true);
    m_seqCounter.store(1);

    m_io = std::make_unique<asio::io_context>();
    m_acceptor = std::make_unique<asio::ip::tcp::acceptor>(*m_io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), DEFAULT_PORT));

    m_thread = std::thread([this]() { ServerLoop(); });
}

void DAP::Stop()
{
    m_running.store(false);

    if (m_acceptor)
    {
        asio::error_code ec;
        m_acceptor->close(ec);
    }

    CloseSocket();

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    m_acceptor.reset();
    if (m_io)
    {
        m_io->stop();
        m_io.reset();
    }
}

void DAP::CloseSocket()
{
    std::lock_guard<std::mutex> lock(m_socketMutex);
    if (m_socket)
    {
        asio::error_code ec;
        m_socket->shutdown(asio::socket_base::shutdown_both, ec);
        m_socket->close(ec);
        m_socket.reset();
    }
}

void DAP::ServerLoop()
{
    Log::Info("Seraph DAP server listening on port {}", DEFAULT_PORT);

    while (m_running.load())
    {
        {
            std::lock_guard<std::mutex> lock(m_socketMutex);
            m_socket = std::make_unique<asio::ip::tcp::socket>(*m_io);
        }

        asio::error_code ec;
        m_acceptor->accept(*m_socket, ec);

        if (ec || !m_running.load())
        {
            if (m_running.load())
            {
                Log::Error("Accept failed: {}", ec.message());
            }
            break;
        }

        Log::Info("DAP client connected");

        ClientSession();

        CloseSocket();
        m_objectReferences.clear();

        Log::Info("DAP client disconnected");
    }

    Log::Info("DAP server stopped");
}

void DAP::ClientSession()
{
    while (m_running.load())
    {
        auto request = ReadMessage();
        if (!request)
        {
            break;
        }

        auto response = HandleCommand(*request);

        if (response)
        {
            json fullResponse = {{"seq", m_seqCounter++},
                                 {"type", "response"},
                                 {"request_seq", (*request)["seq"]},
                                 {"success", true},
                                 {"command", (*request)["command"]},
                                 {"body", *response}};

            if (!SendMessage(fullResponse))
            {
                break;
            }
        }

        if ((*request)["command"] == "disconnect")
        {
            break;
        }
    }
}

std::optional<json> DAP::ReadMessage()
{
    asio::ip::tcp::socket* sock;
    {
        std::lock_guard<std::mutex> lock(m_socketMutex);
        if (!m_socket || !m_socket->is_open())
        {
            return std::nullopt;
        }
        sock = m_socket.get();
    }

    try
    {
        asio::streambuf headerBuffer;
        asio::error_code ec;
        asio::read_until(*sock, headerBuffer, "\r\n\r\n", ec);

        if (ec)
        {
            if (ec != asio::error::eof && ec != asio::error::operation_aborted)
            {
                Log::Error("Failed to read DAP header: {}", ec.message());
            }
            return std::nullopt;
        }

        std::string headers;
        headers.resize(headerBuffer.size());
        headerBuffer.sgetn(&headers[0], headerBuffer.size());

        size_t contentLengthPos = headers.find("Content-Length: ");
        if (contentLengthPos == std::string::npos)
        {
            Log::Error("DAP message missing Content-Length header");
            return std::nullopt;
        }

        // TODO(Seb): I think this could use some refactor
        size_t valueStart = contentLengthPos + 16;
        size_t valueEnd = headers.find("\r\n", valueStart);
        int contentLength = std::stoi(headers.substr(valueStart, valueEnd - valueStart));

        size_t headerEnd = headers.find("\r\n\r\n") + 4;
        std::string body = headers.substr(headerEnd);

        if (body.size() < static_cast<size_t>(contentLength))
        {
            size_t remaining = contentLength - body.size();
            std::string rest(remaining, '\0');
            asio::read(*sock, asio::buffer(rest), ec);

            if (ec)
            {
                Log::Error("Failed to read DAP body: {}", ec.message());
                return std::nullopt;
            }

            body += rest;
        }

        return json::parse(body);
    }
    catch (const std::exception& e)
    {
        Log::Error("Exception reading DAP message: {}", e.what());
        return std::nullopt;
    }
}

bool DAP::SendMessage(const json& message)
{
    std::lock_guard<std::mutex> lock(m_socketMutex);

    if (!m_socket || !m_socket->is_open())
    {
        return false;
    }

    try
    {
        std::string body = message.dump();
        std::string fullMessage = "Content-Length: " + std::to_string(body.length()) + "\r\n\r\n" + body;

        asio::error_code ec;
        asio::write(*m_socket, asio::buffer(fullMessage), ec);

        if (ec)
        {
            Log::Error("Failed to send DAP message: {}", ec.message());
            return false;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        Log::Error("Exception sending DAP message: {}", e.what());
        return false;
    }
}

bool DAP::SendEvent(const std::string& event, const json& body)
{
    json eventMessage = {{"seq", m_seqCounter++}, {"type", "event"}, {"event", event}};

    if (!body.empty())
    {
        eventMessage["body"] = body;
    }

    return SendMessage(eventMessage);
}

void DAP::OnBreakpointHit(std::string /*file*/, int /*line*/)
{
    json body = {{"reason", "breakpoint"}, {"threadId", 1}, {"allThreadsStopped", true}};

    SendEvent("stopped", body);
}

std::optional<json> DAP::HandleCommand(const json& request)
{
    std::string command = request["command"];

    if (command == "initialize") return HandleInitialize(request);
    if (command == "attach") return HandleAttach(request);
    if (command == "setBreakpoints") return HandleSetBreakpoints(request);
    if (command == "configurationDone") return HandleConfigurationDone(request);
    if (command == "threads") return HandleThreads(request);
    if (command == "continue") return HandleContinue(request);
    if (command == "stackTrace") return HandleStackTrace(request);
    if (command == "scopes") return HandleScopes(request);
    if (command == "variables") return HandleVariables(request);
    if (command == "next") return HandleNext(request);
    if (command == "stepIn") return HandleStepIn(request);
    if (command == "stepOut") return HandleStepOut(request);
    if (command == "disconnect") return HandleDisconnect(request);

    Log::Warn("Unhandled DAP command: {}", command);
    return json::object();
}

std::optional<json> DAP::HandleInitialize(const json& request)
{
    json response = {{"supportsConfigurationDoneRequest", true}, {"supportsSetVariable", false}};

    json fullResponse = {{"seq", m_seqCounter++},
                         {"type", "response"},
                         {"request_seq", request["seq"]},
                         {"success", true},
                         {"command", "initialize"},
                         {"body", response}};
    SendMessage(fullResponse);

    SendEvent("initialized");

    return std::nullopt;
}

json DAP::HandleAttach(const json& /*request*/) { return json::object(); }

json DAP::HandleSetBreakpoints(const json& request)
{
    auto args = request["arguments"];
    std::string source = args["source"]["path"];
    auto lines = args["breakpoints"];

    std::string normalized = source;
    std::transform(normalized.begin(),
                   normalized.end(),
                   normalized.begin(),
                   [](char c) { return c == '\\' ? '/' : static_cast<char>(std::tolower(c)); });

    auto& breakpoints = m_debugger->GetBreakpoints();
    breakpoints[normalized].clear();

    std::vector<json> confirmedBPs;
    for (const auto& bp : lines)
    {
        int line = bp["line"];
        breakpoints[normalized].push_back(line);
        confirmedBPs.push_back({{"verified", true}, {"line", line}});
    }

    return {{"breakpoints", confirmedBPs}};
}

json DAP::HandleConfigurationDone(const json& /*request*/) { return json::object(); }

json DAP::HandleThreads(const json& /*request*/) { return {{"threads", {{{"id", 1}, {"name", "Main Thread"}}}}}; }

json DAP::HandleContinue(const json& /*request*/)
{
    m_debugger->Continue();
    return {{"allThreadsContinued", true}};
}

json DAP::HandleStackTrace(const json& /*request*/)
{
    std::vector<json> frames;

    if (m_debugger->m_currentContext)
    {
        asIScriptContext* ctx = m_debugger->m_currentContext;
        asUINT stackSize = ctx->GetCallstackSize();

        for (asUINT i = 0; i < stackSize; i++)
        {
            asIScriptFunction* func = ctx->GetFunction(i);
            const char* section = nullptr;
            int column = 0;
            int line = ctx->GetLineNumber(i, &column, &section);

            std::string funcName = func ? func->GetName() : "unknown";
            std::string filePath = section ? section : "";
            std::string fileName = filePath.empty() ? "" : std::filesystem::path(filePath).filename().string();

            frames.push_back({{"id", i},
                              {"name", funcName},
                              {"line", line},
                              {"column", column},
                              {"source", {{"name", fileName}, {"path", filePath}}}});
        }
    }

    return {{"stackFrames", frames}, {"totalFrames", frames.size()}};
}

json DAP::HandleScopes(const json& request)
{
    int frameId = request["arguments"]["frameId"];

    return {{"scopes",
             {{{"name", "Locals"}, {"variablesReference", frameId * 1000 + 1}, {"expensive", false}},
              {{"name", "Globals"}, {"variablesReference", frameId * 1000 + 2}, {"expensive", false}}}}};
}

json DAP::HandleVariables(const json& request)
{
    int varRef = request["arguments"]["variablesReference"];
    int frameId = varRef / 1000;
    // 1=locals, 2=globals
    int scopeType = varRef % 1000;

    std::vector<json> variables;
    asIScriptContext* ctx = m_debugger->m_currentContext;

    if (!ctx)
    {
        return {{"variables", variables}};
    }

    int typeId = ctx->GetThisTypeId(frameId);
    void* varPointer = ctx->GetThisPointer(frameId);
    if (typeId && varPointer)
    {
        asITypeInfo* typeInfo = ctx->GetEngine()->GetTypeInfoById(typeId);
        int thisVarRef = (frameId * 1000 + 3) + 1000000;

        variables.push_back({{"name", "this"},
                             {"value", typeInfo ? typeInfo->GetName() : "object"},
                             {"type", reflection::GetTypename(typeId, ctx->GetEngine())},
                             {"variablesReference", thisVarRef}});

        m_objectReferences[thisVarRef] = {typeId, varPointer};
    }

    if (varRef >= 1000000)
    {
        auto it = m_objectReferences.find(varRef);
        if (it != m_objectReferences.end())
        {
            int objTypeId = it->second.typeId;
            void* objPtr = it->second.ptr;

            asITypeInfo* typeInfo = ctx->GetEngine()->GetTypeInfoById(objTypeId);
            if (typeInfo)
            {
                int propCount = typeInfo->GetPropertyCount();
                for (int i = 0; i < propCount; i++)
                {
                    const char* propName;
                    int propTypeId;
                    int offset;
                    typeInfo->GetProperty(i, &propName, &propTypeId, nullptr, nullptr, &offset);

                    void* propPtr = static_cast<char*>(objPtr) + offset;
                    std::string value = reflection::GetValue(propTypeId, propPtr, ctx->GetEngine());

                    variables.push_back({{"name", propName},
                                         {"value", value},
                                         {"type", ctx->GetEngine()->GetTypeDeclaration(propTypeId)},
                                         {"variablesReference", 0}});
                }
            }
        }
    }
    else if (scopeType == 1)
    {
        int varCount = ctx->GetVarCount(frameId);

        for (int i = 0; i < varCount; i++)
        {
            const char* name;
            int varTypeId;
            SRPH_VERIFY(ctx->GetVar(i, frameId, &name, &varTypeId), "Failed to read var in scope.")

            void* varPtr = ctx->GetAddressOfVar(i, frameId);
            std::string value = reflection::GetValue(varTypeId, varPtr, ctx->GetEngine());
            std::string type = reflection::GetTypename(varTypeId, ctx->GetEngine());
            const char* typeDecl = ctx->GetEngine()->GetTypeDeclaration(varTypeId);

            if (name && name[0] != '\0' && ctx->IsVarInScope(i, frameId))
            {
                variables.push_back({{"name", name}, {"value", value}, {"type", typeDecl}, {"variablesReference", 0}});
            }
        }
    }
    else if (scopeType == 2)
    {
        asIScriptModule* module = ctx->GetFunction()->GetModule();
        if (module)
        {
            int globalCount = module->GetGlobalVarCount();
            for (int i = 0; i < globalCount; i++)
            {
                const char* name;
                int globalTypeId;
                module->GetGlobalVar(i, &name, nullptr, &globalTypeId);
                void* varPtr = module->GetAddressOfGlobalVar(i);

                std::string value = reflection::GetValue(globalTypeId, varPtr, ctx->GetEngine());
                std::string type = reflection::GetTypename(globalTypeId, ctx->GetEngine());

                variables.push_back({{"name", name}, {"value", value}, {"type", type}, {"variablesReference", 0}});
            }
        }
    }

    return {{"variables", variables}};
}

json DAP::HandleNext(const json& /*request*/)
{
    m_debugger->m_stepMode = StepMode::Over;
    m_debugger->m_currentStackDepth = m_debugger->m_currentContext->GetCallstackSize();
    m_debugger->Continue();

    return json::object();
}

json DAP::HandleStepIn(const json& /*request*/)
{
    m_debugger->m_stepMode = StepMode::In;
    m_debugger->Continue();

    return json::object();
}

json DAP::HandleStepOut(const json& /*request*/)
{
    m_debugger->m_stepMode = StepMode::Out;
    m_debugger->m_currentStackDepth = m_debugger->m_currentContext->GetCallstackSize() - 1;
    m_debugger->Continue();

    return json::object();
}

std::optional<json> DAP::HandleDisconnect(const json& request)
{
    json response = {{"seq", m_seqCounter++},
                     {"type", "response"},
                     {"request_seq", request["seq"]},
                     {"success", true},
                     {"command", "disconnect"},
                     {"body", json::object()}};
    SendMessage(response);

    m_debugger->m_breakpoints.clear();
    m_debugger->Continue();

    return std::nullopt;
}

}  // namespace srph::debugger
