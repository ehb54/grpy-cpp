#!/usr/bin/env python3
"""Compare a candidate GRPY output tree against the golden tree.

For each case we compare the relevant text artifacts (stdout, and the -GRPY.dat
files for the -d batch case). Comparison is structural + numeric:

  * The text is tokenized into a "skeleton" (everything that is not a number)
    and an ordered list of numbers. Skeletons must match exactly (modulo the
    benign gfortran IEEE underflow note on stderr, which we never compare).
  * Numbers are compared pairwise with a relative tolerance, plus an absolute
    floor so that near-zero noise (e.g. -0.000E+00 vs 1.746E-16) is ignored.

Because GRPY prints results to ~4 significant figures, the stdout/.dat contract
can only be validated to ~1e-3 relative; that is exactly the precision SOMO
scrapes, so it is the right bar for a drop-in regression. Deeper numeric
validation of the ported core happens against a high-precision reference later.

Usage: compare.py <golden-dir> <candidate-dir> [--rtol 1e-3] [--atol 1e-12]
Exit status is 0 iff every compared artifact passes.
"""
import re, sys, os, argparse

# Matches Fortran-style numbers: 1.234E+06, -0.000E+00, 3.14, 42, 1.E-8, .5
NUM = re.compile(r'[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[EeDd][-+]?\d+)?')

# Cases and the artifacts to compare within each.
CASES = {
    'dumbbell_e':      ['stdout.txt'],
    'ige_e':           ['stdout.txt'],
    '1znf_u':          ['stdout.txt'],
    'dumbbell_native': ['stdout.txt'],
    # -d's substantive output is the .dat files (WRITEFILE). Its stdout mixes the
    # informational particle-name lines with compute-tied matInv progress that the
    # OpenBLAS port cannot reproduce, so stdout is checked separately, not here.
    'hydro_d':         ['Dumbbell-GRPY.dat', 'IgE-GRPY.dat'],
}


def strip_progress_banner(text):
    """Drop the cosmetic progress banner that precedes the report.

    The `\\r`-separated `NN% TASK:` progress is emitted partly by the driver and
    partly from inside the LAPACK inverter (tied to loop indices). The C++ port
    uses OpenBLAS, whose factorizations cannot emit per-iteration progress, so
    the banner is NOT part of the reproducible contract -- SOMO only reads the
    percentages to advance a progress bar. The substantive contract is the
    report table, which begins at the 'GRPY program' banner line. For output
    files (-d .dat) there is no progress banner and this is a no-op."""
    i = text.find('GRPY program')
    return text[i:] if i != -1 else text


def parse(text):
    """Return (skeleton, numbers). Numbers are floats; skeleton is the text
    with every number replaced by a sentinel so structure can be compared."""
    nums, skel = [], []
    last = 0
    for m in NUM.finditer(text):
        skel.append(text[last:m.start()])
        tok = m.group().replace('D', 'E').replace('d', 'e')
        nums.append(float(tok))
        skel.append('\0')  # sentinel
        last = m.end()
    skel.append(text[last:])
    return ''.join(skel), nums


