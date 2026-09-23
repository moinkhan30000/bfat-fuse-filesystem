# BFAT: FAT-Based FUSE File System

## Overview
A persistent, user-space file system implemented using the FUSE (Filesystem in Userspace) framework. BFAT utilizes a File Allocation Table (FAT) architecture to manage disk blocks, directory entries, and file metadata within a virtualized Linux file acting as the storage disk.

## Technical Details
* **Environment:** C, FUSE Framework (v2), Linux
* **Architecture:** Manages a custom 4KB block-level layout featuring a Volume Superblock, dynamically updated FAT chain entries, and a root directory structure.
* **Storage Logic:** Implements raw disk I/O with manual block allocation/deallocation strategies (`read_block`/`write_block`), bypassing standard `malloc` for disk storage.
* **Supported Operations:** Full POSIX-compliant file operations routed via FUSE callbacks (`getattr`, `readdir`, `open`, `read`, `write`, `release`).

## Build & Execution
Compiled and tested on Ubuntu 22.04 (x86-64).

```bash
# Build the file system and formatting tool
make

# Create a 16MB virtual disk
dd bs=4K count=4K if=/dev/zero of=disk1

# Format the disk with BFAT
./make_bfat disk1

# Mount the file system
mkdir -p /tmp/fusemountpoint
./bfat /tmp/fusemountpoint disk1
```

## Benchmarking & Performance
Performance was evaluated on a 16MB virtual disk (4096 blocks × 4KB) within a VirtualBox Ubuntu environment. Operations were executed using a custom bash script (`run_bfat_experiment.sh`) and measured via `/usr/bin/time -v` for system-level metrics. A full breakdown is available in `Performance_Analysis.pdf`.

**Operation Metrics**
| Operation | Target | Elapsed Time | System Time | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Creation** | 30 single-block files | 0.10s | 0.04s | Low overhead for root directory and initial FAT updates. |
| **Write** | 5MB sequential file | 0.59s | 0.16s | Verifies successful multi-block FAT chaining across the disk. |
| **Read** | 5MB sequential file | < 0.01s | < 0.01s | Minimal read overhead; demonstrates highly efficient FAT traversal. |
| **Deletion** | 15 files | 0.04s | 0.01s | Confirms rapid FAT chain cleanup and directory slot reclamation. |
