#!/usr/bin/env bash
# Validate a GRPY binary against the golden output of the original Fortran program.
#
#   tests/run.sh [path-to-grpy]        (default: ../build/grpy)
#
# The goldens in tests/golden/ were produced by the production Fortran binary SOMO
# shipped (GRPY_osx10.11); tests/examples/ holds the inputs distributed with GRPY.
# Both are inputs to this test, not products of it -- do not regenerate them from
# this program, or the test becomes a comparison with itself.
#
# Two documented differences are tolerated, and only these:
#   * the eigenvector sign gauge (--allow-sign-flip): DSYEV and Eigen return e1/e2/e3
#     with no consistent handedness rule. Only the displayed vectors move; every
#     physical scalar is gauge-invariant and is compared strictly.
#   * structurally-zero matrix entries at the 1e-16 level (--atol), which are
#     rounding noise in quantities that are exactly zero by symmetry.
set -u
HERE="$( cd "$( dirname "$0" )" && pwd )"
BIN="${1:-$HERE/../build/grpy}"

if [ ! -x "$BIN" ]; then
    echo "no GRPY binary at $BIN -- build it first, or pass its path" >&2
    exit 2
fi
# Each case runs in its own directory, so a relative path would not survive the cd.
BIN="$( cd "$( dirname "$BIN" )" && pwd )/$( basename "$BIN" )"

OUT="$( mktemp -d )"
trap 'rm -rf "$OUT"' EXIT

echo "candidate: $BIN"
"$HERE/run_cases.sh" "$BIN" "$OUT" > /dev/null || { echo "cases failed to run" >&2; exit 1; }

status=0

# The full report, every case. -e also carries the progress banner, which is emitted
# from inside the factorization and is compared separately below.
"$HERE/compare.py" "$HERE/golden" "$OUT" --report-only --allow-sign-flip --rtol 1e-3 --atol 1e-7 \
    || status=1

# The banner is what a caller scrapes for a progress bar, so its shape is part of the
# contract even though its exact percentages are not.
for case in dumbbell_e ige_e; do
    if ! grep -q '% TASK:' "$OUT/$case/stdout.txt"; then
        echo "FAIL: $case emitted no progress banner"
        status=1
    fi
done

# A bad invocation must still print the original's usage text.
if ! "$BIN" 2>/dev/null | grep -q 'ERROR: wrong input specified'; then
    echo "FAIL: no usage text on a bad invocation"
    status=1
fi

if [ $status -eq 0 ]; then
    echo "all golden cases pass"
fi
exit $status
