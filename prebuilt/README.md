# Prebuilt GRPY binaries

Built from the source in this repository and validated against the golden output of the
original Fortran GRPY (`tests/run.sh`) on the target platform itself: all five cases on
all three, worst relative error 0.00e+00.

| file | platform | notes |
|---|---|---|
| `GRPY_osx10.11` | macOS universal (x86_64 + arm64) | ad-hoc signed; deployment target 10.13 (x86_64) / 11.0 (arm64) |
| `GRPY_linux64` | Linux x86_64 | **fully static** (`-static`) -- no runtime dependencies at all, like the Fortran binary it replaces |
| `GRPY_win64.exe` | Windows x86_64 | cross-compiled on macOS, fully static; validated on Windows 10 22H2 |

The names are the ones SOMO looks for in the UltraScan `bin` directory, so these drop
straight into `us_somo/add_to_bin/`.

The Linux binary is fully static, matching the portability of the Fortran binary it
replaces: `ldd` reports "not a dynamic executable", so no glibc version floor applies. That
this still threads was checked rather than assumed -- `-static` can quietly cost you
pthreads -- at 1200 beads it runs 67.1 s on one thread and 2.81 s on 32.

## The Windows binary is cross-compiled

It was built with mingw-w64 (GCC 16.2) and linked `-static`; its import table lists only
`KERNEL32.dll` and the UCRT `api-ms-win-crt-*` stubs -- no `libstdc++-6.dll`,
`libwinpthread-1.dll` or `libgcc_s_seh-1.dll` -- so it needs no runtime DLLs on Windows 10
or later.

Because it is cross-compiled, it was run on Windows rather than trusted: all five golden
cases pass on Windows 10 22H2 (build 19045) under MSYS2, worst relative error 0.00e+00 --
native, `-e`, `-u`, the `-d` batch writing both `.dat` files, and the usage text on a bad
invocation.

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
