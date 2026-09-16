# Features

* **File tree** — lazily expanded, directories first, dotfiles one keystroke away.
* **Rendered Markdown** — headings, emphasis, lists, task lists, block quotes,
  tables, thematic breaks, links and fenced code blocks.
* **Syntax highlighting** — a small built-in tokenizer for C/C++, Python,
  JavaScript/TypeScript, shell, JSON, YAML/TOML, CSS and SQL. Unknown languages
  still get comments, strings and numbers highlighted.
* **Proper text layout** — Unicode-aware wrapping that measures *display
  columns*, so CJK text and emoji do not shear your tables.
* **Live reload** — edit the file in another window and the view updates.
* **Resize-aware** — the layout is recomputed from the terminal size, so
  resizing reflows the document instead of scrambling it.
* **Sensible fallbacks** — ASCII borders for terminals without box drawing, no
  colour when colour would be wrong, and a plain `--print` mode when there is no
  terminal at all.

## Requirements

| Platform | Compiler | Notes |
| --- | --- | --- |
| Windows | MSVC (`cl.exe`) | No MinGW, no Cygwin, no Visual Studio IDE required |
| Linux | GCC or Clang | Needs CMake >= 3.20 |
| macOS | Clang | Needs CMake >= 3.20 |

The only build dependency is CMake 3.20 or newer and a C++20 compiler. There
are **no third-party libraries** to fetch, vendor or configure — not even for
the tests.

Windows + MSVC is the target that has actually been built and exercised here
(clean `/W4` build, full test suite). The Linux and macOS paths are written and
wired into CMake, but no OS other than Windows was available to verify them on,
so treat them as untested rather than proven.
