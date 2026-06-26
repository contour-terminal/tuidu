// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include <tuidu/Keymap.hpp>

namespace tuidu
{

Keymap::Keymap()
{
    install(kDefaultKeymap);
}

Keymap::Keymap(std::span<KeyBindingDef const> defs)
{
    install(defs);
}

void Keymap::install(std::span<KeyBindingDef const> defs)
{
    _bindings.clear();
    _help.clear();
    _bindings.reserve(defs.size());
    for (auto const& def: defs)
    {
        if (auto const chord = tui::KeyChord::parse(def.chord); chord.has_value())
            _bindings.emplace_back(*chord, def.action);
        if (!def.help.empty())
            _help.push_back(HelpEntry { .key = def.chord, .help = def.help });
    }
}

bool Keymap::bind(std::string_view chordSpec, Action action)
{
    auto const chord = tui::KeyChord::parse(chordSpec);
    if (!chord.has_value())
        return false;

    // Replace an existing binding for the same chord, else append.
    for (auto& [existing, boundAction]: _bindings)
    {
        if (existing.key == chord->key && existing.modifiers == chord->modifiers
            && existing.codepoint == chord->codepoint)
        {
            boundAction = action;
            return true;
        }
    }
    _bindings.emplace_back(*chord, action);
    return true;
}

Action Keymap::lookup(tui::KeyEvent const& event) const noexcept
{
    for (auto const& [chord, action]: _bindings)
        if (chord.matches(event))
            return action;
    return Action::None;
}

std::vector<HelpEntry> Keymap::helpEntries() const
{
    return _help;
}

} // namespace tuidu
