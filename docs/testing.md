# Testing

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
