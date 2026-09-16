# Advanced Markdown Features

This file demonstrates all the advanced Markdown constructs that mdview supports.

## Task Lists

- [x] Implement Markdown parser
- [x] Add syntax highlighting
- [x] Build file tree navigation
- [ ] Add search functionality
- [ ] Implement bookmarks
- [ ] Mouse support

### Sub-tasks

- [x] Parser
  - [x] Headings
  - [x] Lists
  - [x] Code blocks
  - [x] Tables
- [ ] Editor mode
  - [ ] Split view
  - [ ] Syntax tree

## Blockquotes

> Simple blockquote text.

> Nested blockquotes:
>
> > First level
> >
> > > Second level
> > >
> > > > Third level

> **Bold text** in a blockquote.
>
> - List item 1
> - List item 2
>
> `inline code` also works.

## Emphasis Variants

This is *italic*, this is **bold**, and this is ***bold italic***.

This is ~~strikethrough~~ text.

Inline `code` looks like this.

## Links and Autolinks

Visit [GitHub](https://github.com) for more.

Autolink: https://example.com

## Thematic Breaks

Content above.

---

Content below.

***

Another separator.

___

## Nested Lists

1. First item
   1. Sub-item A
   2. Sub-item B
      - Deep item
      - Another deep item
2. Second item
3. Third item

## Complex Table

| Feature | Parser | Renderer | Highlighter | Total |
| --- | ---: | ---: | ---: | ---: |
| Lines of code | 1,247 | 892 | 456 | 2,595 |
| Test coverage | 94% | 91% | 96% | 93% |
| Bugs found | 3 | 1 | 2 | 6 |
| Status | Done | Done | Done | Ready |

## Mixed Content

Here is a paragraph with **bold**, *italic*, and `code`.

> A blockquote with a list:
> - Item one
> - Item two
>
> And a code block inside:

```python
print("inside a blockquote")
```

And the paragraph continues after the blockquote.

---

*All constructs rendered in a single document.*
