#include "terminal/input.hpp"

#include "utils/unicode.hpp"

#include <algorithm>

namespace mdview::terminal {

namespace {

unsigned modifiersFromAnsi(int encoded) {
    // xterm encodes modifiers as 1 + bitmask(shift = 1, alt = 2, ctrl = 4).
    const int bits = encoded - 1;
    unsigned mods = ModNone;
    if (bits & 1) mods |= ModShift;
    if (bits & 2) mods |= ModAlt;
    if (bits & 4) mods |= ModCtrl;
    return mods;
}

Key keyFromLetter(char letter) {
    switch (letter) {
        case 'A':
            return Key::Up;
        case 'B':
            return Key::Down;
        case 'C':
            return Key::Right;
        case 'D':
            return Key::Left;
        case 'F':
            return Key::End;
        case 'H':
            return Key::Home;
        case 'P':
            return Key::F1;
        case 'Q':
            return Key::F2;
        case 'R':
            return Key::F3;
        case 'S':
            return Key::F4;
        case 'Z':
            return Key::BackTab;
        default:
            return Key::None;
    }
}

/// Maps the `~`-terminated numeric form (e.g. "5~" for PageUp).
Key keyFromTildeCode(int code) {
    switch (code) {
        case 1:
        case 7:
            return Key::Home;
        case 2:
            return Key::Insert;
        case 3:
            return Key::Delete;
        case 4:
        case 8:
            return Key::End;
        case 5:
            return Key::PageUp;
        case 6:
            return Key::PageDown;
        case 11:
            return Key::F1;
        case 12:
            return Key::F2;
        case 13:
            return Key::F3;
        case 14:
            return Key::F4;
        case 15:
            return Key::F5;
        case 17:
            return Key::F6;
        case 18:
            return Key::F7;
        case 19:
            return Key::F8;
        case 20:
            return Key::F9;
        case 21:
            return Key::F10;
        case 23:
            return Key::F11;
        case 24:
            return Key::F12;
        default:
            return Key::None;
    }
}

/// Number of continuation bytes a lead byte promises, or 0 for ASCII and for
/// bytes that can never start a sequence.
int utf8ContinuationCount(unsigned char lead) {
    if (lead < 0x80) return 0;
    if ((lead & 0xE0) == 0xC0) return 1;
    if ((lead & 0xF0) == 0xE0) return 2;
    if ((lead & 0xF8) == 0xF0) return 3;
    return -1;  // invalid lead byte
}

/// False when the UTF-8 sequence starting at `index` is still incomplete, in
/// which case the decoder must wait for the next read.
bool utf8Complete(std::string_view text, std::size_t index) {
    const int extra = utf8ContinuationCount(static_cast<unsigned char>(text[index]));
    if (extra <= 0) return true;  // ASCII or malformed: a single byte is enough
    return index + static_cast<std::size_t>(extra) < text.size();
}

void parseCsiParams(std::string_view params, int (&fields)[2], bool (&present)[2]) {
    fields[0] = fields[1] = 0;
    present[0] = present[1] = false;

    int fieldIndex = 0;
    int value = 0;
    bool any = false;
    for (std::size_t i = 0; i < params.size(); ++i) {
        const char ch = params[i];
        if (ch >= '0' && ch <= '9') {
            value = value * 10 + (ch - '0');
            any = true;
        } else if (ch == ';') {
            if (fieldIndex < 2) {
                fields[fieldIndex] = value;
                present[fieldIndex] = any;
            }
            ++fieldIndex;
            value = 0;
            any = false;
        }
    }
    if (fieldIndex < 2) {
        fields[fieldIndex] = value;
        present[fieldIndex] = any;
    }
}

}  // namespace

KeyEvent controlByteEvent(unsigned char byte) {
    switch (byte) {
        case 0x03:
            return specialEvent(Key::CtrlC, ModCtrl);
        case 0x04:
            return specialEvent(Key::CtrlD, ModCtrl);
        case 0x08:
            return specialEvent(Key::Backspace);
        case 0x09:
            return specialEvent(Key::Tab);
        case 0x0A:
        case 0x0D:
            return specialEvent(Key::Enter);
        case 0x0C:
            return specialEvent(Key::CtrlL, ModCtrl);
        case 0x12:
            return specialEvent(Key::CtrlR, ModCtrl);
        case 0x15:
            return specialEvent(Key::CtrlU, ModCtrl);
        case 0x17:
            return specialEvent(Key::CtrlW, ModCtrl);
        case 0x1A:
            return specialEvent(Key::CtrlZ, ModCtrl);
        case 0x1B:
            return specialEvent(Key::Escape);
        case 0x7F:
            return specialEvent(Key::Backspace);
        default:
            break;
    }
    // Any other control byte is Ctrl+<letter>.
    if (byte >= 0x01 && byte <= 0x1A) {
        return characterEvent(static_cast<char32_t>(U'a' + byte - 1), ModCtrl);
    }
    return specialEvent(Key::None);
}

std::string KeyEvent::text() const {
    if (key != Key::Character) return {};
    return uni::encodeUtf8(codepoint);
}

bool KeyEvent::isPlainChar(char c) const {
    // Case sensitive on purpose: 'g' and 'G' are different shortcuts.
    if (key != Key::Character) return false;
    if (hasCtrl() || hasAlt()) return false;
    return codepoint == static_cast<char32_t>(static_cast<unsigned char>(c));
}

std::string_view keyName(Key key) {
    switch (key) {
        case Key::None: return "None";
        case Key::Up: return "Up";
        case Key::Down: return "Down";
        case Key::Left: return "Left";
        case Key::Right: return "Right";
        case Key::Enter: return "Enter";
        case Key::Escape: return "Escape";
        case Key::Tab: return "Tab";
        case Key::BackTab: return "BackTab";
        case Key::Backspace: return "Backspace";
        case Key::Delete: return "Delete";
        case Key::Insert: return "Insert";
        case Key::PageUp: return "PageUp";
        case Key::PageDown: return "PageDown";
        case Key::Home: return "Home";
        case Key::End: return "End";
        case Key::Character: return "Character";
        case Key::CtrlC: return "CtrlC";
        case Key::CtrlD: return "CtrlD";
        case Key::CtrlL: return "CtrlL";
        case Key::CtrlR: return "CtrlR";
        case Key::CtrlU: return "CtrlU";
        case Key::CtrlW: return "CtrlW";
        case Key::CtrlZ: return "CtrlZ";
        case Key::F1: return "F1";
        case Key::F2: return "F2";
        case Key::F3: return "F3";
        case Key::F4: return "F4";
        case Key::F5: return "F5";
        case Key::F6: return "F6";
        case Key::F7: return "F7";
        case Key::F8: return "F8";
        case Key::F9: return "F9";
        case Key::F10: return "F10";
        case Key::F11: return "F11";
        case Key::F12: return "F12";
    }
    return "Unknown";
}

KeyEvent characterEvent(char32_t codePoint, unsigned mods) {
    KeyEvent event;
    event.key = Key::Character;
    event.codepoint = codePoint;
    event.modifiers = mods;
    return event;
}

KeyEvent specialEvent(Key key, unsigned mods) {
    KeyEvent event;
    event.key = key;
    event.modifiers = mods;
    return event;
}

void InputDecoder::reset() { buffer_.clear(); }

std::vector<KeyEvent> InputDecoder::feed(std::string_view bytes) {
    buffer_.append(bytes);

    std::vector<KeyEvent> events;
    std::size_t i = 0;

    while (i < buffer_.size()) {
        const unsigned char byte = static_cast<unsigned char>(buffer_[i]);

        if (byte == 0x1B) {
            if (i + 1 >= buffer_.size()) break;  // lone ESC: need more input

            const unsigned char next = static_cast<unsigned char>(buffer_[i + 1]);

            if (next == '[') {
                std::size_t cursor = i + 2;
                const std::size_t paramsStart = cursor;
                while (cursor < buffer_.size()) {
                    const unsigned char c = static_cast<unsigned char>(buffer_[cursor]);
                    if (c >= 0x40 && c <= 0x7E) break;  // final byte
                    ++cursor;
                }
                if (cursor >= buffer_.size()) break;  // incomplete CSI

                const unsigned char finalByte = static_cast<unsigned char>(buffer_[cursor]);
                const std::string_view params(buffer_.data() + paramsStart, cursor - paramsStart);
                ++cursor;

                // Private modes (mouse reports, focus in/out, ...) are consumed
                // but never turned into keystrokes.
                if (!params.empty() && (params.front() == '<' || params.front() == '?' ||
                                        params.front() == '=' || params.front() == '>')) {
                    i = cursor;
                    continue;
                }

                int fields[2] = {0, 0};
                bool present[2] = {false, false};
                parseCsiParams(params, fields, present);

                const unsigned mods = present[1] ? modifiersFromAnsi(fields[1]) : ModNone;
                const Key key = (finalByte == '~') ? keyFromTildeCode(present[0] ? fields[0] : 0)
                                                    : keyFromLetter(static_cast<char>(finalByte));
                if (key != Key::None) events.push_back(specialEvent(key, mods));
                i = cursor;
                continue;
            }

            if (next == 'O') {
                if (i + 2 >= buffer_.size()) break;  // incomplete SS3
                const Key key = keyFromLetter(buffer_[i + 2]);
                if (key != Key::None) events.push_back(specialEvent(key));
                i += 3;
                continue;
            }

            if (next == 0x1B) {
                // ESC ESC: report the first Escape and re-examine the second.
                events.push_back(specialEvent(Key::Escape));
                ++i;
                continue;
            }

            // ESC followed by a normal byte means Alt+<key>.
            if (next < 0x20 || next == 0x7F) {
                KeyEvent inner = controlByteEvent(next);
                if (inner.key != Key::None) {
                    inner.modifiers |= ModAlt;
                    events.push_back(inner);
                }
                i += 2;
                continue;
            }

            if (!utf8Complete(buffer_, i + 1)) break;  // incomplete Alt+<char>
            char32_t codePoint = 0;
            const std::size_t consumed = uni::decode(buffer_, i + 1, codePoint);
            events.push_back(characterEvent(codePoint, ModAlt));
            i += 1 + consumed;
            continue;
        }

        if (byte < 0x20 || byte == 0x7F) {
            KeyEvent event = controlByteEvent(byte);
            if (event.key != Key::None) events.push_back(event);
            ++i;
            continue;
        }

        if (!utf8Complete(buffer_, i)) break;  // incomplete UTF-8 code point
        char32_t codePoint = 0;
        const std::size_t consumed = uni::decode(buffer_, i, codePoint);
        events.push_back(characterEvent(codePoint));
        i += consumed;
    }

    buffer_.erase(0, std::min(i, buffer_.size()));
    return events;
}

std::vector<KeyEvent> InputDecoder::flush() {
    std::vector<KeyEvent> events;
    std::size_t i = 0;
    while (i < buffer_.size()) {
        const unsigned char byte = static_cast<unsigned char>(buffer_[i]);
        if (byte == 0x1B) {
            // A trailing ESC with nothing after it is a plain Escape key.
            events.push_back(specialEvent(Key::Escape));
            ++i;
            continue;
        }
        if (byte < 0x20 || byte == 0x7F) {
            KeyEvent event = controlByteEvent(byte);
            if (event.key != Key::None) events.push_back(event);
            ++i;
            continue;
        }
        char32_t codePoint = 0;
        const std::size_t consumed = uni::decode(buffer_, i, codePoint);
        events.push_back(characterEvent(codePoint));
        i += consumed;
    }
    buffer_.clear();
    return events;
}

}  // namespace mdview::terminal
