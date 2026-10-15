#!/bin/bash
# Measures variant A (compressed table) against variant B (table generated in
# constant evaluation). usage: measure.sh <g++> <clang++> [clang -stdlib flag]
set -u
here=$(cd "$(dirname "$0")" && pwd)
out=${OUT:-$(mktemp -d)}
gxx=$1; cxx=$2; stdlib=${3:-}
runs=${RUNS:-5}

median() { sort -n | sed -n "$(( (runs + 1) / 2 ))p"; }
compile_time() {  # compiler flags tu
  local c=$1 flags=$2 tu=$3 ts=()
  for _ in $(seq "$runs"); do
    local s=$(date +%s.%N)
    $c -std=c++20 $flags -c "$here/$tu" -o "$out/tmp.o" || return 1
    local e=$(date +%s.%N)
    ts+=("$(echo "$e - $s" | bc)")
  done
  printf '%s\n' "${ts[@]}" | median
}

echo "== header size (lines / bytes) =="
for f in common.hpp variant_a.hpp variant_b.hpp; do printf '%-14s %4d lines %6d bytes\n' $f $(wc -l < "$here/$f") $(wc -c < "$here/$f"); done

echo "== compile time, median of $runs (s) =="
for c in "$gxx" "$cxx $stdlib"; do
  for opt in -O0 -O2; do
    printf '%-40s' "${c} ${opt}:"
    for tu in tu_baseline.cpp tu_variant_a.cpp tu_variant_b.cpp; do printf '  %s %s' "${tu#tu_}" "$(compile_time "$c" "$opt" $tu)"; done
    echo
  done
done

echo "== object size at -O2 (text / data incl. rodata), $gxx =="
for tu in tu_variant_a.cpp tu_variant_b.cpp; do
  $gxx -std=c++20 -O2 -c "$here/$tu" -o "$out/${tu%.cpp}.o"
  size -A "$out/${tu%.cpp}.o" | awk -v t="$tu" '/^\.text/ {tx+=$2} /^\.rodata|^\.data/ {d+=$2} END {printf "%-20s text %6d  data %6d\n", t, tx, d}'
done

echo "== runtime =="
for c in "$gxx" "$cxx $stdlib"; do
  $c -std=c++20 -O2 "$here/bench_runtime.cpp" -o "$out/bench" && printf '%-30s ' "$c:" && "$out/bench"
done

echo "== smallest constant-evaluation limit that builds variant B's table =="
search() {  # compiler flag-prefix upper-bound
  local lo=1 hi=$3
  $1 -std=c++20 ${2}${hi} -fsyntax-only "$here/tu_b_constant_eval.cpp" 2>/dev/null || { echo "fails even at $hi"; return; }
  while [ $((hi - lo)) -gt $((hi / 100 + 1)) ]; do
    local mid=$(( (lo + hi) / 2 ))
    if $1 -std=c++20 ${2}${mid} -fsyntax-only "$here/tu_b_constant_eval.cpp" 2>/dev/null; then hi=$mid; else lo=$mid; fi
  done
  echo "~$hi"
}
printf '%-40s %s\n' "$cxx -fconstexpr-steps (default 1048576):" "$(search "$cxx $stdlib" -fconstexpr-steps= $(( (1 << 31) - 1 )))"
printf '%-40s %s\n' "$gxx -fconstexpr-ops-limit (default 2^33):" "$(search "$gxx" -fconstexpr-ops-limit= $((1 << 34)))"
