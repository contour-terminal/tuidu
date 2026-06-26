// SPDX-License-Identifier: Apache-2.0
#include <tui/InputEvent.hpp>
#include <tui/KeyCode.hpp>
#include <tui/Modifier.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>

#include <tuidu/Keymap.hpp>

using namespace tuidu;

namespace
{
/// Builds a KeyEvent for a printable character.
[[nodiscard]] tui::KeyEvent charKey(char c, tui::Modifier mod = tui::Modifier::None)
{
    return tui::KeyEvent { .key = {}, .modifiers = mod, .codepoint = static_cast<char32_t>(c) };
}

/// Builds a KeyEvent for a special key.
[[nodiscard]] tui::KeyEvent specialKey(tui::KeyCode code, tui::Modifier mod = tui::Modifier::None)
{
    return tui::KeyEvent { .key = code, .modifiers = mod, .codepoint = 0 };
}
} // namespace

TEST_CASE("Keymap: every default binding dispatches to its action", "[keymap][tables]")
{
    // Table-completeness: each row's chord must resolve to its declared action.
    Keymap const map;
    for (auto const& def: kDefaultKeymap)
    {
        auto const chord = tui::KeyChord::parse(def.chord);
        REQUIRE(chord.has_value());
        // Reconstruct a KeyEvent from the parsed chord and look it up.
        auto const event =
            tui::KeyEvent { .key = chord->key, .modifiers = chord->modifiers, .codepoint = chord->codepoint };
        INFO("chord: " << def.chord);
        CHECK(map.lookup(event) == def.action);
    }
}

TEST_CASE("Keymap: vim motion keys", "[keymap]")
{
    Keymap const map;
    CHECK(map.lookup(charKey('j')) == Action::MoveDown);
    CHECK(map.lookup(charKey('k')) == Action::MoveUp);
    CHECK(map.lookup(charKey('g')) == Action::MoveTop);
    // A shifted letter arrives as the base (lowercase) codepoint plus the Shift modifier.
    CHECK(map.lookup(charKey('g', tui::Modifier::Shift)) == Action::MoveBottom);
    CHECK(map.lookup(charKey('l')) == Action::Descend);
    CHECK(map.lookup(charKey('h')) == Action::Ascend);
    CHECK(map.lookup(charKey('q')) == Action::Quit);
}

TEST_CASE("Keymap: Ctrl-D / Ctrl-U half-page motions", "[keymap]")
{
    Keymap const map;
    // A Ctrl+letter event arrives as the lowercase codepoint plus the Ctrl modifier.
    CHECK(map.lookup(charKey('d', tui::Modifier::Ctrl)) == Action::HalfPageDown);
    CHECK(map.lookup(charKey('u', tui::Modifier::Ctrl)) == Action::HalfPageUp);
    // Without Ctrl they are not half-page motions.
    CHECK(map.lookup(charKey('d')) == Action::None);
    CHECK(map.lookup(charKey('u')) == Action::None);
}

TEST_CASE("Keymap: G jumps to bottom", "[keymap]")
{
    Keymap const map;
    CHECK(map.lookup(charKey('g', tui::Modifier::Shift)) == Action::MoveBottom);
}

TEST_CASE("Keymap: arrow-key aliases mirror j/k", "[keymap]")
{
    Keymap const map;
    CHECK(map.lookup(specialKey(tui::KeyCode::Down)) == Action::MoveDown);
    CHECK(map.lookup(specialKey(tui::KeyCode::Up)) == Action::MoveUp);
    CHECK(map.lookup(specialKey(tui::KeyCode::Enter)) == Action::Descend);
    CHECK(map.lookup(specialKey(tui::KeyCode::Backspace)) == Action::Ascend);
}

TEST_CASE("Keymap: unbound key returns None", "[keymap]")
{
    Keymap const map;
    CHECK(map.lookup(charKey('z')) == Action::None);
}

TEST_CASE("Keymap: help excludes aliases (empty-help rows)", "[keymap]")
{
    Keymap const map;
    auto const help = map.helpEntries();

    // Aliases (down/up/enter/backspace/escape) must not appear.
    for (auto const& e: help)
    {
        CHECK_FALSE(e.help.empty());
        CHECK(e.key != "down");
        CHECK(e.key != "enter");
        CHECK(e.key != "backspace");
    }
    // A representative real binding is present.
    auto const hasQuit = std::ranges::any_of(help, [](HelpEntry const& e) { return e.key == "q"; });
    CHECK(hasQuit);
}

TEST_CASE("Keymap: bind overrides an existing chord", "[keymap]")
{
    Keymap map;
    REQUIRE(map.lookup(charKey('q')) == Action::Quit);
    REQUIRE(map.bind("q", Action::Help));
    CHECK(map.lookup(charKey('q')) == Action::Help);
}

TEST_CASE("Keymap: bind adds a new chord", "[keymap]")
{
    Keymap map;
    REQUIRE(map.lookup(charKey('x')) == Action::None);
    REQUIRE(map.bind("x", Action::Rescan));
    CHECK(map.lookup(charKey('x')) == Action::Rescan);
}

TEST_CASE("Keymap: bind rejects an unparseable chord", "[keymap]")
{
    Keymap map;
    CHECK_FALSE(map.bind("", Action::Quit));
}

TEST_CASE("Keymap: custom table is honored", "[keymap]")
{
    constexpr std::array<KeyBindingDef, 1> custom { {
        { .chord = "x", .action = Action::Rescan, .help = "Rescan now" },
    } };
    Keymap const map { custom };
    CHECK(map.lookup(charKey('x')) == Action::Rescan);
    CHECK(map.lookup(charKey('j')) == Action::None); // default bindings not present
    CHECK(map.helpEntries().size() == 1);
}
