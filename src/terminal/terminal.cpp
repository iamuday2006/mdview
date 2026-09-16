#include "terminal/terminal.hpp"

#include "utils/unicode.hpp"

namespace mdview::terminal {

Terminal::~Terminal() = default;

HeadlessTerminal::HeadlessTerminal(TerminalSize size) : size_(size) {
    capabilities_.ansi = true;
    capabilities_.unicode = true;
    capabilities_.cursorAddressing = true;
    capabilities_.alternateScreen = true;
    capabilities_.colorMode = ColorMode::Ansi256;
}

void HeadlessTerminal::initialize() {}
void HeadlessTerminal::shutdown() {}

void HeadlessTerminal::clear() {
    output_.append("\x1b[2J");
}

void HeadlessTerminal::moveCursor(int x, int y) {
    cursorMoves_.emplace_back(x, y);
    output_.append("\x1b[");
    output_.append(std::to_string(y + 1));
    output_.push_back(';');
    output_.append(std::to_string(x + 1));
    output_.push_back('H');
}

void HeadlessTerminal::showCursor(bool visible) {
    output_.append(visible ? "\x1b[?25h" : "\x1b[?25l");
}

KeyEvent HeadlessTerminal::readKey() {
    if (input_.empty()) return KeyEvent{};
    const KeyEvent event = input_.front();
    input_.pop_front();
    return event;
}

bool HeadlessTerminal::hasPendingInput() { return !input_.empty(); }

void HeadlessTerminal::write(std::string_view bytes) { output_.append(bytes); }

void HeadlessTerminal::flush() {}

TerminalCapabilities HeadlessTerminal::capabilities() const { return capabilities_; }

void HeadlessTerminal::pushInput(const KeyEvent& event) { input_.push_back(event); }

void HeadlessTerminal::pushInput(std::initializer_list<KeyEvent> events) {
    for (const KeyEvent& event : events) input_.push_back(event);
}

void HeadlessTerminal::pushText(std::string_view text) {
    std::size_t i = 0;
    while (i < text.size()) {
        char32_t codePoint = 0;
        const std::size_t consumed = uni::decode(text, i, codePoint);
        if (consumed == 0) break;
        input_.push_back(characterEvent(codePoint));
        i += consumed;
    }
}

void HeadlessTerminal::pushBytes(std::string_view bytes) {
    InputDecoder decoder;
    for (const KeyEvent& event : decoder.feed(bytes)) input_.push_back(event);
    for (const KeyEvent& event : decoder.flush()) input_.push_back(event);
}

}  // namespace mdview::terminal
