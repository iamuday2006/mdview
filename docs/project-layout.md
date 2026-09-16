# Project Layout

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
