// SPDX-License-Identifier: Apache-2.0
#include <tui/Box.hpp>
#include <tui/Canvas.hpp>
#include <tui/Theme.hpp>

#include <algorithm>
#include <cmath>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include <tuidu/DeleteProgressDialog.hpp>

namespace tuidu
{

namespace
{
    constexpr std::string_view Title = " Deleting ";
    constexpr std::string_view FilledCell = "█"; ///< █ — one filled bar cell.
    constexpr std::string_view EmptyCell = "░";  ///< ░ — one empty bar cell.

    /// Truncates @p text to @p maxCols columns, appending an ellipsis when it is too long.
    [[nodiscard]] std::string elide(std::string_view text, std::size_t maxCols)
    {
        if (text.size() <= maxCols)
            return std::string { text };
        if (maxCols <= 1)
            return std::string(maxCols, '.');
        return std::string { text.substr(0, maxCols - 1) } + "…"; // …
    }
} // namespace

DeleteProgressDialog::DeleteProgressDialog() = default;

void DeleteProgressDialog::setTarget(std::string target)
{
    _target = std::move(target);
}

void DeleteProgressDialog::setProgress(float fraction)
{
    _progress = std::clamp(fraction, 0.0f, 1.0f);
}

void DeleteProgressDialog::setStatus(std::string status)
{
    _status = std::move(status);
}

tui::Size DeleteProgressDialog::preferredSize() const
{
    return tui::Size { .width = Width, .height = Height };
}

void DeleteProgressDialog::render(tui::Canvas& canvas)
{
    auto const& theme = canvas.theme();
    auto const boxStyle = theme.dialogBorder;
    auto const nameStyle = theme.textAccent;
    auto const barStyle = theme.textAccent;
    auto const emptyStyle = theme.textMuted;
    auto const statusStyle = theme.textNormal;
    auto const hintStyle = theme.textMuted;

    auto const area = tui::Rect { .x = 0, .y = 0, .width = Width, .height = Height };
    canvas.clear(theme.dialogBackground);
    canvas.drawBox(area, tui::BorderStyle::Rounded, boxStyle, Title, tui::TitleAlign::Left);

    auto const innerWidth = Width - (2 * Padding);

    // Line 1: the target being deleted.
    canvas.putString(1, Padding, elide(_target, static_cast<std::size_t>(innerWidth)), nameStyle);

    // Line 2: the progress bar, split into filled and empty cells.
    auto const barWidth = innerWidth;
    auto const filled =
        std::clamp(static_cast<int>(std::lround(_progress * static_cast<float>(barWidth))), 0, barWidth);
    auto filledBar = std::string {};
    for ([[maybe_unused]] auto const cell: std::views::iota(0, filled))
        filledBar += FilledCell;
    auto emptyBar = std::string {};
    for ([[maybe_unused]] auto const cell: std::views::iota(filled, barWidth))
        emptyBar += EmptyCell;
    auto const consumed = canvas.putString(2, Padding, filledBar, barStyle);
    canvas.putString(2, Padding + consumed, emptyBar, emptyStyle);

    // Line 3: the status counter (e.g. "Deleting 12 / 40").
    canvas.putString(3, Padding, elide(_status, static_cast<std::size_t>(innerWidth)), statusStyle);

    // Footer hint.
    canvas.putString(4, Padding, "Esc to cancel", hintStyle);
}

tui::EventResult DeleteProgressDialog::onEvent(tui::InputEvent const& event)
{
    // Swallow key presses so they never reach the browser beneath; the App owns cancellation
    // (it interprets Esc directly while a delete is in flight).
    if (std::holds_alternative<tui::KeyEvent>(event))
        return tui::EventResult::Handled;
    return tui::EventResult::Ignored;
}

} // namespace tuidu
