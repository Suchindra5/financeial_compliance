#ifndef DFA_ENGINE_HPP
#define DFA_ENGINE_HPP

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <fstream>
#include "json.hpp"

using json = nlohmann::json;

class TimedComplianceEngine {
public:
    struct StateNode {
        size_t id;
        std::string name;
        bool is_accepting;
        uint64_t ttl_seconds;
    };

    struct Metrics {
        std::atomic<uint64_t> total_evaluations{0};
        std::atomic<uint64_t> trap_interceptions{0};
        std::atomic<uint64_t> timeouts{0};
        std::atomic<uint64_t> accum_latency_ns{0};
    };

private:
    std::vector<StateNode> states_;
    std::unordered_map<std::string, size_t> state_index_map_;
    std::unordered_map<std::string, size_t> event_index_map_;
    std::vector<std::vector<size_t>> transition_matrix_;
    
    size_t trap_state_id_{0};
    size_t timeout_state_id_{0};
    Metrics metrics_;

    struct TransactionContext {
        size_t current_state_id;
        int64_t last_updated_epoch;
    };

    std::unordered_map<std::string, TransactionContext> active_transactions_;
    const std::string state_store_file_ = "config/state_store.json";

    void load_persistent_state() {
        std::ifstream file(state_store_file_);
        if (!file.is_open()) return;
        try {
            json j;
            file >> j;
            if (j.contains("transactions")) {
                for (auto& [txn_id, val] : j["transactions"].items()) {
                    active_transactions_[txn_id] = {
                        val["state_id"].get<size_t>(),
                        val["timestamp"].get<int64_t>()
                    };
                }
            }
            if (j.contains("metrics")) {
                metrics_.total_evaluations = j["metrics"].value("total_evaluations", 0);
                metrics_.trap_interceptions = j["metrics"].value("trap_interceptions", 0);
                metrics_.timeouts = j["metrics"].value("timeouts", 0);
                metrics_.accum_latency_ns = j["metrics"].value("accum_latency_ns", 0);
            }
        } catch (...) {}
    }

    void save_persistent_state() {
        json j;
        json txns_json;
        for (const auto& [txn_id, ctx] : active_transactions_) {
            txns_json[txn_id] = {
                {"state_id", ctx.current_state_id},
                {"timestamp", ctx.last_updated_epoch}
            };
        }
        j["transactions"] = txns_json;
        j["metrics"] = {
            {"total_evaluations", metrics_.total_evaluations.load()},
            {"trap_interceptions", metrics_.trap_interceptions.load()},
            {"timeouts", metrics_.timeouts.load()},
            {"accum_latency_ns", metrics_.accum_latency_ns.load()}
        };

        std::ofstream file(state_store_file_);
        if (file.is_open()) {
            file << j.dump(4);
        }
    }

public:
    TimedComplianceEngine() {
        load_persistent_state();
    }

    bool load_schema_from_json(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;

        json j;
        file >> j;

        states_.clear();
        state_index_map_.clear();
        event_index_map_.clear();

        size_t idx = 0;
        for (const auto& s : j["states"]) {
            std::string name = s["name"];
            bool accepting = s.value("accepting", false);
            uint64_t ttl = s.value("ttl_seconds", 0);

            states_.push_back({idx, name, accepting, ttl});
            state_index_map_[name] = idx;

            if (name == "TRAP") trap_state_id_ = idx;
            if (name == "EXPIRED_TRAP") timeout_state_id_ = idx;
            idx++;
        }

        idx = 0;
        for (const auto& e : j["events"]) {
            event_index_map_[e.get<std::string>()] = idx++;
        }

        size_t num_states = states_.size();
        size_t num_events = event_index_map_.size();
        transition_matrix_ = std::vector<std::vector<size_t>>(num_states, std::vector<size_t>(num_events, trap_state_id_));

        for (const auto& t : j["transitions"]) {
            size_t from = state_index_map_[t["from"]];
            size_t event = event_index_map_[t["event"]];
            size_t to = state_index_map_[t["to"]];
            transition_matrix_[from][event] = to;
        }

        return true;
    }

    size_t process_event(const std::string& txn_id, const std::string& event_name, bool persist = true) {
        auto t_start = std::chrono::high_resolution_clock::now();
        int64_t now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        if (active_transactions_.find(txn_id) == active_transactions_.end()) {
            active_transactions_[txn_id] = {0, now_epoch};
        }

        auto& ctx = active_transactions_[txn_id];

        // TTL Guard Check
        uint64_t allowed_ttl = states_[ctx.current_state_id].ttl_seconds;
        if (allowed_ttl > 0 && (now_epoch - ctx.last_updated_epoch) > (int64_t)allowed_ttl) {
            ctx.current_state_id = timeout_state_id_;
            metrics_.timeouts++;
            metrics_.trap_interceptions++;
        } else {
            if (event_index_map_.find(event_name) == event_index_map_.end()) {
                ctx.current_state_id = trap_state_id_;
            } else {
                size_t event_id = event_index_map_[event_name];
                ctx.current_state_id = transition_matrix_[ctx.current_state_id][event_id];
            }
        }

        ctx.last_updated_epoch = now_epoch;

        auto t_end = std::chrono::high_resolution_clock::now();
        uint64_t latency = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
        
        metrics_.total_evaluations++;
        metrics_.accum_latency_ns += latency;
        if (ctx.current_state_id == trap_state_id_) {
            metrics_.trap_interceptions++;
        }

        if (persist) {
            save_persistent_state();
        }

        return ctx.current_state_id;
    }

    json get_telemetry_metrics() const {
        uint64_t total = metrics_.total_evaluations.load();
        uint64_t avg_latency = total > 0 ? (metrics_.accum_latency_ns.load() / total) : 0;

        return {
            {"total_evaluations", total},
            {"trap_interceptions", metrics_.trap_interceptions.load()},
            {"timeouts", metrics_.timeouts.load()},
            {"average_latency_nanoseconds", avg_latency}
        };
    }
};

#endif // DFA_ENGINE_HPP