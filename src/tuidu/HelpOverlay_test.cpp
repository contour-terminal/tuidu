// SPDX-License-Identifier: Apache-2.0
#include <tui/Buffer.hpp>
#include <tui/Canvas.hpp>
#include <tui/InputEvent.hpp>
#include <tui/KeyCode.hpp>
#include <tui/Modifier.hpp>
#include <tui/Rect.hpp>
#include <tui/TestHelpers.hpp>
#include <tui/Theme.hpp>

#include <catch2/catch_test_macros.hpp>

#include <tuidu/HelpOverlay.hpp>
#include <tuidu/Keymap.hpp>

using namespace tuidu;

TEST_CASE("HelpOverlay: sizes to fit the keymap's help entries", "[help]")
{
    Keymap const keymap;
    HelpOverlay overlay(keymap);

    auto const size = overlay.preferredSize();
    auto const entryCount = static_cast<int>(keymap.helpEntries().size());
    // One row per real (non-alias) binding plus borders/footer — must be non-trivial.
    CHECK(size.height > entryCount);
    CHECK(size.width > 10);
}

TEST_CASE("HelpOverlay: a key press is handled (so the host dismisses it)", "[help]")
{
    Keymap const keymap;
    HelpOverlay overlay(keymap);

    auto const key =
        tui::InputEvent { tui::KeyEvent { .key = {}, .modifiers = tui::Modifier::None, .codepoint = U'x' } };
    CHECK(overlay.onEvent(key) == tui::EventResult::Handled);
}

TEST_CASE("HelpOverlay: non-key events are ignored", "[help]")
{
    Keymap const keymap;
    HelpOverlay overlay(keymap);

    auto const resize = tui::InputEvent { tui::ResizeEvent { .columns = 80, .rows = 24 } };
    CHECK(overlay.onEvent(resize) == tui::EventResult::Ignored);
}

TEST_CASE("HelpOverlay: lists the dd/yy chords from the sequence table", "[help]")
{
    Keymap const keymap;
    HelpOverlay overlay(keymap); // defaults to kChordSequences, which includes `dd` and `yy`

    auto const size = overlay.preferredSize();
    tui::Buffer buffer(size.height + 1, size.width + 2);
    buffer.clear();
    tui::Theme theme;
    tui::Canvas canvas(
        buffer, tui::Rect { .x = 0, .y = 0, .width = size.width + 2, .height = size.height + 1 }, theme);
    overlay.render(canvas);

    auto const content = tui::test::canvasToString(buffer);
    CHECK(content.find("dd") != std::string::npos);
    CHECK(content.find("Delete selected") != std::string::npos);
    CHECK(content.find("yy") != std::string::npos);
    CHECK(content.find("Copy path to clipboard") != std::string::npos);
}
