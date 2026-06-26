// SPDX-License-Identifier: Apache-2.0
#include <tui/Box.hpp>
#include <tui/Canvas.hpp>
#include <tui/Theme.hpp>

#include <algorithm>
#include <string>
#include <string_view>

#include <tuidu/HelpOverlay.hpp>

namespace tuidu
{

namespace
{
    constexpr std::string_view kTitle = " Help — keys ";
    constexpr int kKeyColumn = 12; ///< Width reserved for the key column.
    constexpr int kPadding = 2;    ///< Horizontal padding inside the border.
} // namespace

HelpOverlay::HelpOverlay(Keymap const& keymap): _entries(keymap.helpEntries())
{
    // Width = key column + longest description, bounded by a sane minimum (the title).
    std::size_t longestDesc = 0;
    for (auto const& e: _entries)
        longestDesc = std::max(longestDesc, e.help.size());
    auto const content = kKeyColumn + static_cast<int>(longestDesc);
    _width = std::max(content, static_cast<int>(kTitle.size())) + (2 * kPadding);
}

tui::Size HelpOverlay::preferredSize() const
{
    // Rows = entries + top/bottom border; the footer hint adds one more line.
    auto const height = static_cast<int>(_entries.size()) + 3;
    return tui::Size { .width = _width + 2, .height = height + 1 };
}

void HelpOverlay::render(tui::Canvas& canvas)
{
    auto const& theme = canvas.theme();
    auto const boxStyle = theme.dialogBorder;
    auto const keyStyle = theme.textAccent;
    auto const descStyle = theme.textNormal;
    auto const hintStyle = theme.textMuted;

    auto const height = static_cast<int>(_entries.size()) + 3;
    auto const area = tui::Rect { .x = 0, .y = 0, .width = _width + 1, .height = height };

    // Background + border with a title.
    canvas.clear(theme.dialogBackground);
    canvas.drawBox(area, tui::BorderStyle::Rounded, boxStyle, kTitle, tui::TitleAlign::Left);

    // One row per binding: "key   description".
    auto row = 1;
    for (auto const& e: _entries)
    {
        canvas.putString(row, kPadding, std::string { e.key }, keyStyle);
        canvas.putString(row, kPadding + kKeyColumn, std::string { e.help }, descStyle);
        ++row;
    }

    // Footer hint.
    canvas.putString(row, kPadding, "press any key to close", hintStyle);
}

tui::EventResult HelpOverlay::onEvent(tui::InputEvent const& event)
{
    // Any key press closes the overlay; the host hides it on Handled.
    if (std::holds_alternative<tui::KeyEvent>(event))
        return tui::EventResult::Handled;
    return tui::EventResult::Ignored;
}

} // namespace tuidu
