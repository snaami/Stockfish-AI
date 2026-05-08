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

#include "mcp_protocol.h"

#include "engine_coordinator.h"

#include <cctype>
#include <charconv>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Stockfish {

namespace {

class Json {
   public:
    using Array  = std::vector<Json>;
    using Object = std::map<std::string, Json>;

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool v) :
        value(v) {}
    Json(double v) :
        value(v) {}
    Json(std::string v) :
        value(std::move(v)) {}
    Json(const char* v) :
        value(std::string(v)) {}
    Json(Array v) :
        value(std::move(v)) {}
    Json(Object v) :
        value(std::move(v)) {}

    bool is_null() const { return std::holds_alternative<std::nullptr_t>(value); }
    bool is_string() const { return std::holds_alternative<std::string>(value); }
    bool is_object() const { return std::holds_alternative<Object>(value); }

    const std::string& as_string() const { return std::get<std::string>(value); }
    const Object&      as_object() const { return std::get<Object>(value); }

    const Json* get(std::string_view key) const {
        if (!is_object())
            return nullptr;

        const auto& object = as_object();
        auto        it     = object.find(std::string(key));
        return it == object.end() ? nullptr : &it->second;
    }

    std::string dump() const {
        struct Dumper {
            static std::string string(const std::string& s) {
                std::string out = "\"";

                for (char c : s)
                {
                    switch (c)
                    {
                    case '"' :
                        out += "\\\"";
                        break;
                    case '\\' :
                        out += "\\\\";
                        break;
                    case '\b' :
                        out += "\\b";
                        break;
                    case '\f' :
                        out += "\\f";
                        break;
                    case '\n' :
                        out += "\\n";
                        break;
                    case '\r' :
                        out += "\\r";
                        break;
                    case '\t' :
                        out += "\\t";
                        break;
                    default :
                        if (static_cast<unsigned char>(c) < 0x20)
                        {
                            constexpr char hex[] = "0123456789abcdef";
                            out += "\\u00";
                            out += hex[(c >> 4) & 0x0F];
                            out += hex[c & 0x0F];
                        }
                        else
                            out += c;
                    }
                }

                out += '"';
                return out;
            }
        };

        if (std::holds_alternative<std::nullptr_t>(value))
            return "null";
        if (std::holds_alternative<bool>(value))
            return std::get<bool>(value) ? "true" : "false";
        if (std::holds_alternative<double>(value))
        {
            std::stringstream ss;
            ss << std::get<double>(value);
            return ss.str();
        }
        if (std::holds_alternative<std::string>(value))
            return Dumper::string(std::get<std::string>(value));
        if (std::holds_alternative<Array>(value))
        {
            std::string out = "[";
            bool        first = true;
            for (const auto& item : std::get<Array>(value))
            {
                if (!first)
                    out += ',';
                first = false;
                out += item.dump();
            }
            out += ']';
            return out;
        }

        std::string out = "{";
        bool        first = true;
        for (const auto& [key, item] : std::get<Object>(value))
        {
            if (!first)
                out += ',';
            first = false;
            out += Dumper::string(key);
            out += ':';
            out += item.dump();
        }
        out += '}';
        return out;
    }

   private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value = nullptr;
};

class JsonParser {
   public:
    explicit JsonParser(std::string_view input) :
        text(input) {}

    std::optional<Json> parse() {
        auto value = parse_value();
        skip_ws();
        if (pos != text.size())
            return std::nullopt;
        return value;
    }

   private:
    void skip_ws() {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;
    }

    bool consume(char c) {
        skip_ws();
        if (pos >= text.size() || text[pos] != c)
            return false;
        ++pos;
        return true;
    }

