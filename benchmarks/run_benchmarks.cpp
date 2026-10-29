#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include "dfa_engine.hpp"

int main() {
    std::cout << "==================================================\n";
    std::cout << " Financial Workflow DFA: O(1) Performance Benchmark\n";
    std::cout << "==================================================\n";

    TimedComplianceEngine engine;
    std::string schema_path = "config/schema.json";
    
    if (!engine.load_schema_from_json(schema_path)) {
        std::cerr << "Error: Failed to load schema from " << schema_path << "\n";
        return 1;
    }

    const int num_transactions = 25000;
    const std::vector<std::string> sequence = {"VerifyKYC", "ClearAML", "ApproveRisk", "SettlePayment"};
    uint64_t total_evals = num_transactions * sequence.size();

    std::cout << "Initiating stress test with " << num_transactions << " unique transactions...\n";
    std::cout << "Total transition evaluations to process: " << total_evals << "\n\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_transactions; ++i) {
        std::string txn_id = "BENCH_TXN_" + std::to_string(i);
        for (const auto& event : sequence) {
            engine.process_event(txn_id, event, false); // Skip disk writing for pure speed benchmark
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto total_duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    auto total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    double avg_latency_ns = (double)total_duration_ns / total_evals;
    double throughput_eps = ((double)total_evals / total_duration_ms) * 1000.0;

    std::cout << "--------------------------------------------------\n";
    std::cout << " BENCHMARK RESULTS:\n";
    std::cout << "--------------------------------------------------\n";
    std::cout << " Total Evaluations Processed : " << total_evals << "\n";
    std::cout << " Total Execution Time        : " << total_duration_ms << " ms\n";
    std::cout << " Average Constant-Time (O(1)): " << avg_latency_ns << " ns per lookup\n";
    std::cout << " Engine Throughput           : " << (int)throughput_eps << " transactions/sec\n";
    std::cout << "==================================================\n";

    return 0;
}