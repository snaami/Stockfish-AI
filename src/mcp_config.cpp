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

#include "mcp_config.h"

#include <charconv>
#include <string_view>

namespace Stockfish {

namespace {

bool is_mcp_flag(std::string_view arg) {
    return arg == "--mcp-http" || arg == "--mcp-host" || arg == "--mcp-port"
        || arg == "--mcp-control";
}

bool has_value(int argc, int i, char* argv[]) {
    return i + 1 < argc && argv[i + 1][0] != '-';
}

bool parse_port(std::string_view text, std::uint16_t& port) {
    unsigned value = 0;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (ec != std::errc() || ptr != text.data() + text.size() || value == 0 || value > 65535)
        return false;

    port = static_cast<std::uint16_t>(value);
    return true;
}

bool parse_endpoint(std::string_view endpoint, MCPConfig& config) {
    const auto colon = endpoint.rfind(':');

    if (colon == std::string_view::npos)
    {
        config.host = std::string(endpoint);
        return !config.host.empty();
    }

    config.host = std::string(endpoint.substr(0, colon));
    return !config.host.empty() && parse_port(endpoint.substr(colon + 1), config.port);
}

std::optional<MCPConfig::ControlMode> parse_control_mode(std::string_view mode) {
    if (mode == "observe")
        return MCPConfig::ControlMode::Observe;
    if (mode == "advise")
        return MCPConfig::ControlMode::Advise;
    if (mode == "shared")
        return MCPConfig::ControlMode::Shared;
    if (mode == "agent")
        return MCPConfig::ControlMode::Agent;

    return std::nullopt;
}

bool local_host(std::string_view host) {
    return host == "127.0.0.1" || host == "localhost";
}

}  // namespace

std::string MCPConfig::control_mode_name() const {
    switch (control)
    {
    case ControlMode::Observe :
        return "observe";
    case ControlMode::Advise :
        return "advise";
    case ControlMode::Shared :
        return "shared";
    case ControlMode::Agent :
        return "agent";
    }

    return "observe";
}

MCPConfigParseResult parse_mcp_config(int argc, char* argv[]) {
    MCPConfigParseResult result;
    result.uciArgv.push_back(argv[0]);

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg(argv[i]);

        if (!is_mcp_flag(arg))
        {
            result.uciArgv.push_back(argv[i]);
            continue;
        }

        if (arg == "--mcp-http")
        {
            result.config.enabled = true;

            if (has_value(argc, i, argv))
            {
                if (!parse_endpoint(argv[++i], result.config))
                    result.error = "Invalid --mcp-http endpoint. Expected host or host:port.";
            }
        }
        else if (arg == "--mcp-host")
        {
            if (!has_value(argc, i, argv))
                result.error = "--mcp-host requires a value.";
            else
                result.config.host = argv[++i];
        }
        else if (arg == "--mcp-port")
        {
            if (!has_value(argc, i, argv))
                result.error = "--mcp-port requires a value.";
            else if (!parse_port(argv[++i], result.config.port))
                result.error = "Invalid --mcp-port value. Expected 1-65535.";
        }
        else if (arg == "--mcp-control")
        {
            if (!has_value(argc, i, argv))
                result.error = "--mcp-control requires a value.";
            else
            {
                auto mode = parse_control_mode(argv[++i]);
                if (!mode)
                    result.error = "Invalid --mcp-control value. Expected observe, advise, shared, or agent.";
                else
                    result.config.control = *mode;
            }
        }

        if (result.error)
            break;
    }

    if (!result.error && result.config.enabled && !local_host(result.config.host))
        result.error = "MCP HTTP currently only binds to 127.0.0.1 or localhost.";

    return result;
}

}  // namespace Stockfish
