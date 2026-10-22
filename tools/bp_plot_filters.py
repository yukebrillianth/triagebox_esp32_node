#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy", "matplotlib", "scipy"]
# ///
"""
Plot and compare the ECG filter candidates on a recorded window.

    tools/bp_plot_filters.py bp_window.csv [--fs 100] [--out plots]

Run it with `uv run tools/bp_plot_filters.py ...` -- the header above pins numpy
and matplotlib, so uv fetches them into a throwaway env and nothing is installed
system-wide. Plain `python3 tools/bp_plot_filters.py` works too if those two are
already available.

Reads a CSV from tools/bp_capture_csv.py and draws, for the ECG channel:
  1. the raw signal
  2. this firmware's filter -- RBJ band-pass 5-15 Hz, one biquad
  3. the Butterworth cascade from the other team -- HP order 3 + LP order 4
  4. the same cascade REDESIGNED at this recording's rate (--no-redesign to skip)
  5. all of the above overlaid on one beat-scale window
  6. the magnitude responses, with the -3 dB corners marked
  7. amplitude spectra of raw and filtered signals

The point of the comparison is R-peak TIMING: everything downstream of these
filters (PAT, PTT, the whole feature vector) is a delay measured from the R peak
to the finger pulse, so what matters is which filter leaves peaks that are
findable and unshifted -- not which looks smoother. The detected-peak count and
median rate are printed per filter for exactly that reason.

FS MATTERS AND IS NOT IN THEIR CODE. Their coefficients define corners as a
FRACTION of Fs. Asked 2026-09-05, they design at 256 Hz (Fs from their own
recording's timestamps: 1/(t[1]-t[0])), where the cascade is 1.36-15.36 Hz -- a
proper ECG band. This hardware delivers ~99 Hz, and the SAME numbers there
collapse to 0.5-6 Hz, a PPG band that eats the QRS. So the fair comparison is
not their literal coefficients but their design INTENT: --redesign rebuilds the
same orders (HP 3 at 1.36 Hz, LP 4 at 15.36 Hz) at the recording's --fs and
plots both. Their coefficients verbatim are still shown, labelled "wrong Fs",
because that is what copying numbers across a sample-rate boundary produces.

Needs numpy + matplotlib (pip install numpy matplotlib).
"""
import argparse
import sys

try:
    import numpy as np
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError as exc:
    sys.exit(f"missing dependency: {exc}. pip install numpy matplotlib")

# ---- the two filters ------------------------------------------------------

# This firmware, bp_capture.c: RBJ band-pass f0=8.6603 Hz Q=0.866 at Fs=100,
# stored CMSIS-style {b0,b1,b2,-a1,-a2} -- feedback ALREADY NEGATED, so the
# implementation ADDS it. Converted here to the textbook b/a that lfilter wants.
MINE = dict(b=[0.230114, 0.0, -0.230114], a=[1.0, -1.317384, 0.539771])

# The other team's cascade (Pascal source). Their convention subtracts
# `ah[i]*y[n-i]`, i.e. their ah IS the textbook a -- no sign flip needed here,
# unlike the CMSIS arrays above. Getting this backwards puts the poles outside
# the unit circle and the output diverges to inf; that exact mistake cost this
# project weeks, so it is spelled out rather than assumed.
# These literal numbers are their 256 Hz design: at this board's ~99 Hz they are
# the WRONG band, kept in the plots as the "do not copy coefficients across a
# sample rate" reference. THE SAME DESIGN redone at 98.97 Hz is REDESIGNED below.
THEIRS_HP = dict(b=[0.97184831, -2.91554494, 2.91554494, -0.97184831],
                 a=[1.0, -2.94263822, 2.88692428, -0.94427191])
THEIRS_LP = dict(b=[0.00080636, 0.00322544, 0.00483816, 0.00322544, 0.00080636],
                 a=[1.0, -3.01755524, 3.50719372, -1.84755094, 0.37081422])

