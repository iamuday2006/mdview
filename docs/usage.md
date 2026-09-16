# Usage

```text
mdview [options] [path]
```

`path` is a Markdown file or a directory. A directory opens its tree and
 previews the best candidate document (`README.md` first, then `index.md`). With
no argument, the current directory is used.

## Options

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

## Print mode

`--print` is the same parser and renderer without the interactive shell, which
makes it useful for piping and for scripting:

```sh
mdview --print --width 100 README.md | less -R
```

Everything is decided per stream, so `mdview --print` writes plain text when
piped and coloured text when a terminal is attached.
