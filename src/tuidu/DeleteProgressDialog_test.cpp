// SPDX-License-Identifier: Apache-2.0
#include <tui/Buffer.hpp>
#include <tui/Canvas.hpp>
#include <tui/InputEvent.hpp>
#include <tui/KeyCode.hpp>
#include <tui/Modifier.hpp>
#include <tui/Rect.hpp>
#include <tui/TestHelpers.hpp>
#include <tui/Theme.hpp>

#include <catch2/catch_test_macros.hpp>

#include <tuidu/DeleteProgressDialog.hpp>

using namespace tuidu;

namespace
{
/// Renders the dialog to a fresh buffer and returns its text content.
std::string render(DeleteProgressDialog& dialog, int width = 60, int height = 8)
{
    tui::Buffer buffer(height, width);
    buffer.clear();
    tui::Theme theme;
    tui::Canvas canvas(buffer, tui::Rect { .x = 0, .y = 0, .width = width, .height = height }, theme);
    dialog.render(canvas);
    return tui::test::canvasToString(buffer);
}
} // namespace

TEST_CASE("DeleteProgressDialog: renders target, status, and a cancel hint", "[delete][dialog]")
{
    DeleteProgressDialog dialog;
    dialog.setTarget("/tmp/huge-dir");
    dialog.setStatus("Deleting 12 / 40");
    dialog.setProgress(0.3f);

    auto const content = render(dialog);
    CHECK(content.find("/tmp/huge-dir") != std::string::npos);
    CHECK(content.find("Deleting 12 / 40") != std::string::npos);
    CHECK(content.find("Esc to cancel") != std::string::npos);
    // A partial bar paints at least one filled cell.
    CHECK(content.find("█") != std::string::npos);
}

TEST_CASE("DeleteProgressDialog: clamps progress into [0, 1]", "[delete][dialog]")
{
    DeleteProgressDialog dialog;
    dialog.setProgress(-1.0f);
    CHECK(dialog.progress() == 0.0f);
    dialog.setProgress(2.5f);
    CHECK(dialog.progress() == 1.0f);
    dialog.setProgress(0.5f);
    CHECK(dialog.progress() == 0.5f);
}

TEST_CASE("DeleteProgressDialog: swallows key events but ignores others", "[delete][dialog]")
{
    DeleteProgressDialog dialog;

    auto const key =
        tui::InputEvent { tui::KeyEvent { .key = {}, .modifiers = tui::Modifier::None, .codepoint = U'x' } };
    CHECK(dialog.onEvent(key) == tui::EventResult::Handled);

    auto const resize = tui::InputEvent { tui::ResizeEvent { .columns = 80, .rows = 24 } };
    CHECK(dialog.onEvent(resize) == tui::EventResult::Ignored);
}
