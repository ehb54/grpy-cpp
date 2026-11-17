# Prebuilt GRPY binaries

Built from the source in this repository, including the opt-in `GRPY_ETA_ZETA` report
line, and validated against the golden output of the original Fortran GRPY
(`tests/run.sh`): all five cases pass, worst relative error 0.00e+00, on Linux x86_64, on
macOS x86_64 and on Windows 10. The macOS arm64 slice is the one build that was not run
(below).

| file | platform | notes |
|---|---|---|
| `GRPY_osx10.11` | macOS universal (x86_64 + arm64) | ad-hoc signed; deployment target 10.13 (x86_64) / 11.0 (arm64) |
| `GRPY_linux64` | Linux x86_64 | **fully static** (`-static`) -- no runtime dependencies at all, like the Fortran binary it replaces |
| `GRPY_win64.exe` | Windows x86_64 | built on Windows 10 22H2 with MSYS2 mingw-w64, fully static |

The names are the ones SOMO looks for in the UltraScan `bin` directory, so these drop
straight into `us_somo/add_to_bin/`.

The Linux binary is fully static, matching the portability of the Fortran binary it
replaces: `ldd` reports "not a dynamic executable", so no glibc version floor applies. That
this still threads was checked rather than assumed -- `-static` can quietly cost you
pthreads -- on a 1001-bead model it runs 40.1 s on one thread and 1.85 s on 32.

## [η]ζ agrees across platforms

`GRPY_ETA_ZETA=1` has no Fortran golden, so the three binaries were compared with each
other instead, with `GRPY_HP=1`, on the IgE example, the `-u` 1znf example and a 1001-bead
protein model. [η]ζ agrees to within 3 in the 16th significant digit. On the protein model
the Linux and Windows values are identical, and they also match the CMake build that
`tests/test_eta_zeta.cpp` checks.

## The Windows binary

It was built natively on Windows 10 22H2 (build 19045) with the MSYS2 MINGW64 toolchain
(GCC 12.2, posix threads) and linked `-static`. Its import table lists only `KERNEL32.dll`
and `msvcrt.dll`, the C runtime that ships with every version of Windows. There is no
`libstdc++-6.dll`, `libwinpthread-1.dll` or `libgcc_s_seh-1.dll`, so it needs no runtime
DLLs.

All five golden cases pass on Windows 10 22H2 under MSYS2, worst relative error 0.00e+00:
native, `-e`, `-u`, the `-d` batch writing both `.dat` files, and the usage text on a bad
invocation. Threading was checked as on Linux: the 1001-bead model runs 126.2 s on one
thread and 22.6 s on 12, in a 12-core virtual machine.

## The macOS arm64 slice was cross-compiled and not run

Both slices were built on an Intel Mac with Apple clang 13.1 against the macOS 12.3 SDK, the
arm64 one with `-arch arm64`. The goldens pass on the x86_64 slice. The arm64 slice was not
executed, because no Apple Silicon machine was available. Run `tests/run.sh GRPY_osx10.11`
on one before relying on it there.

## Signing

The macOS binary is **ad-hoc signed** (`codesign -s -`), which is what an arm64 binary
needs in order to run at all. It is not signed with an Apple Developer ID and is not
notarized, so on first run macOS will still quarantine it if it arrives by download; inside
a signed UltraScan installer it is re-signed with the installer's identity.

Verify a slice at a time — `codesign -dv` reports only the native one on a universal binary:

```bash
codesign -dv --arch arm64  GRPY_osx10.11
codesign -dv --arch x86_64 GRPY_osx10.11
```

## Out-of-core on Windows

`GRPY_OOC` is unavailable in a Windows build: the out-of-core path uses `mmap`, and the
Win32 equivalent could not be exercised on any machine here. It refuses with a clear
message rather than silently doing something untested. The in-core path, which is the
default and what ordinary runs use, is unaffected.
