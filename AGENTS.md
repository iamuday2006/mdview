# AGENTS.md — AI Agent Instructions

## Project

**mdview** — A terminal Markdown workspace: file tree on the left, rendered
document on the right. Native C++20, zero dependencies, one CMake build.

- **Author:** iamuday2006
- **License:** MIT
- **Repository:** https://github.com/iamuday2006/mdview
- **Primary platform:** Windows + MSVC
- **Secondary:** Linux (GCC/Clang), macOS (Clang)

## Build

```bash
scripts\build.bat                          # Windows (auto-detects VS)
cmake -S . -B build && cmake --build build --config Release  # Manual

ctest --test-dir build -C Release --output-on-failure        # Tests
scripts\build-installer.bat                # Inno Setup installer
```

## Architecture Rule

> **Ported, not branched.** Platform differences are absorbed by a portable
> interface, never by `#ifdef` in UI, parser, renderer, or app code.

- `#ifdef _WIN32` exists in exactly **two files** under `src/platform/windows/`
- No file above `src/platform/` includes an OS header
- The app runs against a `HeadlessTerminal` in tests — no real terminal needed

## Directory Structure

```
src/
  main.cpp              Entry point + --print mode
  app/                  Frame layout, event/render loop
  cli/                  Option parsing, usage text
  filesystem/           File reading, tree model, FileWatcher
  markdown/             Parser, renderer, highlighter
  terminal/             Terminal, Screen, InputDecoder, Style/Theme
  platform/             OS-specific: windows/, posix/, linux/, macos/
  ui/                   Tree view, document view, status bar, help
  utils/                UTF-8, display width, geometry, strings
tests/                  Header-only harness, 107 tests
installer/              Inno Setup script + output
scripts/                Build utilities
docs/                   Architecture, features, usage, keys
```

## Code Style

- C++20, no extensions (`CXX_EXTENSIONS OFF`)
- MSVC: `/W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc`
- GCC/Clang: `-Wall -Wextra -Wpedantic`
- Zero third-party libraries — this is the point
- Paths use `std::filesystem::path`, never manual separator concatenation
- No MSVC-only secure CRT (`_dupenv_s`, `fopen_s`, etc.)

## Key Design Decisions

| Decision | Reason |
|----------|--------|
| Full redraw every frame | Removes all dirty-rect tracking from widgets |
| Diff-based flush | Idle frame writes zero bytes |
| Polling file watcher | Identical behavior on all platforms; native backend drops in later |
| Hand-written Markdown parser | No dependency to build under MSVC/GCC/Clang |
| Hand-written highlighter | Same reason; tokenizer is enough for readability |
| Shared POSIX terminal | Linux + macOS have identical `termios`/`poll` API |
| Capabilities-driven drawing | Nothing assumes ANSI, Unicode, or colour |

## Layers (from codebase-memory-mcp)

| Layer | Packages | Role |
|-------|----------|------|
| **Core** | `terminal` (113), `utils` (53) | High fan-in, foundational |
| **Domain** | `markdown` (90), `filesystem` (41) | Business logic |
| **Entry** | `app` (66), `ui` (17), `cli` (6) | Outbound calls only |

## Hotspots (most referenced)

1. `HeadlessTerminal.size` — 95 callers
2. `Rect.empty` — 85 callers
3. `Screen.clear` — 21 callers
4. `FileTree.find` — 17 callers
5. `Screen.resize` — 13 callers

## Testing

```bash
ctest --test-dir build -C Release --output-on-failure
ctest --test-dir build -C Release -R <name>  # Single test
```

Tests cover: UTF-8, ANSI input, colour degradation, screen grid, parser,
renderer, highlighter, file IO, tree model, CLI, layout, headless E2E.

## When Making Changes

1. **Follow the dependency rule** — no `#ifdef` above `src/platform/`
2. **Check neighbors** for style, includes, naming conventions
3. **No new dependencies** — if you think you need one, reconsider
4. **Run tests** after every change
5. **Platform additions** — only `src/platform/<os>/` + `CMakeLists.txt`

## MCP Tools Available

This project is indexed in `codebase-memory-mcp`. Use:
- `search_graph` — find functions, classes, variables
- `trace_path` — trace callers/callees
- `get_code_snippet` — read function/class source
- `query_graph` — Cypher queries for complex patterns
- `get_architecture` — project overview
