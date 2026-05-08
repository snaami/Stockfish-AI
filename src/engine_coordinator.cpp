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

}  // namespace Stockfish
