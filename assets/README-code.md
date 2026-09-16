# Code Highlighting Showcase

mdview includes a built-in syntax highlighter for several languages.

## C++ Example

```cpp
#include <iostream>
#include <vector>
#include <algorithm>

int main() {
    std::vector<int> numbers = {5, 3, 8, 1, 9, 2};

    std::ranges::sort(numbers);

    for (const auto& n : numbers) {
        std::cout << n << " ";
    }
    // Output: 1 2 3 5 8 9
    return 0;
}
```

## Python Example

```python
from dataclasses import dataclass
from typing import Optional

@dataclass
class Config:
    name: str
    width: int = 80
    color: bool = True
    theme: Optional[str] = None

def load_config(path: str) -> Config:
    """Load configuration from a file."""
    with open(path) as f:
        data = f.read()
    return Config(name="default")

config = load_config("settings.toml")
print(f"Loaded: {config.name}")
```

## JavaScript Example

```javascript
async function fetchDocs(url) {
    try {
        const response = await fetch(url);
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }
        const data = await response.json();
        return data.map(item => ({
            ...item,
            timestamp: Date.now()
        }));
    } catch (err) {
        console.error("Failed:", err.message);
        return [];
    }
}
```

## Shell Script

```bash
#!/bin/bash
set -euo pipefail

BUILD_DIR="build"
CONFIG="Release"

echo "Building mdview..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build "$BUILD_DIR" --config "$CONFIG"

echo "Running tests..."
ctest --test-dir "$BUILD_DIR" -C "$CONFIG" --output-on-failure

echo "Done!"
```

## JSON Config

```json
{
    "name": "mdview",
    "version": "0.1.0",
    "features": {
        "syntaxHighlight": true,
        "liveReload": true,
        "wordWrap": true
    },
    "themes": ["dark", "light", "mono"]
}
```

## YAML Config

```yaml
server:
  host: localhost
  port: 8080
  ssl: true

logging:
  level: info
  format: json
  outputs:
    - stdout
    - file: /var/log/app.log

database:
  driver: postgres
  pool_size: 10
```

---

*Each language gets comments, strings, numbers, and keywords highlighted.*
