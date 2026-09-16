# mdview

A terminal **Markdown workspace**: browse a directory tree on the left, read
beautifully rendered Markdown on the right, and never leave the terminal.

Built as portable C++20 with **one** CMake build that produces the same
application on Windows (MSVC), Linux (GCC/Clang) and macOS (Clang).

```text
mdview .
```

---

## Features

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
| Linux | GCC or Clang | Needs CMake ≥ 3.20 |
| macOS | Clang | Needs CMake ≥ 3.20 |

The only build dependency is CMake 3.20 or newer and a C++20 compiler. There
are **no third-party libraries** to fetch, vendor or configure — not even for
the tests.

Windows + MSVC is the target that has actually been built and exercised here
(clean `/W4` build, full test suite). The Linux and macOS paths are written and
wired into CMake, but no OS other than Windows was available to verify them on,
so treat them as untested rather than proven.

## Building

### Windows (MSVC) — the primary target

```bat
cmake -S . -B build
cmake --build build --config Release
build\Release\mdview.exe --version
build\Release\mdview.exe .
```

The generator is deliberately **not** hard-coded: omitting `-G` makes CMake choose
the newest Visual Studio generator that is actually installed, and the script
echoes back the one it ended up with (`Configured with "Visual Studio 18 2026"`).
If you want to pin a specific generator:

```bat
cmake -S . -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
```

The convenience script does all of this — locating `vcvars64.bat` through
`vswhere` so it works from any shell without a Developer Command Prompt, and
reconfiguring automatically if you switch generators in an existing build tree:

```bat
scripts\build.bat                       :: newest VS generator, Release
scripts\build.bat --ninja               :: Ninja instead (build-ninja\mdview.exe)
scripts\build.bat --generator "NAME"    :: pin a specific CMake generator
scripts\build.bat --test                :: build, then run the test suite
scripts\build.bat --debug               :: Debug configuration
scripts\build.bat --clean               :: wipe the build directory first
scripts\build.bat --help                :: all options
scripts\check-toolchain.bat             :: report what compilers CMake can see
```

### Linux and macOS

```sh
cmake -S . -B build
cmake --build build --config Release
./build/mdview --version
./build/mdview .
```

With a single-configuration generator the executable lands directly in
`build/`; with a multi-configuration generator (Visual Studio, Xcode) it lands
in `build/<Config>/`, which is why the commands above differ only in the path.

## Usage

```text
mdview [options] [path]
```

`path` is a Markdown file or a directory. A directory opens its tree and
previews the best candidate document (`README.md` first, then `index.md`). With
no argument, the current directory is used.

| Option | Meaning |
| --- | --- |
| `-h`, `--help` | Show help and exit |
| `-V`, `--version` | Show the version and exit |
| `-p`, `--print` | Render once to stdout instead of starting the TUI |
| `-w`, `--width N` | Wrap width (default: terminal width, 100 when printing) |
| `--no-color` | Never emit SGR colour |
| `--ascii` | Use ASCII instead of Unicode box drawing |
| `--no-wrap` | Turn off word wrapping and scroll horizontally |
| `--no-tree` | Start with the file tree hidden |
| `-a`, `--hidden` | Show dotfiles |

`--print` is the same parser and renderer without the interactive shell, which
makes it useful for piping and for scripting:

```sh
mdview --print --width 100 README.md | less -R
```

Everything is decided per stream, so `mdview --print` writes plain text when
piped and coloured text when a terminal is attached.

## Keys

| Key | Action |
| --- | --- |
| `Tab` | Switch between the tree and the document |
| `Up` / `Down` | Move the selection, or scroll the document |
| `Left` / `Right` | Collapse/expand a directory, or scroll horizontally |
| `PgUp` / `PgDn`, `Ctrl+U` | Page through the document |
| `Home` / `End`, `g` / `G` | Jump to the start or the end |
| `Enter` | Open the selected file, or toggle a directory |
| `[` / `]` | Jump to the previous or next heading |
| `r` | Reload the current file from disk |
| `R` | Refresh the file tree |
| `w` | Toggle word wrapping |
| `.` | Show or hide dotfiles |
| `t` | Show or hide the file tree |
| `?` | Toggle the help overlay |
| `q`, `Ctrl+C` | Quit |

`j`/`k`/`h`/`l` work as Vim-style aliases for the arrow keys.

## Markdown support

| Construct | Status |
| --- | --- |
| ATX and Setext headings | Yes |
| Paragraphs with soft and hard breaks | Yes |
| Emphasis, strong, strikethrough, inline code | Yes |
| Links, autolinks, images | Yes |
| Unordered, ordered and nested lists | Yes |
| Task lists (`- [x]`) | Yes |
| Block quotes, including nesting | Yes |
| Fenced and indented code blocks | Yes |
| GFM pipe tables with alignment | Yes |
| Thematic breaks | Yes |
| Raw HTML | Passed through as text (not rendered) |
| Reference-style links | Treated as literal text |

## Architecture

The whole point of the design is the direction of the dependencies:

```text
                        mdview
                           │
                     Application
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
   Filesystem          Markdown              UI
   (std::filesystem)    Parser            (widgets)
        │                  │                  │
        └──────────────────┼──────────────────┘
                           │
                     Terminal layer
                  (Screen / Input / Style)
                           │
                     Platform layer
        ┌──────────────────┼──────────────────┐
        │                  │                  │
     Windows            Linux               macOS
      MSVC              GCC                Clang
  Win32 console      termios/poll       termios/poll
```

* `src/terminal/` is the **only** seam to the operating system. Everything above
  it — parsing, rendering, the tree model, every widget — is written once and
  never asks which platform it runs on.
* `src/platform/` contains the genuinely platform-specific code: the Win32
  console implementation under `windows/`, and the shared termios/poll
  implementation under `posix/` for Linux and macOS. `#ifdef _WIN32` appears
  nowhere outside those directories.
* The filesystem is written entirely with `<filesystem>` and
  `std::filesystem::path`, so there is no `CreateFile`, `FindFirstFile`,
  `GetFileAttributes` or manual `"\\"` concatenation anywhere.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full walk-through,
including why the widgets redraw the entire scene every frame and how the
screen buffer turns that into a handful of bytes on the wire.

## Testing

```sh
cmake -S . -B build && cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The suite has no third-party dependency: it is a tiny header-only harness plus
CMake's `add_test`. It covers UTF-8 decoding and display width, the ANSI input
decoder, colour degradation, the cell grid (including wide glyphs, clipping and
diff flushing), the Markdown parser and renderer, the syntax highlighter, file
IO, the tree model, the polling watcher, CLI parsing, layout, and a headless
end-to-end pass that drives the real application through scripted keystrokes.

## Project layout

```text
CMakeLists.txt
src/
  main.cpp          entry point and the non-interactive --print renderer
  app/              layout + the frame loop
  cli/              option parsing, help and version text
  filesystem/       file IO, the tree model, the watcher interface
  markdown/         parser, highlighter, renderer
  ui/               tree view, document view, header, status bar, help
  terminal/         Terminal / Screen / Input / Style interfaces
  platform/         windows/  posix/  linux/  macos/
  utils/            UTF-8, geometry, string and time helpers
tests/
docs/
scripts/
```

## Licence

Copyright (c) 2026 [iamuday2006](https://github.com/iamuday2006). All rights reserved.

Provided as-is for use and modification.

**Repository:** https://github.com/iamuday2006/mdview
