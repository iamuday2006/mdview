# Unicode & International Text

mdview handles Unicode correctly, including wide characters and emoji.

## CJK Text Support

| Language | Example | Width |
| --- | --- | --- |
| Chinese | 你好世界 | 8 columns |
| Japanese | こんにちは | 10 columns |
| Korean | 안녕하세요 | 10 columns |

## Emoji

| Emoji | Name | Category |
| --- | --- | --- |
| :rocket: | Rocket | Deployment |
| :bug: | Bug | Issues |
| :white_check_mark: | Check | Done |
| :warning: | Warning | Alerts |
| :fire: | Fire | Critical |

## Special Characters

- Greek: α β γ δ ε ζ η θ
- Math: ∑ ∏ ∫ √ ∞ ≠ ≤ ≥
- Arrows: → ← ↑ ↓ ↔ ⇒ ⇔
- Currency: $ € £ ¥ ₹

## Mixed Width Table

| Name | Display Width | Characters | Status |
| --- | --- | --- | --- |
| ASCII | 10 | 10 | OK |
| emoji | 2 | 1 | OK |
| CJK | 4 | 2 | OK |
| mixed | 7 | 4 | OK |

## Code with Unicode

```python
# Python handles Unicode natively
 greeting = "你好世界"
 symbols = "→ ← ↑ ↓"
 print(f"{greeting} {symbols}")
```

```rust
// Rust too
fn main() {
    let greeting = "你好世界";
    let symbols = "→ ← ↑ ↓";
    println!("{greeting} {symbols}");
}
```

---

*Display-width-aware wrapping keeps tables aligned.*
