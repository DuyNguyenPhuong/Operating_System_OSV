# TLB and Cache Measurement

# Authur: Dave Nguyen
# With the help of Professor Tanya Amert

## Overview

These set of programs mimics the process of accessing elements, we will investigate the speed (performance) and size of both TLB and cache hierarchies (layers). Our methodology involves implementing functions designed to access varying numbers of elements and utilize different data access patterns. This approach allows us to infer characteristics such as cache size, the cost of cache misses, and the strategies employed by the CPU for data caching.

## Getting Started

### Prerequisites

- A Unix-like operating system (Linux/MacOS)
- GCC compiler
- Run on Mantis

### How to run Cache Test

After ssh to mantis

#### Go to CacheSizeTest folder

```bash
cd CacheSizeTest
```

#### To generate data of speed vs array size

```bash
chmod +x run_cache.sh
./run_cache.sh
```

Then check the result in `cache_results.csv`

### To run the tests cases to check for correctness

```bash
chmod +x run_all_cache_tests.sh
./run_all_cache_tests.sh
```

### How to run TLB Test

After ssh to mantis

#### Go to TLBSizeTest folder

```bash
cd TLBSizeTest
```

#### To generate data of speed vs number of page

```bash
chmod +x run_tlb.sh
./run_tlb.sh
```

Then check the result in `tlb_results.csv`

### To run the tests cases to check for correctness

```bash
chmod +x run_all_tlb_tests.sh
./run_all_tlb_tests.sh
```

### Contact

If you come across any program, contact me through email: `nguyend2@carleton.edu` or through Slack