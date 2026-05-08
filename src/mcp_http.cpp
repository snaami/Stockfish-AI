/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mcp_http.h"
#include "mcp_protocol.h"

#define CPPHTTPLIB_NO_EXCEPTIONS
#include "thirdparty/cpp-httplib/httplib.h"

#include <sstream>
#include <utility>

namespace Stockfish {

MCPHttpServer::MCPHttpServer(MCPConfig cfg, EngineCoordinator& engineCoordinator) :
    config(std::move(cfg)),
    coordinator(engineCoordinator),
    server(std::make_unique<httplib::Server>()) {

    server->Get("/mcp", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("Stockfish MCP HTTP endpoint is running.\n", "text/plain");
    });

    server->Post("/mcp", [this](const httplib::Request& req, httplib::Response& res) {
        auto response = handle_mcp_post(req.body, coordinator);
        res.status    = response.status;

        if (!response.body.empty())
            res.set_content(response.body, response.contentType);
    });

    server->set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404)
            res.set_content("Not found.\n", "text/plain");
    });
}

MCPHttpServer::~MCPHttpServer() { stop(); }

bool MCPHttpServer::start(std::string& error) {
    if (!server->bind_to_port(config.host, config.port))
    {
        std::stringstream ss;
        ss << "Unable to bind MCP HTTP server to " << config.host << ':' << config.port;
        error = ss.str();
        return false;
    }

    serverThread = std::thread([this]() { server->listen_after_bind(); });
    server->wait_until_ready();

    return server->is_running();
}

void MCPHttpServer::stop() {
    if (server)
        server->stop();

    if (serverThread.joinable())
        serverThread.join();
}

}  // namespace Stockfish
