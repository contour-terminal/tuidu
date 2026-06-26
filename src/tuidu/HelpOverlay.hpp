// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file HelpOverlay.hpp
/// @brief A modal help panel listing the active key bindings.

#include <tui/Component.hpp>
#include <tui/InputEvent.hpp>

#include <vector>

#include <tuidu/Keymap.hpp>

namespace tuidu
{

/// A centered, bordered overlay that lists key bindings as "key — description" rows.
///
/// It is built from the same @ref Keymap that drives dispatch, so the help can never drift
/// from the real bindings (alias rows, which have empty help, are excluded). Any key press
/// dismisses it (reported via @ref onEvent returning Handled, which the host treats as
/// "close").
class HelpOverlay: public tui::Component
{
  public:
    /// @param keymap The keymap whose help entries to display (copied into rows).
    explicit HelpOverlay(Keymap const& keymap);

    void render(tui::Canvas& canvas) override;
    [[nodiscard]] tui::EventResult onEvent(tui::InputEvent const& event) override;

    [[nodiscard]] bool focusable() const override { return true; }

    /// @return The preferred size (box sized to fit the entries).
    [[nodiscard]] tui::Size preferredSize() const override;

  private:
    std::vector<HelpEntry> _entries; ///< The help rows (key + description).
    int _width = 0;                  ///< Computed box width.
};

} // namespace tuidu