# Their design INTENT at this hardware's rate: same orders, same absolute
# corners their own coefficients imply at 256 Hz (HP 1.36, LP 15.36), rebuilt
# at the default --fs by scipy.butter. Not used unless --redesign (default on);
# the comment inside main() says why this is the fair version of "theirs".
def butterworth_redesign(fs):
    from scipy.signal import butter
    hp = butter(3, 1.36, "high", fs=fs)
    lp = butter(4, 15.36, "low", fs=fs)
    return dict(hp=dict(b=[float(v) for v in hp[0]], a=[float(v) for v in hp[1]]),
                lp=dict(b=[float(v) for v in lp[0]], a=[float(v) for v in lp[1]]))


def lfilter(b, a, x):
    """Direct-form-I IIR, no scipy: keeps this runnable with numpy alone."""
    y = np.zeros(len(x))
    for n in range(len(x)):
        acc = 0.0
        for i, bi in enumerate(b):
            if n - i >= 0:
                acc += bi * x[n - i]
        for j in range(1, len(a)):
            if n - j >= 0:
                acc -= a[j] * y[n - j]
        y[n] = acc / a[0]
    return y


def response(b, a, f, fs):
    """|H(f)| for a real filter at frequencies f."""
    z = np.exp(-2j * np.pi * f / fs)
    num = sum(bi * z ** i for i, bi in enumerate(b))
    den = sum(ai * z ** i for i, ai in enumerate(a))
    return np.abs(num / den)


def corner(b, a, fs, lo, hi):
    """Bisect for the -3 dB point between lo and hi Hz."""
    target = 1.0 / np.sqrt(2.0)
    rising = response(b, a, np.array([lo]), fs)[0] < target
    for _ in range(80):
        mid = 0.5 * (lo + hi)
        below = response(b, a, np.array([mid]), fs)[0] < target
        if below == rising:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


# ---- peak finding, the thing the filters are actually for -----------------

def peaks(x, fs):
    """
    Same shape as bp_hr_from_ecg() in the firmware: threshold at 60% of the
    range, 250 ms refractory. Deliberately NOT a better detector -- the question
    is which filter the SHIPPED detector works on.
    """
    lo, hi = float(np.min(x)), float(np.max(x))
    if hi - lo < 1e-9:
        return np.array([], dtype=int)
    thresh = lo + 0.6 * (hi - lo)
    refractory = int(0.25 * fs)
    idx, last = [], -refractory - 1
    above = x > thresh
    for n in range(1, len(x)):
        if above[n] and not above[n - 1] and (n - last) > refractory:
            idx.append(n)
            last = n
    return np.array(idx, dtype=int)


def rate_bpm(idx, fs):
    if len(idx) < 3:
        return 0.0
    return 60.0 * fs / float(np.median(np.diff(idx)))


def summarise(name, x, fs):
    p = peaks(x, fs)
    bpm = rate_bpm(p, fs)
    print(f"  {name:<26} {len(p):4d} peaks   median rate "
          f"{bpm:6.1f} bpm   span {np.max(x) - np.min(x):.4g}")
    return p, bpm


# ---- main -----------------------------------------------------------------

