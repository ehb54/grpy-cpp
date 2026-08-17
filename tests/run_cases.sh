#!/usr/bin/env bash
# Run a candidate GRPY binary over all Phase-0 example cases, mirroring the
# golden directory layout so compare.py can diff the two trees.
#
# Usage: run_grpy.sh <path-to-grpy-binary> <output-dir>
set -u
BIN="${1:?usage: run_grpy.sh <binary> <outdir>}"
OUT="${2:?usage: run_grpy.sh <binary> <outdir>}"
HERE="$(cd "$(dirname "$0")" && pwd)"
EX="$HERE/examples"

run() { # name  flag(or empty)  inputfile  [aux coord files...]
  local name="$1" flag="$2" inp="$3"; shift 3
  local d="$OUT/$name"; rm -rf "$d"; mkdir -p "$d"
  cp "$EX/$inp" "$d/"
  for aux in "$@"; do cp "$EX/$aux" "$d/"; done
  ( cd "$d" && { [ -n "$flag" ] && "$BIN" "$flag" "$inp" || "$BIN" "$inp"; } \
      >stdout.txt 2>stderr.txt; echo $? >exitcode.txt )
  echo "  ran $name (exit $(cat "$d/exitcode.txt"))"
}

echo "Running candidate: $BIN"
run dumbbell_e      -e GRPYDumbbellExample
run ige_e          -e GRPYIgEExample
run 1znf_u         -u 1znf_1-A20R50hiOT-so.bead_model
run hydro_d        -d hydroControlFileExample.txt hydroDumbbell hydroIgE
run dumbbell_native "" GRPYDumbbellExample
echo "Done -> $OUT"
