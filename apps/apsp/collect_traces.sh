timeout 180 /users/deepmish/instruction-fusion/src/build/opt/deps/dynamorio/bin64/drrun -c /users/deepmish/instruction-fusion/src/build/opt/deps/dynamorio/clients/lib64/release/libdrmemtrace.so -offline -trace_after_instrs 300M -trace_for_instrs 100M -- ./apsp 2 16384 16

bash /users/deepmish/instruction-fusion/utils/memtrace/run_portabilize_trace.sh

# Step 3: Run update trace script
bash /users/deepmish/instruction-fusion/utils/memtrace/run_update_trace.sh

./scarab --frontend memtrace --cbp_trace_r0=/users/deepmish/CRONO/apps/sssp/drmemtrace.apsp.1035208.3778.dir/trace/drmemtrace.apsp.1035208.0652.trace.zip --memtrace_modules_log=/users/deepmish/CRONO/apps/sssp/drmemtrace.apsp.1035208.3778.dir/bin/  --inst_limit 100000000  --uop_cache_enable 0
