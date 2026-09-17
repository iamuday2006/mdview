# CLAUDE.md — Instructions for Claude

## Project

**mdview** — A terminal Markdown workspace. File tree on the left, rendered
document on the right. Native C++20, zero dependencies, one CMake build.

- **Version:** 0.1.0
- **Author:** iamuday2006
- **License:** MIT
- **Repository:** https://github.com/iamuday2006/mdview

## Quick Reference

```bash
# Build (Windows)
scripts\build.bat

# Build (manual)
cmake -S . -B build
cmake --build build --config Release

# Test
ctest --test-dir build -C Release --output-on-failure

# Build installer
scripts\build-installer.bat
```

## Architecture

The one rule that governs everything:

> **Ported, not branched.** Platform differences are absorbed by a portable
> interface, never by `#ifdef` in the code that consumes it.

### Layer Diagram

```
                    mdview
                       │
                 Application            src/app/
                       │
    ┌──────────────────┼──────────────────┐
    │                  │                  │
Filesystem          Markdown              UI
<filesystem>       parser + render      widgets
    │                  │                  │
    └──────────────────┼──────────────────┘
                       │
                 Terminal layer         src/terminal/
           Screen · Input · Style · Terminal
                       │
                 Platform layer         src/platform/
    ┌──────────────────┼──────────────────┐
    │                  │                  │
 Windows            Linux               macOS
  MSVC              GCC                Clang
```

### Key Boundaries

- `#ifdef _WIN32` exists in exactly **two files** under `src/platform/windows/`
- No file above `src/platform/` includes an OS header
- `windows.h` appears in exactly one file: `src/platform/windows/terminal_windows.cpp`
- The app runs against a `HeadlessTerminal` in tests

## Directory Map

| Path | What it does |
|------|--------------|
| `src/main.cpp` | Entry point, `--print` mode. Nothing else. |
| `src/app/` | Frame layout, event/render loop, all state |
| `src/cli/` | Option parsing, usage text |
| `src/filesystem/` | File reading, tree model, `FileWatcher` |
| `src/markdown/` | Parser, renderer, highlighter |
| `src/terminal/` | The seam: `Terminal`, `Screen`, `InputDecoder`, `Style` |
| `src/platform/` | Only genuinely platform-specific code |
| `src/ui/` | Pure drawing: tree, document, header, status, help |
| `src/utils/` | UTF-8, display width, geometry, strings |
| `tests/` | Header-only harness, 107 tests |
| `installer/` | Inno Setup script and output |
| `scripts/` | Build utilities |

## Code Style

- **C++20** with `CXX_EXTENSIONS OFF`
- MSVC: `/W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc`
- GCC/Clang: `-Wall -Wextra -Wpedantic`
- Zero third-party libraries
- Paths use `std::filesystem::path`, never manual `"\\"` or `"/"` concatenation
- No MSVC-only secure CRT (`_dupenv_s`, `fopen_s`, etc.)

## Design Decisions

| Decision | Why |
|----------|-----|
| Full redraw every frame | No dirty-rect tracking in widgets |
| Diff-based flush | Idle frame writes zero bytes |
| Polling file watcher | Identical on all platforms; native drops in later |
| Hand-written parser | No dependency to build under MSVC/GCC/Clang |
| Hand-written highlighter | Tokenizer is enough; no heavyweight dependency |
| Shared POSIX terminal | Linux + macOS have identical `termios`/`poll` |
| Capabilities-driven drawing | Nothing assumes ANSI, Unicode, or colour |

## Testing

```bash
ctest --test-dir build -C Release --output-on-failure
ctest --test-dir build -C Release -R <test_name>  # Single test
```

Test coverage:
- UTF-8 (malformed, overlong, surrogate, truncated)
- ANSI input decoder (sequences, modifiers, control bytes)
- Colour degradation and SGR
- Screen grid (wide glyphs, clipping, box drawing, diff flush)
- Parser and renderer for every construct
- File IO, tree model, polling watcher
- CLI parsing, layout geometry
- Headless E2E (real Application on HeadlessTerminal)

## Rules for Changes

1. **Never add `#ifdef` above `src/platform/`**
2. **Check neighboring files** for style, includes, naming
3. **No new dependencies** — if you think you need one, reconsider
4. **Run tests** after every change
5. **Platform additions** — only `src/platform/<os>/` + `CMakeLists.txt`
6. **Installer PATH** — never use `uninsdeletevalue`; use Pascal Code

## Hotspots

These are the most-referenced symbols (change with care):

1. `HeadlessTerminal.size` — 95 callers
2. `Rect.empty` — 85 callers
3. `Screen.clear` — 21 callers
4. `FileTree.find` — 17 callers
5. `Screen.resize` — 13 callers

## MCP Tools

This project is indexed in `codebase-memory-mcp`:

| Tool | Use for |
|------|---------|
| `search_graph` | Find functions, classes, variables by pattern |
| `trace_path` | Trace callers/callees |
| `get_code_snippet` | Read function/class source |
| `query_graph` | Cypher queries for complex patterns |
| `get_architecture` | Project overview and metrics |
