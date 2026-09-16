# mdview architecture

This document explains *why* the code is shaped the way it is. The short version
is one rule, applied everywhere:

> **Ported, not branched.** When a platform difference appears, it is absorbed by
> a portable interface, never by an `#ifdef` in the code that consumes it.

---

## 1. The dependency rule

```text
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
  Win32 console      termios/poll       termios/poll
```

Every arrow points *downward*. Nothing below knows about anything above, and
nothing above knows which platform it is running on.

Three consequences are worth spelling out, because they are the reason this
design pays for itself:

1. **`#ifdef _WIN32` exists in exactly two files**, both under
   `src/platform/windows/`. There is no `#ifdef` in the parser, the renderer,
   any widget, or the application loop.
2. **No file above `src/platform/` includes an OS header.** `windows.h` appears
   once, in `src/platform/windows/terminal_windows.cpp`.
3. **The application can be driven with no terminal at all.** Because the whole
   UI talks to the abstract `Terminal`, the test suite runs the real
   `Application` against a `HeadlessTerminal` and asserts on the resulting
   screen. That is a portability property *and* a testability property, which is
   usually a sign that the boundary is in the right place.

---

## 2. Directory map

| Path | Responsibility |
| --- | --- |
| `src/main.cpp` | Argument dispatch and the non-interactive `--print` renderer. Nothing else. |
| `src/app/` | Frame layout and the event/render loop. Owns all application state. |
| `src/cli/` | Option parsing, usage and version text. No terminal required, so it is trivially testable. |
| `src/filesystem/` | File reading, the lazily-expanded tree model, the `FileWatcher` interface and its portable implementation. |
| `src/markdown/` | Block parser, inline parser, syntax highlighter, and the width-aware renderer. |
| `src/terminal/` | The seam to the OS: `Terminal`, `Screen`, `InputDecoder`, `Style`/`Theme`. |
| `src/ui/` | Pure drawing: tree view, document view, header bar, status bar, help overlay. |
| `src/platform/` | The only genuinely platform-specific code: `windows/`, `posix/`, plus per-OS identity in `linux/` and `macos/`. |
| `src/utils/` | UTF-8 and display width, geometry, byte-string helpers, timing, environment. |
| `tests/` | A dependency-free test harness and the suite. |

---

## 3. The terminal seam

### 3.1 The interface

`src/terminal/terminal.hpp` declares the contract:

```cpp
class Terminal {
public:
    virtual void initialize() = 0;      // raw mode, alternate screen
    virtual void shutdown() = 0;        // restore exactly what was saved
    virtual bool isInteractive() const = 0;
    virtual TerminalSize size() const = 0;
    virtual void clear() = 0;
    virtual void moveCursor(int x, int y) = 0;
    virtual void showCursor(bool visible) = 0;
    virtual KeyEvent readKey() = 0;
    virtual bool hasPendingInput() = 0;
    virtual void write(std::string_view bytes) = 0;
    virtual void flush() = 0;
    virtual TerminalCapabilities capabilities() const = 0;
};
```

`createTerminal()` is declared here and *defined* in the platform directory, which
is how the portability boundary is expressed in the build system as well as in
the source.

`KeyEvent` is returned rather than a bare `Key` enum because `Key::Character`
needs to carry the actual code point. The rest of the model is exactly the
normalised set you would expect — arrows, `Enter`, `Escape`, `Tab`, `BackTab`,
`Backspace`, `Delete`, `Insert`, `PageUp`, `PageDown`, `Home`, `End`, `F1`–`F12`,
plus dedicated `CtrlC`/`CtrlD`/`CtrlL`/`CtrlR`/`CtrlU`/`CtrlW`/`CtrlZ` values so
that a shortcut never depends on the host's idea of a control character.
`Key::None` means "nothing available yet", which is what lets `readKey()` be
called every frame without blocking for user-perceivable time.

### 3.2 Capabilities, and degrading gracefully

A terminal is not just present or absent: it may fail to support escape
sequences, may not render Unicode, and may only offer the classic 16 colours.
`TerminalCapabilities` carries that:

```cpp
struct TerminalCapabilities {
    bool ansi = false;
    bool unicode = true;
    bool cursorAddressing = false;
    bool alternateScreen = false;
    ColorMode colorMode = ColorMode::None;   // None | Basic | Ansi256 | TrueColor
};
```

Everything downstream consults it rather than assuming:

* `Screen::flush()` uses cursor addressing when `ansi` is set, and falls back to
  `Terminal::moveCursor()` per run when it is not.
