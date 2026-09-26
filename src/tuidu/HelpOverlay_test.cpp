// SPDX-License-Identifier: Apache-2.0
#include <core/tui/Buffer.hpp>
#include <core/tui/Canvas.hpp>
#include <core/tui/InputEvent.hpp>
#include <core/tui/KeyCode.hpp>
#include <core/tui/Modifier.hpp>
#include <core/tui/Rect.hpp>
#include <core/tui/TestHelpers.hpp>
#include <core/tui/Theme.hpp>

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

    auto const key = core::tui::InputEvent { core::tui::KeyEvent {
        .key = {}, .modifiers = core::tui::Modifier::None, .codepoint = U'x' } };
    CHECK(overlay.onEvent(key) == core::tui::EventResult::Handled);
}

TEST_CASE("HelpOverlay: non-key events are ignored", "[help]")
{
    Keymap const keymap;
    HelpOverlay overlay(keymap);

    auto const resize = core::tui::InputEvent { core::tui::ResizeEvent { .columns = 80, .rows = 24 } };
    CHECK(overlay.onEvent(resize) == core::tui::EventResult::Ignored);
}

TEST_CASE("HelpOverlay: lists the dd/yy chords from the sequence table", "[help]")
{
    Keymap const keymap;
    HelpOverlay overlay(keymap); // defaults to ChordSequences, which includes `dd` and `yy`

    auto const size = overlay.preferredSize();
    core::tui::Buffer buffer(size.height + 1, size.width + 2);
    buffer.clear();
    core::tui::Theme theme;
    core::tui::Canvas canvas(
        buffer,
        core::tui::Rect { .x = 0, .y = 0, .width = size.width + 2, .height = size.height + 1 },
        theme);
    overlay.render(canvas);

    auto const content = core::tui::test::canvasToString(buffer);
    CHECK(content.find("dd") != std::string::npos);
    CHECK(content.find("Delete selected") != std::string::npos);
    CHECK(content.find("yy") != std::string::npos);
    CHECK(content.find("Copy path to clipboard") != std::string::npos);
}
