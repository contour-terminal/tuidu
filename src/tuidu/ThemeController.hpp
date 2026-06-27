// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file ThemeController.hpp
/// @brief Configurable theming with automatic dark/light detection and live switching.

#include <tui/Terminal.hpp>
#include <tui/Theme.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace tuidu
{

/// How the active theme is chosen.
enum class ThemeMode : std::uint8_t
{
    Auto,  ///< Follow the terminal's reported dark/light color scheme.
    Dark,  ///< Always use the dark theme.
    Light, ///< Always use the light theme.
    Mono,  ///< Always use the monochrome theme.
};

/// Parses a @ref ThemeMode from its lowercase config/CLI name (data-driven lookup).
/// @param name One of "auto", "dark", "light", "mono" (case-sensitive).
/// @return The matching mode, or @c std::nullopt if @p name is unrecognized.
[[nodiscard]] std::optional<ThemeMode> themeModeFromString(std::string_view name) noexcept;

/// Resolves a @ref ThemeMode plus the terminal's reported @c tui::ColorScheme into a
/// concrete @c tui::Theme and installs it (via @c tui::ThemeManager and the @c Screen).
///
/// In @ref ThemeMode::Auto it follows the terminal: it applies the right theme at startup
/// from @c Terminal::colorScheme() and re-applies it whenever a @c ColorSchemeReport
/// arrives at runtime (the app forwards those to @ref onColorScheme). Explicit modes pin
/// a theme regardless of the terminal. The mapping is pure data — adding a mode is a
/// switch arm here, not scattered logic.
class ThemeController
{
  public:
    /// @param mode The initial theme mode.
    explicit ThemeController(ThemeMode mode = ThemeMode::Auto) noexcept;

    /// Resolves and installs the theme for @p scheme under the current mode. Call once at
    /// startup with @c Terminal::colorScheme().
    /// @param scheme The terminal's current color scheme.
    /// @return The theme that was installed.
    tui::Theme applyForScheme(tui::ColorScheme scheme);

    /// Handles a runtime color-scheme change. In @ref ThemeMode::Auto this swaps the theme
    /// and returns true (the caller should redraw); in a pinned mode it is a no-op.
    /// @param scheme The newly reported color scheme.
    /// @return true if the active theme changed.
    bool onColorScheme(tui::ColorScheme scheme);

    /// Switches the theme mode and re-applies it against @p scheme.
    /// @param mode The new mode.
    /// @param scheme The current color scheme (used in Auto mode).
    /// @return true if the active theme changed.
    bool setMode(ThemeMode mode, tui::ColorScheme scheme);

    /// @return The current theme mode.
    [[nodiscard]] ThemeMode mode() const noexcept { return _mode; }

    /// Resolves (without installing) the theme for @p mode and @p scheme. Pure mapping.
    /// @param mode The theme mode.
    /// @param scheme The terminal color scheme (used only in Auto).
    /// @return The theme that mode/scheme maps to.
    [[nodiscard]] static tui::Theme resolve(ThemeMode mode, tui::ColorScheme scheme);

  private:
    /// Installs @p theme into the global ThemeManager and records it as current.
    void install(tui::Theme theme);

    ThemeMode _mode;          ///< Active theme mode.
    tui::Theme _current;      ///< The currently-installed theme.
    bool _hasCurrent = false; ///< Whether @c _current has been set yet.
};

} // namespace tuidu
