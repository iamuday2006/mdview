# Welcome to mdview

A **terminal Markdown workspace** for reading documentation without leaving the command line.

## Getting Started

First, install the dependencies:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Then run the viewer:

```text
mdview .
```

## Features

- **File tree** with lazy expansion
- **Rendered Markdown** with proper formatting
- **Syntax highlighting** for popular languages
- **Live reload** when files change on disk
- **Resize-aware** layout that reflows automatically

### Why Terminal?

Because sometimes you just need to read a README without opening a browser.

> The best tool is the one that stays out of your way.

---

*Built with C++20 and zero dependencies.*