* Every box-drawing glyph has an ASCII alternative (`+ - |`), selected from
  `unicode`.
* `sgrFor(style, colorMode)` degrades a colour to what the terminal can express:
  truecolour → xterm-256 → the 16 basics → nothing at all. The palette in
  `Theme::colored()` is written in terms of roles, so a degraded theme still
  reads correctly, and `Theme::plain()` expresses hierarchy purely with
  attributes for monochrome terminals.
* `NO_COLOR` is honoured, as is `--no-color` and `--ascii`.

The important part is that none of this logic is Windows-specific or POSIX-specific.
It is all portable policy that simply reads a capability record.

### 3.3 Two implementations

**`src/platform/windows/terminal_windows.cpp`** — the only place `windows.h`
appears. It:

* enables `ENABLE_VIRTUAL_TERMINAL_PROCESSING` (falling back to `FillConsoleOutputCharacter` /
  `SetConsoleCursorPosition` when that fails) and `DISABLE_NEWLINE_AUTO_RETURN`;
* switches the console code page to UTF-8 and stdout to binary, so the UTF-8 the
  renderer produces is displayed as-is;
* clears `ENABLE_PROCESSED_INPUT` so `Ctrl+C` arrives as a key *event* instead of
  a signal, and `ENABLE_LINE_INPUT`/`ENABLE_ECHO_INPUT`/`ENABLE_QUICK_EDIT_MODE`
  so every keystroke is delivered immediately;
* reads `INPUT_RECORD`s, maps virtual keys to the normalised `Key` set, and joins
  UTF-16 surrogate pairs for astral characters;
* restores the saved console mode, code page and screen buffer on `shutdown()`.

**`src/platform/posix/terminal_posix.cpp`** — shared by Linux and macOS, because
their interfaces really are identical (`termios`, `poll`, `ioctl(TIOCGWINSZ)`,
`SIGWINCH`). Duplicating it into two byte-identical files would be duplication
without benefit, so `src/platform/linux/` and `src/platform/macos/` hold only the
differences that do exist: platform identity and the `FileWatcher` factory.

`SIGWINCH` is handled by setting a `volatile sig_atomic_t` flag; the flag is read
inside `size()`, which is the only place that cares. Escape sequences are parsed
by the shared `InputDecoder`, and a trailing lone `ESC` is resolved by
`InputDecoder::flush()` after a poll timeout, which is how `Escape` is
distinguished from the start of a sequence.

### 3.4 Input normalisation

`InputDecoder` (in the portable `src/terminal/input.cpp`) is the single place
that knows about ANSI input encoding:

* CSI sequences: `ESC [ A/B/C/D`, `ESC [ H/F`, `ESC [ 1~`…`24~`, `ESC [ Z`;
* SS3 sequences: `ESC O A`…`ESC O D`, `ESC O P`…`ESC O S`;
* xterm modifier encodings (`ESC [ 1;5C` → `Ctrl+Right`);
* control bytes (`0x01`–`0x1A`, `0x7F`) mapped to `Ctrl+<letter>` or a dedicated
  `Key`;
* `ESC` followed by an ordinary byte → `Alt+<key>`;
* mouse, focus and other private modes consumed and ignored;
* UTF-8 split across reads held back until the code point is complete.

The Windows implementation reuses the same `controlByteEvent()` helper, so both
platforms agree on what `Ctrl+D` means.

---

## 4. The screen buffer

`src/terminal/screen.hpp` is a platform-neutral grid of cells. Two decisions in
it are worth explaining, because they are what keep the code simple elsewhere.

### 4.1 The application redraws the whole scene every frame

Widgets never track dirty rectangles, never restore what was underneath them, and
never reason about draw order across frames. Each frame:

1. `screen.clear()`;
2. draw the header, tree, divider, document, status bar, and help overlay;
3. `screen.flush(terminal)`.

That is obviously "wasteful" and it is the right trade: an 80×24 grid is under
two thousand cells, redrawing it costs microseconds, and the diff in step 3 makes
the cost on the wire proportional to what actually changed. In exchange, an
overlay can be drawn on top of anything, a pane can be resized and simply
redrawn, and no widget needs an "invalidate" protocol.

### 4.2 Flushing is a diff

`Screen::flush()` compares the frame against the previous one and emits only
changed runs, grouped by identical style, each preceded by a cursor move and at
most one SGR sequence. An idle application therefore writes **zero bytes**, and a
single keystroke that moves a selection writes a few dozen. The first frame after
a resize is forced to a full repaint because the grid itself changed.

