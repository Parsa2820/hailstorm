# Issue Summary

While running Hailstorm with compaction offloading enabled, we encounter issues with erroneous table magic numbers in RocksDB files. This results in corruption errors and unexpected behavior during benchmarks.

---

## Steps to Reproduce

1. Enable compaction offloading in Hailstorm (with `-l` option).
2. Run database benchmarking tools with RocksDB data directory on Hailstorm-mounted directory, such as:
  - `db_bench` (RocksDB’s built-in benchmark tool)
  - `YCSB` (Yahoo! Cloud Serving Benchmark)
3. Monitor benchmark tools' error logs during the execution.

---

## Observed Behavior

- RocksDB reports corruption errors with mismatched magic numbers.
- Example error message:
  **“Corruption: Bad table magic number: expected 9868021990004412407, found 3658749561082095167”**.
- This issue occurs consistently across both `db_bench` and `YCSB` runs and will cause the benchmark tools to fail to produce meaningful performance statistics.

---

## Investigation of the Issues

### What is Magic Number in RocksDB

The magic number in RocksDB is a constant in the file header to identify the version or type of the SSTable file. The main purpose of the magic number is to verify the integrity and format of the file being processed. RocksDB uses a specific magic number in the beginning of WAL and SSTable files to make sure the validity of those files.
The RocksDB uses 64 bits of the SHA1 hash output of the block data as the magic number.

### Potential Causes

1. **File Corruption:** SSTable files were corrupted during storage, compaction, or offloading.
2. **Incompatible Table Format:** Generated files may belong to a different format or older RocksDB version.
3. **Misconfigured Compaction Offloading:** Hailstorm might have improperly offloaded data, resulting in invalid SSTable files.
4. **Incomplete Writes:** If the magic number wasn't correctly appended during write operations.

---

## Resolution Steps

### Ensure db_bench, YCSB, and RocksDB are using compatible versions.

- We compiled the `db_bench` tool in the same directory and ensured it is on the same branch and commit head as the RocksDB git repository. This was done to rule out any inconsistencies between the benchmark tool and the RocksDB codebase that could contribute to the observed magic number mismatches.
- We also tried different versions of YCSB to rule out any incompatibility or version-specific issues.
- Additionally, we compiled the RocksDB JNI Jar package using our local RocksDB repository to ensure version consistency between the YCSB benchmark and RocksDB library.

Despite these efforts, the issue persisted, indicating a deeper problem possibly related to compaction offloading or filesystem interactions.

---

### Verify that Hailstorm’s mounted directory is able to accept the RocksDB read/write and compaction.

To verify the compatibility of RocksDB with Hailstorm’s mounted directory, we designed and implemented a series of custom test programs. These tests were tailored to evaluate core functionalities, including read/write operations, data persistence, manual compaction, and overall data integrity under simulated workloads.

The tests involved continuous insertion, deletion, and compaction processes while monitoring RocksDB’s behavior. Additionally, we validated data consistency from MemTable to SSTable, inspected the structure of the LSM-tree, and ensured that RocksDB statistics reflected accurate operations. Throughout these tests, we paid close attention to any signs of corruption, such as mismatched magic numbers or failed compactions.

All tests were executed successfully, with no errors or inconsistencies observed. RocksDB demonstrated reliable performance, maintaining data integrity and functioning seamlessly on Hailstorm’s mounted directory. Importantly, no magic number corruption was encountered during any phase of the testing.

In summary, the tests we created confirm that RocksDB is fully compatible with Hailstorm’s mounted directory, performing as expected across all key operations.


---

## Summary

The tests we created confirm that RocksDB is fully compatible with Hailstorm’s mounted directory, performing as expected across all key operations.
