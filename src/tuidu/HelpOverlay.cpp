// SPDX-License-Identifier: Apache-2.0
#include <core/tui/Box.hpp>
#include <core/tui/Canvas.hpp>
#include <core/tui/Theme.hpp>

#include <algorithm>
#include <string>
#include <string_view>

#include <tuidu/HelpOverlay.hpp>

namespace tuidu
{

namespace
{
    constexpr std::string_view Title = " Help — keys ";
    constexpr int KeyColumn = 12; ///< Width reserved for the key column.
    constexpr int Padding = 2;    ///< Horizontal padding inside the border.
} // namespace

HelpOverlay::HelpOverlay(Keymap const& keymap, std::span<ChordSequenceDef const> sequences):
    _entries(keymap.helpEntries())
{
    // Append chord-sequence rows (e.g. "dd  Delete selected") after the single-key bindings, so
    // the same data tables that drive behavior also drive the help — no parallel list to maintain.
    for (auto const& seq: sequences)
        if (!seq.help.empty())
            _entries.push_back(HelpEntry { .key = seq.display, .help = seq.help });

    // Width = key column + longest description, bounded by a sane minimum (the title).
    std::size_t longestDesc = 0;
    for (auto const& e: _entries)
        longestDesc = std::max(longestDesc, e.help.size());
    auto const content = KeyColumn + static_cast<int>(longestDesc);
    _width = std::max(content, static_cast<int>(Title.size())) + (2 * Padding);
}

core::tui::Size HelpOverlay::preferredSize() const
{
    // Rows = entries + top/bottom border; the footer hint adds one more line.
    auto const height = static_cast<int>(_entries.size()) + 3;
    return core::tui::Size { .width = _width + 2, .height = height + 1 };
}

void HelpOverlay::render(core::tui::Canvas& canvas)
{
    auto const& theme = canvas.theme();
    auto const boxStyle = theme.dialogBorder;
    auto const keyStyle = theme.textAccent;
    auto const descStyle = theme.textNormal;
    auto const hintStyle = theme.textMuted;

    auto const height = static_cast<int>(_entries.size()) + 3;
    auto const area = core::tui::Rect { .x = 0, .y = 0, .width = _width + 1, .height = height };

    // Background + border with a title.
    canvas.clear(theme.dialogBackground);
    canvas.drawBox(area, core::tui::BorderStyle::Rounded, boxStyle, Title, core::tui::TitleAlign::Left);

    // One row per binding: "key   description".
    auto row = 1;
    for (auto const& e: _entries)
    {
        canvas.putString(row, Padding, std::string { e.key }, keyStyle);
        canvas.putString(row, Padding + KeyColumn, std::string { e.help }, descStyle);
        ++row;
    }

    // Footer hint.
    canvas.putString(row, Padding, "press any key to close", hintStyle);
}

core::tui::EventResult HelpOverlay::onEvent(core::tui::InputEvent const& event)
{
    // Any key press closes the overlay; the host hides it on Handled.
    if (std::holds_alternative<core::tui::KeyEvent>(event))
        return core::tui::EventResult::Handled;
    return core::tui::EventResult::Ignored;
}

} // namespace tuidu
