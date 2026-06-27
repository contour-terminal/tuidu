// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <array>
#include <utility>

#include <tuidu/ThemeController.hpp>

namespace tuidu
{

namespace
{
    /// Config/CLI name → @ref ThemeMode. Adding a mode is one row here.
    constexpr std::array<std::pair<std::string_view, ThemeMode>, 4> ThemeModeNames { {
        { "auto", ThemeMode::Auto },
        { "dark", ThemeMode::Dark },
        { "light", ThemeMode::Light },
        { "mono", ThemeMode::Mono },
    } };
} // namespace

std::optional<ThemeMode> themeModeFromString(std::string_view name) noexcept
{
    for (auto const& [label, mode]: ThemeModeNames)
        if (label == name)
            return mode;
    return std::nullopt;
}

ThemeController::ThemeController(ThemeMode mode) noexcept: _mode(mode)
{
}

tui::Theme ThemeController::resolve(ThemeMode mode, tui::ColorScheme scheme)
{
    switch (mode)
    {
        case ThemeMode::Dark: return tui::darkTheme();
        case ThemeMode::Light: return tui::lightTheme();
        case ThemeMode::Mono: return tui::monoTheme();
        case ThemeMode::Auto: break;
    }
    // Auto: follow the terminal. Unknown defaults to dark (the conventional safe default).
    return (scheme == tui::ColorScheme::Light) ? tui::lightTheme() : tui::darkTheme();
}

void ThemeController::install(tui::Theme theme)
{
    _current = theme;
    _hasCurrent = true;
    tui::ThemeManager::instance().setCurrent(_current);
}

tui::Theme ThemeController::applyForScheme(tui::ColorScheme scheme)
{
    install(resolve(_mode, scheme));
    return _current;
}

bool ThemeController::onColorScheme(tui::ColorScheme scheme)
{
    if (_mode != ThemeMode::Auto)
        return false; // pinned: ignore terminal changes

    auto next = resolve(_mode, scheme);
    // Only redraw if the palette actually changed (avoid churn on duplicate reports).
    if (_hasCurrent && next.colors.background == _current.colors.background
        && next.colors.text == _current.colors.text)
        return false;

    install(next);
    return true;
}

bool ThemeController::setMode(ThemeMode mode, tui::ColorScheme scheme)
{
    _mode = mode;
    auto next = resolve(_mode, scheme);
    auto const changed = !_hasCurrent || next.colors.background != _current.colors.background
                         || next.colors.text != _current.colors.text;
    install(next);
    return changed;
}

} // namespace tuidu