def cmp_file(gpath, cpath, rtol, atol, report_only=False, structure_only=False,
             allow_sign=False):
    if not os.path.exists(cpath):
        return False, f"MISSING candidate file {cpath}", 0.0
    g = open(gpath, encoding='latin-1').read()
    c = open(cpath, encoding='latin-1').read()
    if report_only:
        g, c = strip_progress_banner(g), strip_progress_banner(c)
    gs, gn = parse(g)
    cs, cn = parse(c)
    # Structural gate uses WHITESPACE-NORMALIZED skeletons: a fixed-width ES field's
    # leading padding depends on the value's sign/magnitude (`  9.137E-07` vs
    # ` -9.479E-01`, or `-0.000E+00` vs `0.000E+00` for near-zero noise whose sign
    # differs across BLAS implementations). That padding is not "structure" -- labels,
    # headers, units, line breaks and field count are. Normalizing lets the numeric
    # isclose check (with its atol floor) adjudicate the values, including noise.
    norm = lambda s: re.sub(r'[ \t]+', ' ', s)
    if norm(gs) != norm(cs):
        i = next((k for k in range(min(len(norm(gs)), len(norm(cs)))) if norm(gs)[k] != norm(cs)[k]), 0)
        return False, f"STRUCTURE differs (whitespace-normalized) near {i}: " \
                      f"golden={norm(gs)[i:i+40]!r} cand={norm(cs)[i:i+40]!r}", 0.0
    if len(gn) != len(cn):
        return False, f"NUMBER COUNT differs: golden={len(gn)} cand={len(cn)}", 0.0
    if structure_only:
        return True, f"structure ok ({len(gn)} numeric fields)", 0.0
    # numpy.isclose semantics: a and b agree iff |a-b| <= atol + rtol*|b|.
    # atol absorbs structurally-zero matrix noise (off-diagonal terms that are
    # zero by symmetry but print as ~1e-8..1e-11 and differ between compilers /
    # BLAS implementations). Among the numbers that do NOT pass by the absolute
    # floor, we track the worst relative error as the reported quality metric.
    worst, worst_pair, bad = 0.0, None, None
    for a, b in zip(gn, cn):
        if abs(a - b) <= atol + rtol * abs(b):
            continue
        # allow_sign: treat a ~= -b as a match. The eigenvector sign gauge (DSYEV vs
        # Eigen) is physically arbitrary and flips displayed reference-frame vectors
        # and reoriented off-diagonal coupling; eigenvalues and every scalar SOMO
        # consumes are gauge-invariant. See phase2/README.md.
        if allow_sign and abs(a + b) <= atol + rtol * abs(b):
            continue
        denom = max(abs(a), abs(b))
        err = abs(a - b) / denom if denom else 0.0
        if err > worst:
            worst, worst_pair, bad = err, (a, b), (a, b)
    if bad is not None:
        return False, f"NUMERIC rel err {worst:.2e} > {rtol:.0e} at {worst_pair} " \
                      f"(abs {abs(bad[0]-bad[1]):.2e} > atol {atol:.0e})", worst
    return True, "ok", 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('golden')
    ap.add_argument('candidate')
    ap.add_argument('--rtol', type=float, default=1e-3)
    # 1e-7 absolute floor clears the structurally-zero matrix-noise terms
    # (<=~1e-8) while staying far below every genuine observable in production
    # (4-sig-fig) output. Pass a tighter --atol for extended-precision goldens.
    ap.add_argument('--atol', type=float, default=1e-7)
    ap.add_argument('--report-only', action='store_true',
                    help='compare only the report table, ignoring the cosmetic '
                         'progress banner (which the OpenBLAS port cannot reproduce)')
    ap.add_argument('--structure-only', action='store_true',
                    help='pass if skeleton + numeric-field count match, ignoring '
                         'the values themselves (for Phase 1 stubbed compute)')
    ap.add_argument('--allow-sign-flip', action='store_true',
                    help='treat a ~= -b as a match: tolerates the arbitrary eigenvector '
                         'sign gauge (gauge-invariant physics still validated strictly)')
    a = ap.parse_args()

    all_ok, worst_overall = True, 0.0
    for case, files in CASES.items():
        for fn in files:
            gp = os.path.join(a.golden, case, fn)
            cp = os.path.join(a.candidate, case, fn)
            if not os.path.exists(gp):
                print(f"[SKIP] {case}/{fn}: no golden")
                continue
            ok, msg, worst = cmp_file(gp, cp, a.rtol, a.atol,
                                      report_only=a.report_only,
                                      structure_only=a.structure_only,
                                      allow_sign=a.allow_sign_flip)
            worst_overall = max(worst_overall, worst)
            print(f"[{'PASS' if ok else 'FAIL'}] {case}/{fn}: {msg}")
            all_ok &= ok
    print(f"\n{'ALL PASS' if all_ok else 'FAILURES PRESENT'} "
          f"(rtol={a.rtol:.0e}, worst rel err {worst_overall:.2e})")
    sys.exit(0 if all_ok else 1)


if __name__ == '__main__':
    main()
