# GRPY — C++

Hydrodynamic properties of rigid bead models by the generalized Rotne–Prager–Yamakawa
method: a C++ reimplementation of [GRPY](https://github.com/pjzuk/GRPY), thread-parallel
and memory-lean, run as a standalone program.

It is a drop-in replacement for the original `GRPY.exe`: same command line, same progress
banner, same report. `tests/run.sh` compares its output against the golden output of the
Fortran program on every example, and the physical values agree exactly at the reported
precision.

## Building

Eigen (header-only) is the only dependency.

```bash
cmake -B build -DEIGEN_INCLUDE_DIR=/path/to/eigen
cmake --build build -j
./tests/run.sh build/grpy
```

`EIGEN_INCLUDE_DIR` is the directory holding `Eigen/Dense`; UltraScan vendors a copy at
`us_somo/develop/include`. Without it, CMake looks for a system Eigen3.

## Usage

```
grpy <file>        GRPY-native input
grpy -e <file>     GRPY-native input, plus the progress banner on stdout
grpy -u <file>     us-somo .bead_model input
grpy -d <file>     hydro++ control file; each model reports to <output>-GRPY.dat
```

The command line is fixed by compatibility, so everything added since the Fortran is an
environment variable:

| variable | effect |
|---|---|
| `GRPY_THREADS=<n>` | worker threads (default: all cores) |
| `GRPY_SINGLE=1` | single-precision storage and factorization — half the memory, ~2× faster, same result to the reported precision |
| `GRPY_OOC=<dir>` | spill the tiled matrix to disk, so RAM stays bounded |
| `GRPY_HP=1` | report in extended precision (ES24.15E3), for validation |

Memory is the binding constraint: peak RSS ≈ (11N)²/2 × scalar size, so 32 GB reaches
about 6800 beads in double precision and 9600 in single.

## Two documented differences from the Fortran

Both are gauge or noise, never a physical quantity, and `tests/run.sh` tolerates exactly
these two and nothing else:

- **Eigenvector sign.** LAPACK's `DSYEV` and Eigen's solver return `e1/e2/e3` with no
  consistent handedness rule; the displayed vectors can differ in sign. Every scalar the
  report derives from them is gauge-invariant and matches exactly.
- **Structurally-zero matrix entries** differ at the 1e-16 level — rounding noise in
  quantities that are zero by symmetry.

The `30–90% INVERTING MATRICES` progress is emitted by the blocked factorization here
rather than from inside a bundled LAPACK, so the percentages differ from the original's.
A caller scraping `^\s*(\d+)%\s*TASK:` for a progress bar sees the same shape.

## Provenance, authorship and licence

This is a derivative work with two layers of authorship.

- **GRPY** — Copyright © 2017 Paweł Jan Żuk — GPLv3. The Rotne–Prager–Yamakawa method, the
  compute path, the report and the input formats originate in `GRPY.f`.
- **The C++ port and everything built on it** — Copyright © 2026 the UltraScan project.

Copyright in the original work remains with its author; copyright in the new material is
the UltraScan project's. The combined work is GPLv3, as a derivative of GPLv3 code must
be — see `LICENSE`. Each file records which layer it belongs to and, where it derives from
`GRPY.f`, what was changed and when, as GPLv3 §5(a) requires.

### What is new here

Substantially more than a transcription:

- the dense inverse replaced by a **tiled, in-place Cholesky** solved against only the 11
  right-hand sides the rigid-body reduction needs, so the full inverse is never formed and
  only the upper triangle is stored — the memory wall this port exists to move;
- **thread-parallel** assembly and factorization over an injected pool;
- **single precision** storage and factorization, halving memory again at no cost to the
  reported figures;
- **out-of-core** storage, bounding resident memory independently of model size;
- fine-grained **progress** from inside the factorization;
- errors **reported** — by field and line for input, with a non-zero exit for failures —
  where the original aborted or computed on zeros;
- `PI`, `KB` and `NA` corrected to double precision; they were single, which capped the
  whole calculation at about seven digits.

Measured, on the same inputs and the same machine:

| | Fortran GRPY | this program |
|---|---|---|
| 5169 beads | ~30 min (single-threaded) | 53 s (64 threads) |
| 14142 beads (11N = 155562) | not attempted | 16 min (64 threads), 90 GB double / 45 GB single |

Threading accounts for much of the first row — this program at 1200 beads runs 67.1 s on
one thread against 2.81 s on 32 — and the memory model is what puts the second row within
reach at all.

### Citing

Two things are worth citing separately.

**The method** is Żuk's, and any work using this program rests on it: Żuk, P. J., Cichocki,
B. and Szymczak, P., *GRPY: an accurate bead method for calculation of hydrodynamic
properties of rigid biomacromolecules*, Biophys. J. **115**:782–800 (2018).

**This implementation** — the memory-lean tiled solver and its parallelization — is the
subject of a paper in preparation by Brookes, Żuk and Rocco. This note will be replaced by
the reference when it appears.

## Coding standards

The sources follow the
[UltraScan III coding standards](https://github.com/ehb54/ultrascan3/wiki/UltraScan-III-Coding-Standards)
(v2.0). `tools/us3_style.pl` applies them and `--check` reports violations; the tree is
clean.

One deliberate deviation, under the standards' own readability clause: inside the
translated tensor expressions in `grpy_core.hpp`, operators stay tight (`aI*aI+aJ*aJ`).
Those lines correspond one-to-one with lines of `GRPY.f`, which is what makes the
translation auditable against the original, and spacing them out breaks the correspondence
without making the algebra clearer.
