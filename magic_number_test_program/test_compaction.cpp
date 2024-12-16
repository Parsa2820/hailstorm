#include <iostream>
#include <fstream>
#include <chrono>
#include <csignal>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/statistics.h>
#include <rocksdb/perf_context.h>
#include <thread>

// Signal handler to stop the program gracefully
volatile std::sig_atomic_t stop = 0;

void handle_signal(int signal) {
    stop = 1;
}

// Function to get current timestamp
std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now_time_t));
    return std::string(buffer);
}

int main(int argc, char* argv[]) {
    // Check if the RocksDB directory is passed as an argument
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <rocksdb_directory>" << std::endl;
        return 1;
    }

    // Get the RocksDB directory from CLI
    std::string db_path = argv[1];

    // Create and configure the database
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    options.statistics = rocksdb::CreateDBStatistics(); // Enable RocksDB statistics
    options.compaction_style = rocksdb::kCompactionStyleLevel; // Level compaction for load testing

    // Open the database
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db);
    if (!status.ok()) {
        std::cerr << "Failed to open RocksDB: " << status.ToString() << std::endl;
        return 1;
    }

    // Set up signal handling for graceful exit
    std::signal(SIGINT, handle_signal);

    // Log file for performance metrics
    std::ofstream log_file("rocksdb_perf.log", std::ios::out);
    log_file << "Timestamp,Iteration,InsertTime(ms),DeleteTime(ms),CompactionTime(ms),Throughput(ops/s)\n";

    // Infinite loop for load testing
    int iteration = 0;
    while (!stop) {
        std::cout << "Iteration " << ++iteration << ": Starting load test..." << std::endl;

        // Start measuring performance
        auto start_time = std::chrono::high_resolution_clock::now();

        // Insert key-value pairs
        auto insert_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100000; i++) {
            db->Put(rocksdb::WriteOptions(), "key" + std::to_string(i), "value" + std::to_string(iteration));
        }
        auto insert_end = std::chrono::high_resolution_clock::now();

        // Delete some keys to create space for compaction
        auto delete_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 50000; i++) {
            db->Delete(rocksdb::WriteOptions(), "key" + std::to_string(i));
        }
        auto delete_end = std::chrono::high_resolution_clock::now();

        // Trigger manual compaction
        auto compaction_start = std::chrono::high_resolution_clock::now();
        rocksdb::CompactRangeOptions compaction_options;
        status = db->CompactRange(compaction_options, nullptr, nullptr);
        auto compaction_end = std::chrono::high_resolution_clock::now();

        // Calculate elapsed times
        auto insert_time = std::chrono::duration_cast<std::chrono::milliseconds>(insert_end - insert_start).count();
        auto delete_time = std::chrono::duration_cast<std::chrono::milliseconds>(delete_end - delete_start).count();
        auto compaction_time = std::chrono::duration_cast<std::chrono::milliseconds>(compaction_end - compaction_start).count();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(compaction_end - start_time).count();

        // Calculate throughput
        double throughput = 150000.0 / (total_time / 1000.0); // Operations per second

        // Log performance metrics
        log_file << get_timestamp() << "," << iteration << "," << insert_time << "," << delete_time << ","
                 << compaction_time << "," << throughput << "\n";
        std::cout << "Iteration " << iteration << ": InsertTime=" << insert_time << "ms, DeleteTime=" << delete_time
                  << "ms, CompactionTime=" << compaction_time << "ms, Throughput=" << throughput << " ops/s\n";

        // Wait for a short time before the next iteration
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // Print RocksDB statistics
    std::cout << "RocksDB Statistics:\n" << options.statistics->ToString() << std::endl;

    // Clean up
    log_file.close();
    delete db;
    std::cout << "Database operations stopped. RocksDB directory: " << db_path << std::endl;

    return 0;
}
