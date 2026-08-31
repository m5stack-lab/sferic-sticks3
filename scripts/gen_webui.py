"""Generate src/webui_html.h from web/ (index.html + style.css + app.js).

The station's SoftAP has no internet, so the page must be one self-contained
file. This inlines the stylesheet and script into the HTML and wraps the
result in a C++ raw string literal.

Runs two ways:
  * as a PlatformIO pre-build extra_script (firmware envs) — regenerates on
    every build, but only rewrites the header when the bytes change so it
    doesn't force a needless recompile;
  * standalone: `python scripts/gen_webui.py` (used by CI / pre-commit).
"""

import os
import pathlib
import re
import sys

# PlatformIO runs extra scripts via SCons, where `__file__` is not defined;
# there the build cwd is the project root. Standalone, resolve from the file.
_here = globals().get("__file__")
ROOT = pathlib.Path(_here).resolve().parent.parent if _here else pathlib.Path(os.getcwd())
WEB = ROOT / "web"
OUT = ROOT / "src" / "webui_html.h"
DELIM = "SFERICWEBUI"  # raw-string delimiter: R"SFERICWEBUI( ... )SFERICWEBUI"


def build_html() -> str:
    html = (WEB / "index.html").read_text()
    css = (WEB / "style.css").read_text().strip()
    js = (WEB / "app.js").read_text().strip()

    html = re.sub(
        r'[ \t]*<link rel="stylesheet" href="style\.css">\s*',
        f"<style>\n{css}\n</style>\n",
        html,
        count=1,
    )
    html = re.sub(
        r'[ \t]*<script src="app\.js"></script>\s*',
        f"<script>\n{js}\n</script>\n",
        html,
        count=1,
    )

    if "style.css" in html or "app.js" in html:
        sys.exit("gen_webui: failed to inline style.css / app.js — check the tags in web/index.html")
    if f"){DELIM}" in html:
        sys.exit(f"gen_webui: page content contains the raw-string delimiter ){DELIM}")
    return html.strip() + "\n"


def render_header(html: str) -> str:
    return (
        "#pragma once\n\n"
        "// The single-page web UI (ADR-0006). GENERATED from web/ by\n"
        "// scripts/gen_webui.py — edit the sources there, not this file.\n"
        f"static const char kWebUiHtml[] = R\"{DELIM}(\n"
        f"{html}"
        f"){DELIM}\";\n"
    )


def main() -> None:
    header = render_header(build_html())
    if OUT.exists() and OUT.read_text() == header:
        return  # unchanged — leave mtime alone
    OUT.write_text(header)
    print(f"gen_webui: wrote {OUT.relative_to(ROOT)} ({len(header)} bytes)")


main()
