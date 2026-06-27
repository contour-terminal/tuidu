// SPDX-License-Identifier: Apache-2.0
#include <tui/Theme.hpp>

#include <catch2/catch_test_macros.hpp>

#include <tuidu/ThemeController.hpp>

using namespace tuidu;
using tui::ColorScheme;

TEST_CASE("ThemeController: Auto maps dark/light scheme to the matching theme", "[theme]")
{
    CHECK(ThemeController::resolve(ThemeMode::Auto, ColorScheme::Dark).colors.background
          == tui::darkTheme().colors.background);
    CHECK(ThemeController::resolve(ThemeMode::Auto, ColorScheme::Light).colors.background
          == tui::lightTheme().colors.background);
}

TEST_CASE("ThemeController: Auto defaults Unknown scheme to dark", "[theme]")
{
    CHECK(ThemeController::resolve(ThemeMode::Auto, ColorScheme::Unknown).colors.background
          == tui::darkTheme().colors.background);
}

TEST_CASE("ThemeController: pinned modes ignore the scheme", "[theme]")
{
    CHECK(ThemeController::resolve(ThemeMode::Dark, ColorScheme::Light).colors.background
          == tui::darkTheme().colors.background);
    CHECK(ThemeController::resolve(ThemeMode::Light, ColorScheme::Dark).colors.background
          == tui::lightTheme().colors.background);
    CHECK(ThemeController::resolve(ThemeMode::Mono, ColorScheme::Light).colors.background
          == tui::monoTheme().colors.background);
}

TEST_CASE("ThemeController: Auto follows a runtime scheme change", "[theme]")
{
    ThemeController controller(ThemeMode::Auto);
    controller.applyForScheme(ColorScheme::Dark);

    // Switching to light should change the theme and report it.
    CHECK(controller.onColorScheme(ColorScheme::Light));
    CHECK(tui::ThemeManager::instance().current().colors.background == tui::lightTheme().colors.background);

    // A repeated identical report does not re-trigger a change.
    CHECK_FALSE(controller.onColorScheme(ColorScheme::Light));
}

TEST_CASE("ThemeController: pinned mode ignores runtime scheme changes", "[theme]")
{
    ThemeController controller(ThemeMode::Dark);
    controller.applyForScheme(ColorScheme::Dark);
    CHECK_FALSE(controller.onColorScheme(ColorScheme::Light)); // pinned: no change
}

TEST_CASE("ThemeController: setMode switches and re-applies", "[theme]")
{
    ThemeController controller(ThemeMode::Auto);
    controller.applyForScheme(ColorScheme::Dark);

    CHECK(controller.setMode(ThemeMode::Light, ColorScheme::Dark));
    CHECK(controller.mode() == ThemeMode::Light);
    CHECK(tui::ThemeManager::instance().current().colors.background == tui::lightTheme().colors.background);
}

TEST_CASE("themeModeFromString: every name maps; unknown is rejected", "[theme][tables]")
{
    CHECK(themeModeFromString("auto") == ThemeMode::Auto);
    CHECK(themeModeFromString("dark") == ThemeMode::Dark);
    CHECK(themeModeFromString("light") == ThemeMode::Light);
    CHECK(themeModeFromString("mono") == ThemeMode::Mono);
    CHECK_FALSE(themeModeFromString("Dark").has_value()); // case-sensitive
    CHECK_FALSE(themeModeFromString("solarized").has_value());
}
