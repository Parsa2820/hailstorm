#include <iostream>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/statistics.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <rocksdb_directory>" << std::endl;
        return 1;
    }

    std::string db_path = argv[1];
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    options.statistics = rocksdb::CreateDBStatistics(); // Enable statistics for tracing

    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db);
    if (!status.ok()) {
        std::cerr << "Error opening RocksDB: " << status.ToString() << std::endl;
        return 1;
    }

    std::string key = "test_key";
    std::string value = "test_value";
    std::string read_value;

    // Step 1: Write key-value pair
    status = db->Put(rocksdb::WriteOptions(), key, value);
    if (!status.ok()) {
        std::cerr << "Error writing key-value: " << status.ToString() << std::endl;
        delete db;
        return 1;
    }
    std::cout << "Wrote key-value: (" << key << ", " << value << ") to MemTable." << std::endl;

    // Step 2: Read key-value pair from MemTable
    status = db->Get(rocksdb::ReadOptions(), key, &read_value);
    if (status.ok()) {
        std::cout << "Read from MemTable: Key = " << key << ", Value = " << read_value << std::endl;
    } else {
        std::cerr << "Error reading from MemTable: " << status.ToString() << std::endl;
    }

    // Step 3: Flush MemTable to SSTable
    status = db->Flush(rocksdb::FlushOptions());
    if (!status.ok()) {
        std::cerr << "Error flushing MemTable: " << status.ToString() << std::endl;
        delete db;
        return 1;
    }
    std::cout << "Flushed MemTable to SSTable." << std::endl;

    // Step 4: Read key-value pair from SSTable
    status = db->Get(rocksdb::ReadOptions(), key, &read_value);
    if (status.ok()) {
        std::cout << "Read from SSTable: Key = " << key << ", Value = " << read_value << std::endl;
    } else {
        std::cerr << "Error reading from SSTable: " << status.ToString() << std::endl;
    }

    // Log RocksDB statistics
    std::cout << "RocksDB Statistics:\n" << options.statistics->ToString() << std::endl;

    // Clean up
    delete db;
    return 0;
}