def load(path):
    """The CSV, with its `#` header lines kept as a dict of what produced it."""
    meta, cols = [], None
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith("#"):
                meta.append(line.lstrip("# "))
            elif cols is None and line.startswith("i,"):
                cols = line.split(",")
            elif cols is not None:
                rows.append([float(v) for v in line.split(",")])
    if not rows:
        sys.exit(f"{path} has no data rows -- was the recording actually run?")
    data = np.array(rows)
    return {c: data[:, k] for k, c in enumerate(cols)}, meta


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--fs", type=float, default=100.0,
                    help="sample rate of the recording (default 100; the "
                         "hardware measures 98.97, close enough for plots)")
    ap.add_argument("--fs-theirs", type=float, default=256.0,
                    help="the other team's design rate, for their LITERAL "
                         "coefficients' response only (they derive Fs from "
                         "their own recording's timestamps; 256 as of "
                         "2026-09-05)")
    ap.add_argument("--no-redesign", action="store_true",
                    help="skip their cascade rebuilt at --fs (needs scipy)")
    ap.add_argument("--out", default="bp_filters",
                    help="output PNG prefix (default bp_filters)")
    ap.add_argument("--window", type=float, default=6.0,
                    help="seconds to show in the beat-scale overlay")
    args = ap.parse_args()

    cols, meta = load(args.csv)
    fs = args.fs
    raw = cols["raw_ecg"]
    t = np.arange(len(raw)) / fs

    print(f"{args.csv}: {len(raw)} samples, {len(raw) / fs:.1f}s at {fs} Hz")
    for line in meta:
        print(f"  # {line}")

    # Remove the DC offset before either filter so the plots are comparable:
    # both filters high-pass anyway, this just stops the transient dominating
    # the y-axis. Not a filter choice -- it happens identically to both.
    x = raw - np.mean(raw)

    mine = lfilter(MINE["b"], MINE["a"], x)
    theirs = lfilter(THEIRS_LP["b"], THEIRS_LP["a"],
                     lfilter(THEIRS_HP["b"], THEIRS_HP["a"], x))
    firmware = cols.get("fil_ecg")

    # Their design at OUR rate: same orders, same physical corners their own
    # coefficients imply at 256 Hz. The literal arrays above stay in the plots
    # as the "coefficients copied across a sample rate" lesson.
    theirs_fixed = None
    if not args.no_redesign:
        c = butterworth_redesign(fs)
        theirs_fixed = lfilter(c["lp"]["b"], c["lp"]["a"],
                               lfilter(c["hp"]["b"], c["hp"]["a"], x))

    print("\nR-peak detection with the SHIPPED detector "
          "(60% threshold, 250 ms refractory):")
    p_raw, _ = summarise("raw (no filter)", x, fs)
    p_mine, bpm_mine = summarise("mine: RBJ BP 5-15 Hz", mine, fs)
    p_theirs, _ = summarise("theirs @256 run @99 (wrong Fs)", theirs, fs)
    if theirs_fixed is not None:
        p_fixed, _ = summarise("theirs rebuilt @99: 1.4-15.4 Hz",
                               theirs_fixed, fs)
        # R-instant agreement, the number the BP model actually consumes: the
        # features are delays FROM the R wave, so a peak-by-peak offset between
        # filters is a per-feature bias, not a timing error that cancels.
        d = [int(np.min(np.abs(p_fixed - m))) for m in p_mine]
        matched = sum(1 for v in d if v <= int(0.25 * fs))
        print(f"    R-instant vs mine: {matched}/{len(p_mine)} peaks within "
              f"refractory, median offset {np.median(d) / fs * 1000:.0f} ms, "
              f"max {np.max(d) / fs * 1000:.0f} ms")
    if firmware is not None:
        summarise("fil_ecg (from the device)", firmware, fs)
    print("\n  A plausible adult rate is 50-100 bpm. A filter whose peak count "
          "or rate\n  disagrees with the others is the one distorting the "
          "timing the model needs.")

    plot_signals(t, x, mine, theirs, firmware, p_mine, p_theirs, args, fs,
                 theirs_fixed)
    plot_responses(args, fs)
    plot_spectra(x, mine, theirs, fs, args, theirs_fixed)
    print(f"\nwrote {args.out}_signals.png, {args.out}_response.png, "
          f"{args.out}_spectra.png")


