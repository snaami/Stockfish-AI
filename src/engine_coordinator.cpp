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

#include "engine_coordinator.h"

#include <algorithm>
#include <mutex>
#include <sstream>

namespace Stockfish {

EngineCoordinator::EngineCoordinator(std::optional<std::string> path) :
    engine(std::move(path)) {}

std::uint64_t EngineCoordinator::perft(const std::string& fen, Depth depth, bool isChess960) {
    std::lock_guard<std::mutex> lock(mutex);
    return engine.perft(fen, depth, isChess960);
}

void EngineCoordinator::go(Search::LimitsType& limits) {
    std::lock_guard<std::mutex> lock(mutex);
    engine.go(limits);
}

void EngineCoordinator::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    engine.stop();
}

void EngineCoordinator::wait_for_search_finished() {
    std::lock_guard<std::mutex> lock(mutex);
    engine.wait_for_search_finished();
}

std::optional<PositionSetError> EngineCoordinator::set_position(
  const std::string& fen, const std::vector<std::string>& moves) {
    std::lock_guard<std::mutex> lock(mutex);
    return engine.set_position(fen, moves);
}

void EngineCoordinator::set_ponderhit(bool b) {
    std::lock_guard<std::mutex> lock(mutex);
    engine.set_ponderhit(b);
}

void EngineCoordinator::search_clear() {
    std::lock_guard<std::mutex> lock(mutex);
    engine.search_clear();
}

void EngineCoordinator::flip() {
    std::lock_guard<std::mutex> lock(mutex);
    engine.flip();
}

void EngineCoordinator::set_on_update_no_moves(
  std::function<void(const Engine::InfoShort&)>&& f) {
    engine.set_on_update_no_moves(std::move(f));
}

void EngineCoordinator::set_on_update_full(std::function<void(const Engine::InfoFull&)>&& f) {
    engine.set_on_update_full(std::move(f));
}

void EngineCoordinator::set_on_iter(std::function<void(const Engine::InfoIter&)>&& f) {
    engine.set_on_iter(std::move(f));
}

void EngineCoordinator::set_on_bestmove(
  std::function<void(std::string_view, std::string_view)>&& f) {
    engine.set_on_bestmove(std::move(f));
}

void EngineCoordinator::set_on_verify_network(std::function<void(std::string_view)>&& f) {
    engine.set_on_verify_network(std::move(f));
}

void EngineCoordinator::load_network(const std::string& file) {
    std::lock_guard<std::mutex> lock(mutex);
    engine.load_network(file);
}

void EngineCoordinator::save_network(std::pair<std::optional<std::string>, std::string> file) {
    std::lock_guard<std::mutex> lock(mutex);
    engine.save_network(std::move(file));
}

void EngineCoordinator::trace_eval() const {
    std::lock_guard<std::mutex> lock(mutex);
    engine.trace_eval();
}

const OptionsMap& EngineCoordinator::get_options() const { return engine.get_options(); }
OptionsMap&       EngineCoordinator::get_options() { return engine.get_options(); }

int EngineCoordinator::get_hashfull(int maxAge) const {
    std::lock_guard<std::mutex> lock(mutex);
    return engine.get_hashfull(maxAge);
}

std::string EngineCoordinator::fen() const {
    std::lock_guard<std::mutex> lock(mutex);
    return engine.fen();
}

std::string EngineCoordinator::visualize() const {
    std::lock_guard<std::mutex> lock(mutex);
    return engine.visualize();
}

std::string EngineCoordinator::options_as_uci() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::stringstream           ss;
    ss << engine.get_options();
    return ss.str();
}

std::string EngineCoordinator::get_numa_config_as_string() const {
    std::lock_guard<std::mutex> lock(mutex);
    return engine.get_numa_config_as_string();
}

std::string EngineCoordinator::numa_config_information_as_string() const {
    std::lock_guard<std::mutex> lock(mutex);
    return engine.numa_config_information_as_string();
}

std::string EngineCoordinator::thread_allocation_information_as_string() const {
    std::lock_guard<std::mutex> lock(mutex);
    return engine.thread_allocation_information_as_string();
}

std::string EngineCoordinator::thread_binding_information_as_string() const {
    std::lock_guard<std::mutex> lock(mutex);
    return engine.thread_binding_information_as_string();
}

void EngineCoordinator::mark_position_changed(const std::string& source) {
    auto currentFen = fen();

    std::lock_guard<std::mutex> lock(stateMutex);
    activeController = source;
    analysis.active  = false;
    analysis.source  = source;
    analysis.fen     = currentFen;
    analysis.candidates.clear();
    add_event_locked("position_changed", source, currentFen);
}

void EngineCoordinator::mark_new_game(const std::string& source) {
    auto currentFen = fen();

    std::lock_guard<std::mutex> lock(stateMutex);
    activeController = source;
    analysis         = MCPAnalysisSnapshot{};
    analysis.source  = source;
    analysis.fen     = currentFen;
    add_event_locked("new_game", source, currentFen);
}

void EngineCoordinator::mark_search_started(const std::string& source) {
    auto currentFen = fen();

    std::lock_guard<std::mutex> lock(stateMutex);
    activeController = source;
    analysis.active  = true;
    analysis.source  = source;
    analysis.fen     = currentFen;
    analysis.bestmove.clear();
    analysis.ponder.clear();
    analysis.candidates.clear();
    add_event_locked("search_started", source, currentFen);
}

void EngineCoordinator::update_analysis(const Engine::InfoFull& info, const std::string& score) {
    MCPAnalysisLine line;
    line.depth    = info.depth;
    line.selDepth = info.selDepth;
    line.multiPV  = info.multiPV;
    line.score    = score;
    line.bound    = std::string(info.bound);
    line.wdl      = std::string(info.wdl);
    line.timeMs   = info.timeMs;
    line.nodes    = info.nodes;
    line.nps      = info.nps;
    line.tbHits   = info.tbHits;
    line.hashfull = info.hashfull;
    line.pv       = std::string(info.pv);

    std::lock_guard<std::mutex> lock(stateMutex);
    analysis.active = true;

    auto sameMultiPV = [&](const MCPAnalysisLine& candidate) {
        return candidate.multiPV == line.multiPV;
    };

    auto it = std::find_if(analysis.candidates.begin(), analysis.candidates.end(), sameMultiPV);
    if (it == analysis.candidates.end())
        analysis.candidates.push_back(std::move(line));
    else
        *it = std::move(line);
}

void EngineCoordinator::update_bestmove(std::string_view bestmove, std::string_view ponder) {
    auto currentFen = fen();

    std::lock_guard<std::mutex> lock(stateMutex);
    analysis.active   = false;
    analysis.bestmove = std::string(bestmove);
    analysis.ponder   = std::string(ponder);
    analysis.fen      = currentFen;
    add_event_locked("bestmove", analysis.source, currentFen, analysis.bestmove);
}

MCPAnalysisSnapshot EngineCoordinator::analysis_snapshot() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return analysis;
}

std::vector<MCPEvent> EngineCoordinator::recent_events() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return events;
}

bool EngineCoordinator::search_active() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return analysis.active;
}

std::string EngineCoordinator::controller() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return activeController;
}

void EngineCoordinator::add_event_locked(const std::string& type, const std::string& source,
                                         const std::string& fen, const std::string& detail) {
    events.push_back({type, source, fen, detail});

    if (events.size() > MaxEvents)
        events.erase(events.begin(), events.begin() + (events.size() - MaxEvents));
}

}  // namespace Stockfish
