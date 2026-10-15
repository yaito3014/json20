#!/bin/bash
# Compile-time benchmark for json20.
# usage: bench/compile_time/measure.sh [include-dir] [label]
#   CXX selects the compiler (default g++), RUNS the sample count (default 5).
# Prints the median wall time of compiling each TU at -O0.
set -u
here=$(cd "$(dirname "$0")" && pwd)
inc=${1:-$here/../../include}
label=${2:-current}
cxx=${CXX:-g++}
runs=${RUNS:-5}
for tu in instantiate.cpp consteval_heavy.cpp; do
  ts=()
  for _ in $(seq "$runs"); do
    s=$(date +%s.%N)
    if ! "$cxx" -std=c++23 -O0 -I"$inc" -c "$here/$tu" -o /dev/null 2>/dev/null; then
      echo "$label $tu: compile failed"; continue 2
    fi
    e=$(date +%s.%N)
    ts+=("$(echo "$e - $s" | bc)")
  done
  med=$(printf '%s\n' "${ts[@]}" | sort -n | sed -n "$(( (runs + 1) / 2 ))p")
  echo "$label $tu median_of_$runs=${med}s ($cxx)"
done
