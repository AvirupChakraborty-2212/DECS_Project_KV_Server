#!/bin/bash

# ---------------------------------------------------------
# CONFIGURATION
# ---------------------------------------------------------
# Load Generator (Client) Pinning
LOADGEN_CORES="2-7"

# Monitoring
SERVER_MONITOR_CORE="1" # Monitor this core for Server CPU
DISK_DEVICE="sdd"       # Disk to monitor

# Paths
EXE_PATH="./build/loadgen"
# ---------------------------------------------------------

if [ "$#" -lt 3 ]; then
    echo "Usage: sudo $0 <max_clients> <duration> <workload> [p1] [p2]"
    echo "Examples:"
    echo "  sudo $0 20 30 put_all"
    echo "  sudo $0 20 30 mix 80 20"
    exit 1
fi

# Ensure Root
if [ "$EUID" -ne 0 ]; then
  echo "Error: Run as root (sudo)."
  exit 1
fi

MAX_CLIENTS=$1
DURATION=$2
WORKLOAD=$3
shift 3
EXTRA_ARGS="$@"

OUTPUT_FILE="results_${WORKLOAD}.csv"
TEMP_LOG="temp_run.log"
MON_CPU_LOG="temp_cpu.log"
MON_DISK_LOG="temp_disk.log"

# Header
echo "Clients,Throughput,Latency,ServerCPU(%),DiskUtil(%),CacheHitRate(%)" > "$OUTPUT_FILE"

echo "=================================================================="
echo "PHASE 1: SYSTEM PRE-WARM"
echo "Running single-threaded warmup to populate DB and heat up Caches."
echo "Workload: $WORKLOAD $EXTRA_ARGS"
echo "=================================================================="

# 1. PRE-WARM
# Run WITHOUT --no-warmup. This fills the DB.
# We run for just 2 seconds after warmup finishes.
taskset -c $LOADGEN_CORES $EXE_PATH 1 2 $WORKLOAD $EXTRA_ARGS > /dev/null 2>&1

echo ">>> Pre-warm done. Sleeping 5s to clear ports..."
sleep 5

echo "=================================================================="
echo "PHASE 2: BENCHMARK EXECUTION"
echo "Running loop 1 to $MAX_CLIENTS with --no-warmup"
echo "=================================================================="

for (( c=1; c<=MAX_CLIENTS; c++ ))
do
    echo "Running test with $c client(s)..."
    
    # A. Start Monitors
    mpstat -P $SERVER_MONITOR_CORE 1 > "$MON_CPU_LOG" 2>&1 &
    PID_CPU=$!
    
    iostat -dx $DISK_DEVICE 1 > "$MON_DISK_LOG" 2>&1 &
    PID_DISK=$!
    
    # B. Run LoadGen (WITH --no-warmup)
    # DB is already populated. We just measure performance now.
    taskset -c $LOADGEN_CORES $EXE_PATH $c $DURATION $WORKLOAD $EXTRA_ARGS --no-warmup > "$TEMP_LOG" 2>&1
    
    # C. Stop Monitors
    kill $PID_CPU $PID_DISK 2>/dev/null
    
    # D. Parse Results
    TP=$(grep "Throughput:" "$TEMP_LOG" | awk '{print $2}')
    LAT=$(grep "Latency:" "$TEMP_LOG" | awk '{print $2}')
    HIT_RATE=$(grep "HitRate=" "$TEMP_LOG" | awk -F'HitRate=' '{print $2}' | tr -d '%')

    # Parse Hardware Stats (Skip boot-time average using tail -n +2 or similar logic)
    # Server CPU: Average the Idle time, then 100 - Idle
    AVG_IDLE=$(awk '/^[0-9]/ {sum+=$NF; count++} END {if(count>0) print sum/count; else print 100}' "$MON_CPU_LOG")
    SERVER_CPU=$(awk "BEGIN {printf \"%.2f\", 100 - $AVG_IDLE}")

    # Disk Util: Grep device, tail skip first line, average %util (last col)
    DISK_UTIL=$(grep "$DISK_DEVICE" "$MON_DISK_LOG" | tail -n +2 | awk '{sum+=$(NF); count++} END {if(count>0) printf "%.2f", sum/count; else print 0}')

    # Defaults
    [ -z "$TP" ] && TP="0"; [ -z "$LAT" ] && LAT="0"; [ -z "$HIT_RATE" ] && HIT_RATE="0"

    echo "$c,$TP,$LAT,$SERVER_CPU,$DISK_UTIL,$HIT_RATE" >> "$OUTPUT_FILE"
    echo "  -> TP: $TP | Lat: $LAT | CPU: $SERVER_CPU% | Disk: $DISK_UTIL% | Hits: $HIT_RATE%"

    # Cleanup
    rm -f "$TEMP_LOG" "$MON_CPU_LOG" "$MON_DISK_LOG"
    
    # Sleep to clean up TIME_WAIT sockets
    sleep 2
done

echo "=================================================================="
echo "Benchmark Complete. Results in $OUTPUT_FILE"