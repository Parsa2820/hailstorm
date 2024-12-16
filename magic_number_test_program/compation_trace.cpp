#include <iostream>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/statistics.h>
#include <string>

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
    options.compaction_style = rocksdb::kCompactionStyleLevel; // Enable level compaction
    options.write_buffer_size = 64 * 1024; // Small buffer to trigger flush quickly
    options.max_write_buffer_number = 3;
    options.level0_file_num_compaction_trigger = 2;

    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db);
    if (!status.ok()) {
        std::cerr << "Error opening RocksDB: " << status.ToString() << std::endl;
        return 1;
    }

    std::string key_prefix = "test_key_";
    std::string value_prefix = "test_value_";
    std::string read_value;

    // Step 1: Write multiple key-value pairs
    std::cout << "Writing data..." << std::endl;
    for (int i = 0; i < 1000; i++) { // Writing 1000 key-value pairs
        std::string key = key_prefix + std::to_string(i);
        std::string value = value_prefix + std::to_string(i);
        status = db->Put(rocksdb::WriteOptions(), key, value);
        if (!status.ok()) {
            std::cerr << "Error writing key: " << key << " - " << status.ToString() << std::endl;
            delete db;
            return 1;
        }
    }
    std::cout << "Inserted 1000 key-value pairs into RocksDB." << std::endl;

    // Step 2: Force manual compaction
    rocksdb::CompactRangeOptions compaction_options;
    compaction_options.exclusive_manual_compaction = false;
    status = db->CompactRange(compaction_options, nullptr, nullptr);
    if (status.ok()) {
        std::cout << "Manual compaction completed successfully." << std::endl;
    } else {
        std::cerr << "Error during manual compaction: " << status.ToString() << std::endl;
        delete db;
        return 1;
    }

    // Step 3: Verify all data
    std::cout << "Verifying data..." << std::endl;
    bool all_data_correct = true;
    for (int i = 0; i < 1000; i++) {
        std::string key = key_prefix + std::to_string(i);
        std::string expected_value = value_prefix + std::to_string(i);

        status = db->Get(rocksdb::ReadOptions(), key, &read_value);
        if (status.ok()) {
            if (read_value != expected_value) {
                std::cerr << "Mismatch: Key = " << key << ", Expected Value = " << expected_value
                          << ", Actual Value = " << read_value << std::endl;
                all_data_correct = false;
            }
        } else {
            std::cerr << "Error reading key: " << key << " - " << status.ToString() << std::endl;
            all_data_correct = false;
        }
    }

    if (all_data_correct) {
        std::cout << "All data verified successfully." << std::endl;
    } else {
        std::cerr << "Data verification failed. Check the logs above for details." << std::endl;
    }

    // Step 4: Check LSM tree levels
    for (int level = 0; level < 7; level++) { // Check levels 0 to 6
        std::string property;
        if (db->GetProperty(rocksdb::DB::Properties::kNumFilesAtLevelPrefix + std::to_string(level), &property)) {
            std::cout << "Files in Level " << level << ": " << property << std::endl;
        }
    }

    // Log RocksDB statistics
    std::cout << "RocksDB Statistics:\n" << options.statistics->ToString() << std::endl;

    // Clean up
    delete db;
    return 0;
}