def plot_signals(t, x, mine, theirs, firmware, p_mine, p_theirs, args, fs,
                 theirs_fixed=None):
    n = min(len(x), int(args.window * fs))
    # raw + mine + theirs + optional rebuilt + optional device + overlay
    rows = 3 + (theirs_fixed is not None) + (firmware is not None) + 1
    fig, ax = plt.subplots(rows, 1, figsize=(13, 2.4 * rows), sharex=True)

    ax[0].plot(t, x, lw=0.6, color="#444")
    ax[0].set_ylabel("raw\n(DC removed)")
    k = 1
    ax[k].plot(t, mine, lw=0.6, color="#005F73")
    ax[k].plot(t[p_mine], mine[p_mine], "v", ms=4, color="#C1121F")
    ax[k].set_ylabel("mine\nRBJ 5-15 Hz")
    k += 1
    ax[k].plot(t, theirs, lw=0.6, color="#BB3E03")
    ax[k].plot(t[p_theirs], theirs[p_theirs], "v", ms=4, color="#C1121F")
    ax[k].set_ylabel("theirs\n@256 run @99")
    k += 1
    if theirs_fixed is not None:
        ax[k].plot(t, theirs_fixed, lw=0.6, color="#9D4EDD")
        ax[k].set_ylabel("theirs rebuilt\n@99: 1.4-15.4 Hz")
        k += 1
    if firmware is not None:
        ax[k].plot(t, firmware, lw=0.6, color="#4A5568")
        ax[k].set_ylabel("fil_ecg\n(on device)")
        k += 1

    # Beat scale, normalised, so shape and PHASE are comparable -- the filters
    # have wildly different gains and the absolute scale hides the shift that
    # actually matters to a delay measurement.
    def unit(v):
        s = np.max(np.abs(v[:n])) or 1.0
        return v[:n] / s

    ax[k].plot(t[:n], unit(x), lw=0.8, color="#999", label="raw")
    ax[k].plot(t[:n], unit(mine), lw=1.0, color="#005F73", label="mine")
    ax[k].plot(t[:n], unit(theirs), lw=1.0, color="#BB3E03",
               label="theirs (wrong Fs)")
    if theirs_fixed is not None:
        ax[k].plot(t[:n], unit(theirs_fixed), lw=1.0, color="#9D4EDD",
                   label="theirs rebuilt")
    ax[k].set_ylabel(f"overlay\nfirst {args.window:g}s, normalised")
    ax[k].set_xlabel("seconds")
    ax[k].legend(loc="upper right", fontsize=8)
    ax[k].set_xlim(0, args.window)

    for a in ax:
        a.grid(alpha=0.25, lw=0.5)
    fig.suptitle("ECG channel: raw vs both candidate filters "
                 "(markers = peaks the shipped detector finds)", y=0.995)
    fig.tight_layout()
    fig.savefig(f"{args.out}_signals.png", dpi=130)
    plt.close(fig)


