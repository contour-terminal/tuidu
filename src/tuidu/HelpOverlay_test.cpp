// SPDX-License-Identifier: Apache-2.0
#include <tui/InputEvent.hpp>
#include <tui/KeyCode.hpp>
#include <tui/Modifier.hpp>

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
