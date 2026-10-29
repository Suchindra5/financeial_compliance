#include <iostream>
#include <string>
#include "dfa_engine.hpp"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <schema.json> <txn_id> <event>\n";
        return 1;
    }

    std::string schema_path = argv[1];
    std::string txn_id = argv[2];
    std::string event_name = argv[3];

    TimedComplianceEngine engine;
    if (!engine.load_schema_from_json(schema_path)) {
        std::cerr << "Error: Failed to load schema from " << schema_path << "\n";
        return 1;
    }

    if (txn_id == "__METRICS__") {
        json metrics = engine.get_telemetry_metrics();
        std::cout << metrics.dump() << std::endl;
        return 0;
    }

    size_t next_state = engine.process_event(txn_id, event_name, true);
    std::cout << next_state << std::endl;

    return 0;
}