def plot_responses(args, fs):
    fs_theirs = args.fs_theirs or fs
    f = np.linspace(0.01, fs / 2, 2000)
    mine_mag = response(MINE["b"], MINE["a"], f, fs)
    hp = response(THEIRS_HP["b"], THEIRS_HP["a"], f, fs_theirs)
    lp = response(THEIRS_LP["b"], THEIRS_LP["a"], f, fs_theirs)
    hp_fix = lp_fix = None
    if not args.no_redesign:
        c = butterworth_redesign(fs)
        hp_fix = response(c["hp"]["b"], c["hp"]["a"], f, fs)
        lp_fix = response(c["lp"]["b"], c["lp"]["a"], f, fs)

    fig, ax = plt.subplots(figsize=(11, 5))
    ax.plot(f, mine_mag, color="#005F73", lw=1.6,
            label=f"mine: RBJ band-pass @ Fs={fs:g}")
    ax.plot(f, hp * lp, color="#BB3E03", lw=1.6,
            label=f"theirs @ their Fs={fs_theirs:g} (design rate)")
    ax.plot(f, hp, color="#BB3E03", lw=0.8, ls=":", alpha=0.7, label="  their HP")
    ax.plot(f, lp, color="#BB3E03", lw=0.8, ls="--", alpha=0.7, label="  their LP")
    ax.axhline(1 / np.sqrt(2), color="#888", lw=0.8, ls="-.")
    ax.text(f[-1], 1 / np.sqrt(2), " -3 dB", va="center", fontsize=8,
            color="#666")

    # The QRS band, which is what these filters exist to pass.
    ax.axvspan(5, 15, color="#00D460", alpha=0.10)
    ax.text(10, 1.12, "QRS energy (5-15 Hz)", ha="center", fontsize=8,
            color="#2F855A")

    for b, a, lo, hi, col in (
        (MINE["b"], MINE["a"], 0.01, 8.6, "#005F73"),
        (MINE["b"], MINE["a"], 8.6, fs / 2, "#005F73"),
    ):
        fc = corner(b, a, fs, lo, hi)
        ax.axvline(fc, color=col, lw=0.7, ls=":", alpha=0.6)
        ax.text(fc, 0.03, f"{fc:.2f}", rotation=90, fontsize=7, color=col)
    for b, a, lo, hi in ((THEIRS_HP["b"], THEIRS_HP["a"], 0.001, fs_theirs / 4),
                         (THEIRS_LP["b"], THEIRS_LP["a"], fs_theirs / 4, 0.001)):
        fc = corner(b, a, fs_theirs, lo, hi)
        ax.axvline(fc, color="#BB3E03", lw=0.7, ls=":", alpha=0.6)
        ax.text(fc, 0.03, f"{fc:.2f}", rotation=90, fontsize=7, color="#BB3E03")
    if hp_fix is not None:
        ax.plot(f, hp_fix * lp_fix, color="#9D4EDD", lw=1.6,
                label=f"theirs rebuilt @ Fs={fs:g}")
        c = butterworth_redesign(fs)
        for b, a, lo, hi in ((c["hp"]["b"], c["hp"]["a"], 0.001, fs / 4),
                             (c["lp"]["b"], c["lp"]["a"], fs / 4, 0.001)):
            fc = corner(b, a, fs, lo, hi)
            ax.axvline(fc, color="#9D4EDD", lw=0.7, ls=":", alpha=0.6)
            ax.text(fc, 0.03, f"{fc:.2f}", rotation=90, fontsize=7,
                    color="#9D4EDD")

    ax.set_xlim(0, min(60, fs / 2))
    ax.set_ylim(0, 1.25)
    ax.set_xlabel("Hz")
    ax.set_ylabel("|H(f)|")
    ax.set_title("Magnitude response. Dotted lines are -3 dB corners; the green "
                 "band is the QRS energy an R-peak detector needs")
    ax.grid(alpha=0.25, lw=0.5)
    ax.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(f"{args.out}_response.png", dpi=130)
    plt.close(fig)


def plot_spectra(x, mine, theirs, fs, args, theirs_fixed=None):
    def spec(v):
        w = np.hanning(len(v))
        mag = np.abs(np.fft.rfft(v * w)) / len(v)
        return np.fft.rfftfreq(len(v), 1.0 / fs), mag

    fig, ax = plt.subplots(figsize=(11, 5))
    traces = [(x, "raw", "#999"), (mine, "mine", "#005F73"),
              (theirs, "theirs (wrong Fs)", "#BB3E03")]
    if theirs_fixed is not None:
        traces.append((theirs_fixed, "theirs rebuilt", "#9D4EDD"))
    for v, name, col in traces:
        f, m = spec(v)
        ax.semilogy(f, m + 1e-12, lw=0.8, color=col, label=name)
    ax.axvspan(5, 15, color="#00D460", alpha=0.10)
    # 50 Hz is the mains here; at Fs=100 it lands exactly on Nyquist and folds,
    # which is worth seeing rather than assuming away.
    for hz in (50.0, 25.0):
        if hz < fs / 2:
            ax.axvline(hz, color="#C1121F", lw=0.7, ls=":", alpha=0.7)
            ax.text(hz, ax.get_ylim()[1], f" {hz:g}Hz", fontsize=7,
                    color="#C1121F", va="top")
    ax.set_xlim(0, fs / 2)
    ax.set_xlabel("Hz")
    ax.set_ylabel("amplitude (log)")
    ax.set_title("Spectrum. Green band is the QRS energy the R-peak detector "
                 "needs; anything a filter leaves outside it is noise it passed")
    ax.grid(alpha=0.25, lw=0.5, which="both")
    ax.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(f"{args.out}_spectra.png", dpi=130)
    plt.close(fig)


if __name__ == "__main__":
    main()
