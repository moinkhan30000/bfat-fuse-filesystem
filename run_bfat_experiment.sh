#!/bin/bash

set -e  # Exit on any error
echo "🚀 Starting BFAT experiment..."

# Step 1: Clean and Rebuild
echo "🔧 make clean && make"
make clean && make

# Step 2: Create fresh disk
echo "💾 Creating disk1 (16MB)..."
dd if=/dev/zero of=disk1 bs=4K count=4096 status=none

# Step 3: Format with make_bfat
echo "🛠 Formatting disk..."
./make_bfat disk1

# Step 4: Setup mount point
echo "📂 Preparing mount point..."
fusermount -u ~/bfat_mount 2>/dev/null || true
rm -rf ~/bfat_mount
mkdir -p ~/bfat_mount

# Step 5: Mount BFAT in background
echo "📦 Mounting BFAT filesystem..."
./bfat ~/bfat_mount disk1 &
BFAT_PID=$!
sleep 1  # Give FUSE a moment to initialize

# Step 6: Run timed experiments in a subshell
(
  cd ~/bfat_mount
  echo "⏱ Test 1: Creating 30 small files..."
  /usr/bin/time -v bash -c 'for i in {1..30}; do echo "File $i" > file_$i; done'

  echo "⏱ Test 2: Writing 5MB to bigfile..."
  /usr/bin/time -v dd if=/dev/zero of=bigfile bs=1M count=5 status=none

  echo "⏱ Test 3: Reading bigfile..."
  /usr/bin/time -v cat bigfile > /dev/null

  echo "⏱ Test 4: Deleting 15 files..."
  /usr/bin/time -v rm file_{1..15}
) | tee ~/bfat_test_results.txt

# Step 7: Clean unmount
echo "🧼 Unmounting filesystem..."
cd ~
fusermount -u ~/bfat_mount
kill $BFAT_PID 2>/dev/null || true

echo "✅ Experiment complete. Results saved to ~/bfat_test_results.txt"

