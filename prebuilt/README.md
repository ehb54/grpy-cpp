# Prebuilt GRPY binaries

Built from the source in this repository and validated against the golden output of the
original Fortran GRPY (`tests/run.sh`) on the machine that built them: all five cases,
worst relative error 0.00e+00.

| file | platform | notes |
|---|---|---|
| `GRPY_osx10.11` | macOS universal (x86_64 + arm64) | ad-hoc signed; deployment target 10.13 (x86_64) / 11.0 (arm64) |
| `GRPY_linux64` | Linux x86_64 | `-static-libstdc++ -static-libgcc`, so it needs only glibc |

The names are the ones SOMO looks for in the UltraScan `bin` directory, so these drop
straight into `us_somo/add_to_bin/`.

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