### 4.3 Wide glyphs, clipping and overlays

* A wide glyph (CJK, most emoji) occupies a lead cell plus a "tail" cell, and the
  grid records which is which. Overwriting either half blanks the other, so the
  buffer can never contain a half-glyph that would desynchronise the terminal's
  cursor from our grid. A wide glyph that would not fit in the remaining columns
  is drawn as a blank instead of being allowed to wrap.
* Combining marks have no advance of their own and cannot be represented in a
  one-code-point-per-cell model, so they are dropped rather than mis-placed.
* `pushClip()`/`popClip()` restrict drawing to a rectangle, which is what makes
  horizontal scrolling trivial: the document view draws at a negative x and the
  clip discards whatever falls outside.
* Blank lines are *not* trimmed from the middle of the grid, because positions
  matter; only trailing blank rows are dropped from the debug/`--print` text
  renderings.

---

## 5. The Markdown pipeline

```text
text ──▶ parseMarkdown ──▶ Document ──▶ renderMarkdown ──▶ RenderedDocument
        (block + inline)   (model)      (width-aware)      (styled lines)
                                                                    │
                                                                    ▼
                                                            MarkdownView
                                                            (blit + clip)
```

**Parser** (`src/markdown/parser.cpp`). A line-based block parser — ATX and
Setext headings, paragraphs with soft/hard breaks, fenced and indented code
blocks, block quotes (recursively parsed), unordered/ordered/nested lists with
task items, GFM tables, thematic breaks — feeding a recursive-descent inline
parser for emphasis, strong, strikethrough, code spans, links, autolinks, images
and escapes. Emphasis follows flanking rules, so `snake_case_name` is not
italicised, and `3 * 4 * 5` is not emphasis. Reference-style links are treated as
literal text; raw HTML is passed through as text.

**Renderer** (`src/markdown/renderer.cpp`) is where *everything width-dependent*
happens. It knows the available columns, so it wraps prose, boxes code blocks,
and lays out tables. Its output is a list of `RenderedLine`s, each a list of
styled `Span`s. The widget that displays them does no layout at all — which is
why handling a terminal resize is just "re-render at the new width and blit
again".

Two details in the wrapping algorithm earn their keep:

* Wrapping measures **display columns**, not bytes or code points. A CJK glyph is
  two columns, a combining accent is zero. Without this, every table containing
  East Asian text shears.
* A word that does not fit is **moved to the next line** when it would fit there,
  and only split when it is wider than a whole line by itself. Wrapping that
  always fills the current line produces the classic mid-word break
  (`...application on W` / `indows ...`).

**Highlighter** (`src/markdown/highlight.cpp`) is a single-pass tokenizer with a
small per-language rule table (comments, strings, numbers, keywords, types,
constants, call-like identifiers). It is deliberately not a parser: a few
hundred lines replace a heavyweight third-party dependency that would also have
to build cleanly under MSVC, GCC and Clang. Tokens tile the input exactly, and an
unknown language falls back to the C-like rules, which still finds comments,
strings and numbers.

---

## 6. Filesystem

Everything goes through `<filesystem>` and `std::filesystem::path`:

```cpp
auto child = parent / filename;          // correct on every platform
std::filesystem::directory_iterator ...  // no FindFirstFile
```

There is no `CreateFile`, no `FindFirstFile`, no `GetFileAttributes`, and no
manual `"\\"` or `"/"` concatenation anywhere in the application. Paths are
compared and displayed through `std::filesystem::path`, and `displayPath()`
produces a root-relative form for the header using `std::filesystem::relative`
rather than string surgery.

`FileTree` loads children **lazily**: expanding a directory reads only that
directory, so opening a large repository does not walk the disk. Children are
sorted directories-first and case-insensitively. `TreeNode::depth` bounds
nesting, which is what stops a symlink cycle from becoming an infinite walk. On
`refresh()` the set of expanded directories is captured first and re-applied
afterwards, so the user's view survives a reload.

One subtlety worth recording, because it was a real bug: `directory_iterator`'s
`operator*` returns a reference that `increment()` invalidates. The loop copies
the `directory_entry` *before* advancing.

---

## 7. File watching

```cpp
class FileWatcher {
public:
    virtual void watch(const std::filesystem::path& path) = 0;
    virtual void unwatch(const std::filesystem::path& path) = 0;
    virtual bool changed() = 0;
    virtual std::vector<std::filesystem::path> takeChanged() = 0;
};
```

