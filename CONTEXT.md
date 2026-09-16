# mdview — project context

A terminal Markdown workspace: file tree on the left, rendered document on the
right. Native C++20, no third-party libraries, one CMake build.

Primary environment: **Windows + MSVC (`cl.exe`)**. Portable by design to Linux
(GCC/Clang) and macOS (Clang); see `docs/ARCHITECTURE.md` for the dependency
rule and platform seams.

## Glossary

**Pane** — one of the two focusable regions of the frame: `Tree` or `Content`.
Exactly one pane has focus at a time; `Tab` cycles it.

**Tree** — the left-hand navigation over the filesystem root the user opened.
Lazily expanded: a directory's children are read only when it is expanded.
Directories sort before files, case-insensitively.

**Document** — the parsed in-memory model of the Markdown file currently open.
Produced once per (re)load from file text.

**Rendered document** — the width-dependent, styled lines produced from a
Document for a specific wrap width. Re-produced whenever the document reloads
or the content pane changes width. A Document is width-independent; a rendered
document is not.

**Wrap** — the display mode of the Content pane: either word-wrapped at the pane
width, or unwrapped with horizontal scrolling.

**Heading jump** — motion that moves the viewport to the previous or next
heading line in the rendered document (`[` / `]`).

**Hidden files** — entries whose names begin with a dot. Hidden by default;
toggled per session, never persisted.

**Message** — the transient status line text (e.g. "file reloaded"), flagged as
error or not. Distinct from the status bar's persistent details.

**Live reload** — watching the open file and re-reading it when it changes on
disk. The view keeps its scroll position across a reload.

**Capabilities** — what the connected terminal can do (ANSI addressing,
Unicode, colour depth). Every drawing decision consults these; nothing assumes.

**Theme** — the palette and attribute set used to draw. Two flavours: a
coloured theme degraded to the terminal's colour depth, and a plain theme for
monochrome or colour-disabled sessions.

**Headless terminal** — a test double implementing the Terminal interface so
the real Application can be driven by scripted keystrokes and asserted on
screen, with no OS terminal involved.

**Print mode** — rendering one document to stdout and exiting (`--print`),
sharing the exact parser and renderer of the interactive view.

## Status

Implemented: tree browsing, rendered document view, wrap toggle, tree toggle,
hidden-file toggle, heading jumps, paging/Home/End, live reload, manual reload,
help overlay, resize reflow, print mode, ASCII/colour fallbacks, 107 tests.

Not implemented (roadmap, not features — do not assume they exist):
**search** (document and/or tree), **code/raw source view mode** (viewing the
unrendered Markdown side-by-side or toggled), bookmarks, mouse support.

## Where things live

Deep architecture (dependency rule, seams, trade-offs, how to add a platform):
`docs/ARCHITECTURE.md`. User-facing build/run/keys: `README.md`. Build scripts:
`scripts/build.bat` (detects the installed VS generator — never hard-code one).

The rule that governs every change: platform differences are absorbed by an
interface in `src/terminal/` or `src/platform/`, never by `#ifdef` in UI,
parser, renderer, or app code.
