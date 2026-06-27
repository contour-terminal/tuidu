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
    constexpr std::string_view kTitle = " Deleting ";
    constexpr std::string_view kFilledCell = "█"; ///< █ — one filled bar cell.
    constexpr std::string_view kEmptyCell = "░";  ///< ░ — one empty bar cell.

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
    return tui::Size { .width = kWidth, .height = kHeight };
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

    auto const area = tui::Rect { .x = 0, .y = 0, .width = kWidth, .height = kHeight };
    canvas.clear(theme.dialogBackground);
    canvas.drawBox(area, tui::BorderStyle::Rounded, boxStyle, kTitle, tui::TitleAlign::Left);

    auto const innerWidth = kWidth - (2 * kPadding);

    // Line 1: the target being deleted.
    canvas.putString(1, kPadding, elide(_target, static_cast<std::size_t>(innerWidth)), nameStyle);

    // Line 2: the progress bar, split into filled and empty cells.
    auto const barWidth = innerWidth;
    auto const filled =
        std::clamp(static_cast<int>(std::lround(_progress * static_cast<float>(barWidth))), 0, barWidth);
    auto filledBar = std::string {};
    for ([[maybe_unused]] auto const cell: std::views::iota(0, filled))
        filledBar += kFilledCell;
    auto emptyBar = std::string {};
    for ([[maybe_unused]] auto const cell: std::views::iota(filled, barWidth))
        emptyBar += kEmptyCell;
    auto const consumed = canvas.putString(2, kPadding, filledBar, barStyle);
    canvas.putString(2, kPadding + consumed, emptyBar, emptyStyle);

    // Line 3: the status counter (e.g. "Deleting 12 / 40").
    canvas.putString(3, kPadding, elide(_status, static_cast<std::size_t>(innerWidth)), statusStyle);

    // Footer hint.
    canvas.putString(4, kPadding, "Esc to cancel", hintStyle);
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
