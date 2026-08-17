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

## Provenance and licence

GRPY — Copyright © 2017 Paweł Jan Żuk — GPLv3. Cite: Żuk, P. J., Cichocki, B. and
Szymczak, P., *GRPY: an accurate bead method for calculation of hydrodynamic properties of
rigid biomacromolecules*, Biophys. J. **115**:782–800 (2018).

This program is distributed under the GPLv3; see `LICENSE`. Each header records what it
was translated from.

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