    std::optional<Json> parse_value() {
        skip_ws();
        if (pos >= text.size())
            return std::nullopt;

        char c = text[pos];
        if (c == '"')
            return parse_string();
        if (c == '{')
            return parse_object();
        if (c == '[')
            return parse_array();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c)))
            return parse_number();
        if (text.substr(pos, 4) == "true")
        {
            pos += 4;
            return Json(true);
        }
        if (text.substr(pos, 5) == "false")
        {
            pos += 5;
            return Json(false);
        }
        if (text.substr(pos, 4) == "null")
        {
            pos += 4;
            return Json(nullptr);
        }

        return std::nullopt;
    }

    std::optional<Json> parse_string() {
        if (!consume('"'))
            return std::nullopt;

        std::string out;
        while (pos < text.size())
        {
            char c = text[pos++];
            if (c == '"')
                return Json(out);
            if (c != '\\')
            {
                out += c;
                continue;
            }

            if (pos >= text.size())
                return std::nullopt;

            char escaped = text[pos++];
            switch (escaped)
            {
            case '"' :
                out += '"';
                break;
            case '\\' :
                out += '\\';
                break;
            case '/' :
                out += '/';
                break;
            case 'b' :
                out += '\b';
                break;
            case 'f' :
                out += '\f';
                break;
            case 'n' :
                out += '\n';
                break;
            case 'r' :
                out += '\r';
                break;
            case 't' :
                out += '\t';
                break;
            case 'u' :
            {
                auto codepoint = parse_hex4();
                if (!codepoint)
                    return std::nullopt;

                if (*codepoint >= 0xD800 && *codepoint <= 0xDBFF)
                {
                    if (pos + 2 > text.size() || text[pos++] != '\\' || text[pos++] != 'u')
                        return std::nullopt;

                    auto low = parse_hex4();
                    if (!low || *low < 0xDC00 || *low > 0xDFFF)
                        return std::nullopt;

                    *codepoint = 0x10000 + ((*codepoint - 0xD800) << 10) + (*low - 0xDC00);
                }
                else if (*codepoint >= 0xDC00 && *codepoint <= 0xDFFF)
                    return std::nullopt;

                append_utf8(out, *codepoint);
                break;
            }
            default :
                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    std::optional<Json> parse_number() {
        const auto start = pos;
        if (text[pos] == '-')
            ++pos;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
            ++pos;
        if (pos < text.size() && text[pos] == '.')
        {
            ++pos;
            if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos])))
                return std::nullopt;
            while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
                ++pos;
        }
        if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E'))
        {
            ++pos;
            if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
                ++pos;
            if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos])))
                return std::nullopt;
            while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
                ++pos;
        }

        double value = 0;
        auto   number = text.substr(start, pos - start);
        auto [ptr, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
        if (ec != std::errc() || ptr != number.data() + number.size())
            return std::nullopt;

        return Json(value);
    }

    static int hex_value(char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return 10 + c - 'a';
        if (c >= 'A' && c <= 'F')
            return 10 + c - 'A';
        return -1;
    }

    std::optional<char32_t> parse_hex4() {
        if (pos + 4 > text.size())
            return std::nullopt;

        char32_t value = 0;
        for (int i = 0; i < 4; ++i)
        {
            int digit = hex_value(text[pos++]);
            if (digit < 0)
                return std::nullopt;
            value = (value << 4) | char32_t(digit);
        }

        return value;
    }

    static void append_utf8(std::string& out, char32_t codepoint) {
        if (codepoint <= 0x7F)
            out += char(codepoint);
        else if (codepoint <= 0x7FF)
        {
            out += char(0xC0 | (codepoint >> 6));
            out += char(0x80 | (codepoint & 0x3F));
        }
        else if (codepoint <= 0xFFFF)
        {
            out += char(0xE0 | (codepoint >> 12));
            out += char(0x80 | ((codepoint >> 6) & 0x3F));
            out += char(0x80 | (codepoint & 0x3F));
        }
        else
        {
            out += char(0xF0 | (codepoint >> 18));
            out += char(0x80 | ((codepoint >> 12) & 0x3F));
            out += char(0x80 | ((codepoint >> 6) & 0x3F));
            out += char(0x80 | (codepoint & 0x3F));
        }
    }

    std::optional<Json> parse_array() {
        if (!consume('['))
            return std::nullopt;

        Json::Array out;
        skip_ws();
        if (consume(']'))
            return Json(out);

        do
        {
            auto value = parse_value();
            if (!value)
                return std::nullopt;
            out.push_back(*value);
        } while (consume(','));

        return consume(']') ? std::optional<Json>(Json(out)) : std::nullopt;
    }

    std::optional<Json> parse_object() {
        if (!consume('{'))
            return std::nullopt;

        Json::Object out;
        skip_ws();
        if (consume('}'))
            return Json(out);

        do
        {
            auto key = parse_string();
            if (!key || !key->is_string() || !consume(':'))
                return std::nullopt;

            auto value = parse_value();
            if (!value)
                return std::nullopt;

            out.emplace(key->as_string(), *value);
        } while (consume(','));

        return consume('}') ? std::optional<Json>(Json(out)) : std::nullopt;
    }

    std::string_view text;
    size_t           pos = 0;
};

