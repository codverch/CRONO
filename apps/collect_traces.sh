#!/bin/bash

# Step 1: Run drrun with memory tracing (3-minute timeout)
timeout 180 /users/deepmish/instruction-fusion/src/build/opt/deps/dynamorio/bin64/drrun -c /users/deepmish/instruction-fusion/src/build/opt/deps/dynamorio/clients/lib64/release/libdrmemtrace.so -offline -trace_after_instrs 300M -trace_for_instrs 100M -- sssp/sssp 0 1 100000 5

# Step 2: Run portabilize trace script
bash /users/deepmish/instruction-fusion/utils/memtrace/run_portabilize_trace.sh

# Step 3: Run update trace script
bash /users/deepmish/instruction-fusion/utils/memtrace/run_update_trace.sh

# Step 4: Find and process window directories, rename files and directory
cd sssp/drmemtrace.sssp.*/trace
if [ -d "window.0000" ]; then
    mv window.0000/* .
    rmdir window.0000
fi

# Rename the trace zip file to trace.zip and set permissions
mv drmemtrace.sssp.*.trace.zip trace.zip 2>/dev/null || true
chmod 777 trace.zip

# Step 5: Create main traces directory structure and move trace
cd ../../..
mkdir -p traces/sssp
DRMEM_DIR=$(ls sssp/drmemtrace.sssp.* 2>/dev/null | head -1)
if [ -n "$DRMEM_DIR" ]; then
    mv "$DRMEM_DIR" traces/sssp/
fi

# SSSP p2p-Gnutella30.txt.gz:
# wget https://snap.stanford.edu/data/p2p-Gnutella30.txt.gz
# gunzip p2p-Gnutella30.txt.gz
# Change the value of DEG in sssp.cc from 16 to 160 
# Compile the code (make) 
# ./sssp 1 1 p2p-Gnutella30.txt  
# /users/deepmish/scarab/src/build/opt/deps/dynamorio/bin64/drrun   -c /users/deepmish/scarab/src/build/opt/deps/dynamorio/clients/lib64/release/libdrmemtrace.so -offline -trace_after_instrs 5G -trace_for_instrs 100M   -- ./sssp 1 1 p2p-Gnutella30.txt
