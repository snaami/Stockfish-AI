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

#include <iostream>
#include <memory>

#include "bitboard.h"
#include "mcp_config.h"
#include "mcp_http.h"
#include "misc.h"
#include "position.h"
#include "tune.h"
#include "uci.h"

using namespace Stockfish;

#ifdef UNIVERSAL_BINARY
namespace Stockfish {

int main(int argc, char* argv[]);
#endif

int main(int argc, char* argv[]) {
    auto mcpConfig = parse_mcp_config(argc, argv);

    if (mcpConfig.error)
    {
        std::cerr << "MCP configuration error: " << *mcpConfig.error << std::endl;
        return 1;
    }

    std::cout << engine_info() << std::endl;

    Bitboards::init();
    Position::init();

    if (mcpConfig.config.enabled)
        std::cerr << "MCP HTTP configured on " << mcpConfig.config.host << ':'
                  << mcpConfig.config.port << " with control mode "
                  << mcpConfig.config.control_mode_name() << std::endl;

    auto uci = std::make_unique<UCIEngine>(int(mcpConfig.uciArgv.size()), mcpConfig.uciArgv.data());

    Tune::init(uci->engine_options());

    std::unique_ptr<MCPHttpServer> mcpServer;
    if (mcpConfig.config.enabled)
    {
        std::string error;
        mcpServer = std::make_unique<MCPHttpServer>(mcpConfig.config, uci->coordinator());

        if (!mcpServer->start(error))
        {
            std::cerr << "MCP HTTP startup error: " << error << std::endl;
            return 1;
        }
    }

    uci->loop();

    if (mcpServer)
        mcpServer->stop();

    return 0;
}

#ifdef UNIVERSAL_BINARY
}
#endif