Json jsonrpc_response(const Json& id, Json result) {
    return Json::Object{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

Json jsonrpc_error(const Json& id, int code, std::string message) {
    Json::Object error{{"code", double(code)}, {"message", std::move(message)}};
    return Json::Object{{"jsonrpc", "2.0"}, {"id", id}, {"error", std::move(error)}};
}

Json server_capabilities() {
    return Json::Object{
      {"tools", Json::Object{{"listChanged", false}}},
      {"resources", Json::Object{{"subscribe", true}, {"listChanged", false}}}};
}

Json initialize_result(const Json* params) {
    std::string protocolVersion = "2025-11-25";

    if (params)
        if (const auto* requested = params->get("protocolVersion"))
            if (requested->is_string()
                && (requested->as_string() == "2025-11-25" || requested->as_string() == "2025-06-18"))
                protocolVersion = requested->as_string();

    return Json::Object{
      {"protocolVersion", protocolVersion},
      {"capabilities", server_capabilities()},
      {"serverInfo", Json::Object{{"name", "stockfish-embedded-mcp"}, {"version", "0.1.0"}}}};
}

MCPProtocolResponse response_with_json(const Json& json) {
    MCPProtocolResponse response;
    response.body = json.dump();
    return response;
}

Json resource_descriptor(std::string uri, std::string name, std::string description,
                         std::string mimeType = "application/json") {
    return Json::Object{{"uri", std::move(uri)},
                        {"name", std::move(name)},
                        {"description", std::move(description)},
                        {"mimeType", std::move(mimeType)}};
}

Json resources_list_result() {
    return Json::Object{{"resources",
                         Json::Array{
                           resource_descriptor("stockfish://state/current", "Current state",
                                               "Current shared Stockfish position and search state."),
                           resource_descriptor("stockfish://analysis/latest", "Latest analysis",
                                               "Latest known analysis snapshot."),
                           resource_descriptor("stockfish://engine/options", "Engine options",
                                               "Current UCI engine options."),
                           resource_descriptor("stockfish://events/recent", "Recent events",
                                               "Recent Stockfish state transition events.")}}};
}

std::string side_to_move_from_fen(const std::string& fen) {
    std::istringstream is(fen);
    std::string        board;
    std::string        side;
    is >> board >> side;

    if (side == "w")
        return "white";
    if (side == "b")
        return "black";
    return "unknown";
}

std::string state_resource(EngineCoordinator& coordinator) {
    const auto fen = coordinator.fen();
    return Json(Json::Object{{"fen", fen},
                             {"sideToMove", side_to_move_from_fen(fen)},
                             {"moveHistory", Json::Array{}},
                             {"searchActive", coordinator.search_active()},
                             {"controller", coordinator.controller()},
                             {"legalMoveCount", nullptr}})
      .dump();
}

Json::Array analysis_candidates(const MCPAnalysisSnapshot& snapshot) {
    Json::Array candidates;

    for (const auto& candidate : snapshot.candidates)
        candidates.emplace_back(Json::Object{{"multipv", double(candidate.multiPV)},
                                             {"depth", double(candidate.depth)},
                                             {"seldepth", double(candidate.selDepth)},
                                             {"score", candidate.score},
                                             {"bound", candidate.bound},
                                             {"wdl", candidate.wdl},
                                             {"nodes", double(candidate.nodes)},
                                             {"nps", double(candidate.nps)},
                                             {"hashfull", double(candidate.hashfull)},
                                             {"tbhits", double(candidate.tbHits)},
                                             {"timeMs", double(candidate.timeMs)},
                                             {"pv", candidate.pv}});

    return candidates;
}

std::string latest_analysis_resource(EngineCoordinator& coordinator) {
    const auto snapshot = coordinator.analysis_snapshot();

    return Json(Json::Object{{"active", snapshot.active},
                             {"source", snapshot.source},
                             {"fen", snapshot.fen},
                             {"bestmove", snapshot.bestmove},
                             {"ponder", snapshot.ponder},
                             {"candidates", analysis_candidates(snapshot)}})
      .dump();
}

std::string engine_options_resource(EngineCoordinator& coordinator) {
    return Json(Json::Object{{"format", "uci"}, {"uciOptionsText", coordinator.options_as_uci()}})
      .dump();
}

std::string recent_events_resource(EngineCoordinator& coordinator) {
    Json::Array events;

    for (const auto& event : coordinator.recent_events())
        events.emplace_back(Json::Object{{"type", event.type},
                                         {"source", event.source},
                                         {"fen", event.fen},
                                         {"detail", event.detail}});

    return Json(events).dump();
}

std::optional<std::string> resource_text(std::string_view uri, EngineCoordinator& coordinator) {
    if (uri == "stockfish://state/current")
        return state_resource(coordinator);
    if (uri == "stockfish://analysis/latest")
        return latest_analysis_resource(coordinator);
    if (uri == "stockfish://engine/options")
        return engine_options_resource(coordinator);
    if (uri == "stockfish://events/recent")
        return recent_events_resource(coordinator);

    return std::nullopt;
}

Json resources_read_result(std::string uri, std::string text) {
    return Json::Object{
      {"contents",
       Json::Array{Json::Object{{"uri", std::move(uri)},
                                {"mimeType", "application/json"},
                                {"text", std::move(text)}}}}};
}

}  // namespace

MCPProtocolResponse handle_mcp_post(const std::string& body, EngineCoordinator& coordinator) {
    auto parsed = JsonParser(body).parse();
    if (!parsed)
        return response_with_json(jsonrpc_error(nullptr, -32700, "Parse error"));

    const Json request = *parsed;
    const Json* id     = request.get("id");
    const Json* method = request.get("method");

    if (!request.is_object() || !method || !method->is_string())
        return response_with_json(jsonrpc_error(id ? *id : Json(nullptr), -32600, "Invalid Request"));

    const std::string& methodName = method->as_string();

    if (methodName == "notifications/initialized")
        return {202, "", "application/json"};

    if (!id)
        return {202, "", "application/json"};

    if (methodName == "initialize")
        return response_with_json(jsonrpc_response(*id, initialize_result(request.get("params"))));

    if (methodName == "ping")
        return response_with_json(jsonrpc_response(*id, Json::Object{}));

    if (methodName == "resources/list")
        return response_with_json(jsonrpc_response(*id, resources_list_result()));

    if (methodName == "resources/read")
    {
        const Json* params = request.get("params");
        const Json* uri    = params ? params->get("uri") : nullptr;

        if (!uri || !uri->is_string())
            return response_with_json(jsonrpc_error(*id, -32602, "Missing resource uri"));

        auto text = resource_text(uri->as_string(), coordinator);
        if (!text)
            return response_with_json(jsonrpc_error(*id, -32002, "Resource not found"));

        return response_with_json(
          jsonrpc_response(*id, resources_read_result(uri->as_string(), std::move(*text))));
    }

    return response_with_json(jsonrpc_error(*id, -32601, "Method not found"));
}

}  // namespace Stockfish