The application depends on this interface and never on `ReadDirectoryChangesW`
or `inotify`. `createFileWatcher()` is implemented per platform, and currently
every platform returns the portable `PollingFileWatcher`, which compares
last-write-time and size on a throttled interval (400 ms by default).

That is a deliberate choice rather than an unfinished one. Polling uses nothing
but `<filesystem>`, so it behaves identically under all three toolchains and
cannot miss an event by mis-using a platform API; the cost is up to one interval
of latency, which is imperceptible for "my editor saved the file". The seam is in
place so that a native backend — `ReadDirectoryChangesW` with overlapped I/O on
Windows, `inotify` on Linux, `kqueue` on macOS — can be dropped in behind
`createFileWatcher()` without touching a single call site. That is the point of
the abstraction, and it is why the abstraction exists before the native backends
do.

---

## 8. Layout and resize

`computeLayout(screen, options)` is a pure function from a rectangle to a
`Layout { header, body, tree, divider, content, status }`. The header and status
bar are always present; the tree pane appears only when the body is wide enough
to be useful (≥ 44 columns), and the content pane takes whatever is left.

Because it is pure, a resize is just: compare `Terminal::size()` with the last
known size, resize the `Screen`, re-render the document at the new width, and
draw the next frame. There is no incremental layout state to invalidate and no
widget that holds a stale rectangle. The same function is unit-tested directly.

---

## 9. Testing strategy

The suite has no third-party dependency: a header-only harness (`tests/test_framework.hpp`)
with `MDVIEW_TEST`, `CHECK`, `CHECK_EQ`, `CHECK_CONTAINS` and `REQUIRE`, plus
CMake's `add_test`. It covers, in order of increasing integration:

* UTF-8 decoding (including malformed, overlong, surrogate and truncated input),
  display width, truncation and padding;
* the ANSI input decoder: sequences, modifiers, control bytes, lone `ESC`,
  mouse reports, UTF-8 split across reads;
* colour degradation and SGR generation;
* the screen grid: wide glyphs and their partners, clipping, box drawing, and the
  diff flush (asserting that an unchanged frame writes **nothing**);
* the parser and renderer for every supported construct, plus the highlighter;
* file IO, the tree model, and the polling watcher against a real temporary
  directory;
* CLI parsing, layout geometry;
* a headless end-to-end pass that constructs the real `Application` on a
  `HeadlessTerminal`, feeds it scripted `KeyEvent`s, and asserts on the rendered
  screen — opening a README, navigating the tree, scrolling, jumping between
  headings, toggling wrap/hidden/tree, the help overlay, resize, and quitting.

That last group is the payoff for the `Terminal` abstraction: the interactive
application is tested without a terminal, without timing, and without a
pseudo-console.

CTest additionally runs the real executable for `--version`, `--help` and
`--print`, so the CLI contract is covered end to end.

---

## 10. Adding a platform

1. Add a directory under `src/platform/` with `platform_<os>.cpp` implementing
   `platform::name()`, `stdoutIsTerminal()`, `stdinIsTerminal()` and
   `filesystem::createFileWatcher()`.
2. Add a `Terminal` implementation. If the platform has `termios` and `poll`,
   reuse `src/platform/posix/terminal_posix.cpp` and skip this step.
3. Add the two `target_sources()` blocks to `CMakeLists.txt`.
4. Nothing else. No file above `src/platform/` should need to change — and if one
   does, that is a bug in the abstraction, not in the port.

---

## 11. Deliberate trade-offs

| Decision | Why |
| --- | --- |
| Full redraw every frame + diff flush | Removes all cross-frame state and every "invalidate" protocol from the widgets. |
| Polling file watcher by default | Identical, testable behaviour on all three toolchains; a native backend drops in behind the same interface. |
| Hand-written Markdown subset | No third-party dependency to build under MSVC, GCC and Clang; covers the constructs a terminal viewer actually benefits from. |
| Hand-written syntax highlighter | Same reason; a tokenizer is enough to make code blocks readable. |
| One shared POSIX terminal | Linux and macOS expose the same `termios`/`poll`/`SIGWINCH` API; two identical files would be duplication without benefit. |
| Combining marks dropped | A cell holds one code point; placing a combining mark correctly would require per-cell grapheme storage for very little gain in a Markdown viewer. |
| Renderer clamps its width to 20 columns | Prevents degenerate layouts and division-by-zero-adjacent edge cases at absurd widths. |
| `--print` mode | Makes the parser and renderer verifiable from a script, and gives a sane answer when there is no TTY instead of half-drawing a screen into a pipe. |
