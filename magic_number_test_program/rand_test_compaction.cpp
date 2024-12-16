#include <iostream>
#include <fstream>
#include <chrono>
#include <csignal>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/statistics.h>
#include <rocksdb/perf_context.h>
#include <random>
#include <unordered_map>
#include <thread>

// Signal handler to stop the program gracefully
volatile std::sig_atomic_t stop = 0;

void handle_signal(int signal) {
    stop = 1;
}

// Function to generate a random string of given length
std::string generate_random_string(size_t length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t max_index = sizeof(charset) - 1;
    std::string random_string;
    random_string.reserve(length);

    static std::random_device rd;
    static std::mt19937 generator(rd());
    static std::uniform_int_distribution<> distribution(0, max_index);

    for (size_t i = 0; i < length; ++i) {
        random_string += charset[distribution(generator)];
    }

    return random_string;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <rocksdb_directory>" << std::endl;
        return 1;
    }

    std::string db_path = argv[1];

    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    options.statistics = rocksdb::CreateDBStatistics();
    options.compaction_style = rocksdb::kCompactionStyleLevel;

    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db);
    if (!status.ok()) {
        std::cerr << "Failed to open RocksDB: " << status.ToString() << std::endl;
        return 1;
    }

    std::signal(SIGINT, handle_signal);

    std::ofstream log_file("rocksdb_perf.log", std::ios::out);
    log_file << "Timestamp,Iteration,InsertTime(ms),DeleteTime(ms),CompactionTime(ms),ReadTime(ms),Throughput(ops/s)\n";

    int iteration = 0;
    static constexpr size_t key_length = 16;    // Length of random keys
    static constexpr size_t value_length = 64; // Length of random values
    std::unordered_map<std::string, std::string> key_value_store; // In-memory map for validation

    while (!stop) {
        std::cout << "Iteration " << ++iteration << ": Starting load test..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();

        // Insert random key-value pairs
        auto insert_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100000; i++) {
            std::string key = generate_random_string(key_length);
            std::string value = generate_random_string(value_length);
            db->Put(rocksdb::WriteOptions(), key, value);
            key_value_store[key] = value; // Store key-value pair for verification
        }
        auto insert_end = std::chrono::high_resolution_clock::now();

        // Delete random keys
        auto delete_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 50000; i++) {
            std::string key = generate_random_string(key_length); // Random keys unlikely to match existing ones
            db->Delete(rocksdb::WriteOptions(), key);
            key_value_store.erase(key); // Remove key from in-memory map if it exists
        }
        auto delete_end = std::chrono::high_resolution_clock::now();

        // Trigger manual compaction
        auto compaction_start = std::chrono::high_resolution_clock::now();
        rocksdb::CompactRangeOptions compaction_options;
        status = db->CompactRange(compaction_options, nullptr, nullptr);
        auto compaction_end = std::chrono::high_resolution_clock::now();

        // Verify all reads
        auto read_start = std::chrono::high_resolution_clock::now();
        bool all_correct = true;
        for (const auto& kv : key_value_store) {
            std::string read_value;
            status = db->Get(rocksdb::ReadOptions(), kv.first, &read_value);
            if (!status.ok() || read_value != kv.second) {
                all_correct = false;
                std::cerr << "Error: Key=" << kv.first << ", Expected=" << kv.second << ", Got=" << read_value << std::endl;
            }
        }
        auto read_end = std::chrono::high_resolution_clock::now();

        auto insert_time = std::chrono::duration_cast<std::chrono::milliseconds>(insert_end - insert_start).count();
        auto delete_time = std::chrono::duration_cast<std::chrono::milliseconds>(delete_end - delete_start).count();
        auto compaction_time = std::chrono::duration_cast<std::chrono::milliseconds>(compaction_end - compaction_start).count();
        auto read_time = std::chrono::duration_cast<std::chrono::milliseconds>(read_end - read_start).count();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(compaction_end - start_time).count();

        double throughput = 150000.0 / (total_time / 1000.0);

        log_file << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << "," << iteration << "," << insert_time << "," << delete_time << "," << compaction_time << "," << read_time << "," << throughput << "\n";
        std::cout << "Iteration " << iteration << ": InsertTime=" << insert_time << "ms, DeleteTime=" << delete_time
                  << "ms, CompactionTime=" << compaction_time << "ms, ReadTime=" << read_time
                  << "ms, Throughput=" << throughput << " ops/s, ReadsCorrect=" << (all_correct ? "Yes" : "No") << "\n";

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    log_file.close();
    delete db;
    std::cout << "Database operations stopped. RocksDB directory: " << db_path << std::endl;

    return 0;
}
