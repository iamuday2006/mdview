#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Normalised keyboard model.
//
// The UI only ever sees these values.  Whether the event came from a Win32
// KEY_EVENT_RECORD or from an ANSI escape sequence produced by termios is
// entirely hidden behind the Terminal implementations and this decoder.

namespace mdview::terminal {

enum class Key : std::uint8_t {
    None = 0,  ///< "nothing available yet" -- lets readKey() be non-blocking
    Up,
    Down,
    Left,
    Right,
    Enter,
    Escape,
    Tab,
    BackTab,
    Backspace,
    Delete,
    Insert,
    PageUp,
    PageDown,
    Home,
    End,
    Character,
    CtrlC,
    CtrlD,
    CtrlL,
    CtrlR,
    CtrlU,
    CtrlW,
    CtrlZ,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
};

enum Modifier : unsigned {
    ModNone = 0,
    ModShift = 1u << 0,
    ModAlt = 1u << 1,
    ModCtrl = 1u << 2,
};

struct KeyEvent {
    Key key = Key::None;
    /// Valid when key == Key::Character.
    char32_t codepoint = 0;
    unsigned modifiers = ModNone;

    bool valid() const { return key != Key::None; }
    bool isCharacter() const { return key == Key::Character; }
    bool hasCtrl() const { return (modifiers & ModCtrl) != 0; }
    bool hasAlt() const { return (modifiers & ModAlt) != 0; }
    bool hasShift() const { return (modifiers & ModShift) != 0; }

    /// The character as UTF-8, or an empty string for non-character keys.
    std::string text() const;

    /// True when the event is the character `c` (case-insensitive) without
    /// Ctrl or Alt, i.e. the usual way a TUI matches shortcuts.
    bool isPlainChar(char c) const;

    friend bool operator==(const KeyEvent&, const KeyEvent&) noexcept = default;
};

/// Human readable name, used by the help overlay and by tests.
std::string_view keyName(Key key);

/// Normalises a control byte (0x00..0x1F and 0x7F) the same way the ANSI
/// decoder does.  The Windows implementation reuses this so both platforms
/// agree on what Ctrl+<letter> means.
KeyEvent controlByteEvent(unsigned char byte);

/// Builds a plain character event.  `mods` defaults to no modifiers.
KeyEvent characterEvent(char32_t codePoint, unsigned mods = ModNone);

/// Builds a non-character event.
KeyEvent specialEvent(Key key, unsigned mods = ModNone);

/// Turns a byte stream into KeyEvents.
///
/// Bytes may arrive split across reads, so the decoder keeps whatever it could
/// not yet interpret in an internal buffer.  `flush()` forces the leftovers to
/// be decoded -- that is how a lone ESC becomes Key::Escape once the terminal
/// has stopped sending bytes.
class InputDecoder {
public:
    std::vector<KeyEvent> feed(std::string_view bytes);
    std::vector<KeyEvent> flush();

    bool pending() const { return !buffer_.empty(); }
    std::size_t pendingBytes() const { return buffer_.size(); }
    void reset();

private:
    std::string buffer_;
};

}  // namespace mdview::terminal
