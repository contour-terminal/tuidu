// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file DeleteProgressDialog.hpp
/// @brief A centered modal overlay showing the progress of a running delete.

#include <tui/Component.hpp>
#include <tui/InputEvent.hpp>

#include <string>

namespace tuidu
{

/// A centered, bordered overlay that reports the progress of a recursive delete: the target name,
/// a progress bar, an "n / total" counter, and a hint that Esc cancels.
///
/// It is purely presentational — the @ref App owns the @ref DeleteWorker, drives @ref setProgress
/// / @ref setStatus from drained @ref DeleteProgress messages, and interprets the Esc key. The
/// dialog swallows key events (so they never leak to the browser beneath) and never resolves an
/// action itself.
class DeleteProgressDialog: public tui::Component
{
  public:
    DeleteProgressDialog();

    /// Sets the name/path of the item being deleted (shown on the first line).
    /// @param target The display string for the target.
    void setTarget(std::string target);

    /// Sets the completion fraction of the progress bar.
    /// @param fraction Progress in [0, 1]; clamped into range.
    void setProgress(float fraction);

    /// Sets the status line (e.g. "Deleting 12 / 40").
    /// @param status The status text.
    void setStatus(std::string status);

    /// @return The current progress fraction in [0, 1].
    [[nodiscard]] float progress() const noexcept { return _progress; }

    void render(tui::Canvas& canvas) override;
    [[nodiscard]] tui::EventResult onEvent(tui::InputEvent const& event) override;

    [[nodiscard]] bool focusable() const override { return true; }

    [[nodiscard]] tui::Size preferredSize() const override;

  private:
    std::string _target;    ///< The item being deleted.
    std::string _status;    ///< The status line text.
    float _progress = 0.0f; ///< Completion fraction in [0, 1].

    static constexpr int Width = 54;  ///< Fixed dialog width (columns), borders included.
    static constexpr int Height = 6;  ///< Fixed dialog height (rows), borders included.
    static constexpr int Padding = 2; ///< Horizontal padding inside the border.
};

} // namespace tuidu
