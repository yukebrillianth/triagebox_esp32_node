#!/usr/bin/env python3
"""Generate LVGL raw icons from the Lucide icon set (MIT licensed).

Downloads Lucide SVGs and renders them to 256x256 PNGs with a transparent
background and a solid stroke color (black by default), into ``raw/``.
These raw PNGs are downscaled to ``#icon_size`` by the ``<convert>`` entries
in ``globals.xml`` and recolored at the use-site.

Usage
-----
Regenerate the whole default set:

    python3 gen_icons.py

Add one or more new icons (``lucide-name`` or ``lucide-name:output-name``):

    python3 gen_icons.py wind volume-x:mute battery-charging

Each run prints the ``<convert>`` / ``<data>`` XML lines to paste into the
``<images>`` block of ``globals.xml`` for any icons it produced.

Options
-------
    --color  #RRGGBB   stroke color (default: #000000)
    --size   N         output square size in px (default: 256)
    --list             list the names in the default set and exit

Requires: ``cairosvg`` (``pip install --break-system-packages cairosvg``)
and network access to raw.githubusercontent.com.
Browse available icon names at https://lucide.dev/icons/
"""
import argparse
import os
import sys
import urllib.request

RAW_URL = "https://raw.githubusercontent.com/lucide-icons/lucide/main/icons/{}.svg"
HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(HERE, "raw")

# Default set. Each entry is (lucide name, output basename). The output name is
# what you reference as the <data name="..."> in globals.xml.
DEFAULT_ICONS = [
    ("house", "home"),
    ("arrow-up", "arrow_up"),
    ("arrow-down", "arrow_down"),
    ("arrow-left", "arrow_left"),
    ("arrow-right", "arrow_right"),
    ("chevron-up", "chevron_up"),
    ("chevron-down", "chevron_down"),
    ("chevron-left", "chevron_left"),
    ("chevron-right", "chevron_right"),
    ("x", "close"),
    ("check", "check"),
    ("plus", "plus"),
    ("minus", "minus"),
    ("menu", "menu"),
    ("settings", "settings"),
    ("search", "search"),
    ("heart", "heart"),
    ("star", "star"),
    ("bell", "bell"),
    ("user", "user"),
    ("wifi", "wifi"),
    ("wifi-high", "wifi_high"),
    ("wifi-low", "wifi_low"),
    ("wifi-zero", "wifi_zero"),
    ("bluetooth", "bluetooth"),
    ("battery", "battery"),
    ("battery-full", "battery_full"),
    ("signal", "signal"),
    ("play", "play"),
    ("pause", "pause"),
    ("volume-2", "volume"),
    ("trash-2", "trash"),
    ("pencil", "edit"),
    ("download", "download"),
    ("upload", "upload"),
    ("lock", "lock"),
    ("lock-open", "unlock"),
    ("calendar", "calendar"),
    ("clock", "clock"),
    ("mail", "mail"),
    ("camera", "camera"),
    ("sun", "sun"),
    ("moon", "moon"),
    ("power", "power"),
    ("refresh-cw", "refresh"),
    ("info", "info"),
]


def render(name, out, color, size):
    """Download one Lucide icon and write raw/<out>.png. Returns True on success."""
    import cairosvg
    try:
        with urllib.request.urlopen(RAW_URL.format(name), timeout=20) as r:
            svg = r.read().decode("utf-8")
    except Exception as e:
        print(f"  ! {name}: {e}", file=sys.stderr)
        return False
    # Lucide strokes use currentColor; substitute the requested stroke color.
    svg = svg.replace("currentColor", color)
    dst = os.path.join(OUT_DIR, out + ".png")
    cairosvg.svg2png(bytestring=svg.encode("utf-8"), write_to=dst,
                     output_width=size, output_height=size)
    return True


def parse_spec(spec):
    """'lucide-name' or 'lucide-name:output' -> (lucide_name, output_basename)."""
    if ":" in spec:
        name, out = spec.split(":", 1)
    else:
        name, out = spec, spec.replace("-", "_")
    return name, out


def main():
    ap = argparse.ArgumentParser(description="Generate LVGL raw icons from Lucide.")
    ap.add_argument("icons", nargs="*",
                    help="icons to add as 'lucide-name' or 'lucide-name:output-name'")
    ap.add_argument("--color", default="#000000", help="stroke color (default #000000)")
    ap.add_argument("--size", type=int, default=256, help="output size in px (default 256)")
    ap.add_argument("--list", action="store_true", help="list the default set and exit")
    args = ap.parse_args()

    if args.list:
        for name, out in DEFAULT_ICONS:
            print(f"{out:16} (lucide: {name})")
        return

    icons = [parse_spec(s) for s in args.icons] if args.icons else DEFAULT_ICONS
    os.makedirs(OUT_DIR, exist_ok=True)

    done = [(n, o) for n, o in icons if render(n, o, args.color, args.size)]
    print(f"Generated {len(done)}/{len(icons)} icons in {os.path.relpath(OUT_DIR, HERE)}/")

    if done:
        print("\nAdd these to the <images> block of globals.xml:\n")
        for _, out in done:
            print(f'\t\t<convert src="images/icons/raw/{out}.png" '
                  f'dest="images/icons/{out}.png" width="#icon_size" '
                  f'color_format="argb8888" />')
            print(f'\t\t<data name="{out}" src_path="images/icons/{out}.png" '
                  f'color_format="argb8888" />')


if __name__ == "__main__":
    main()
