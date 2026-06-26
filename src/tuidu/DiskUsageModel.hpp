// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file DiskUsageModel.hpp
/// @brief tuidu's TreeTableModel: adapts the disk-usage Tree to the generic browser.

#include <tui/TreeTableView.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include <tuidu/Columns.hpp>
#include <tuidu/Tree.hpp>

namespace tuidu
{

/// Adapts a disk-usage @ref Tree to the generic @c tui::TreeTableModel so the reusable
/// @c tui::TreeTableView renders and navigates it. This is the DI seam between the
/// domain (Tree, columns, color rules, sort modes) and the generic component: the view
/// knows nothing about disk usage, and the Tree knows nothing about rendering.
class DiskUsageModel: public tui::TreeTableModel
{
  public:
    /// @param tree The disk-usage tree to present (not owned; outlives the model).
    explicit DiskUsageModel(Tree& tree) noexcept;

    // --- tui::TreeTableModel ---
    [[nodiscard]] std::vector<tui::TableColumn> columns() const override;
    [[nodiscard]] std::vector<tui::RowId> rows() const override;
    [[nodiscard]] std::string cellText(tui::RowId row, std::size_t column) const override;
    [[nodiscard]] tui::RowStyle rowStyle(tui::RowId row, bool selected) const override;
    [[nodiscard]] bool canDescend(tui::RowId row) const override;
    bool descend(tui::RowId row) override;
    bool ascend() override;
    [[nodiscard]] std::string currentTitle() const override;
    void sortBy(int sortKey) override;

    // --- tuidu view options (data-driven) ---

    /// Sets apparent-size vs disk-usage mode for size/bar/percent columns.
    void setSizeMode(SizeMode mode);

    /// @return The current size mode.
    [[nodiscard]] SizeMode sizeMode() const noexcept { return _sizeMode; }

    /// Sets the unit system (binary vs SI) used to format sizes.
    void setUnits(UnitSystem units) { _units = units; }

    /// @return The node id currently being browsed.
    [[nodiscard]] NodeId currentDir() const noexcept { return _currentDir; }

    /// Resets the browsed directory to the tree's root and re-sorts. Call after the tree's
    /// root is (re)created — e.g. once the scan has built it — since a model constructed
    /// over an empty tree has no valid current directory yet.
    void resetToRoot();

    /// Re-applies the active sort order to the current directory (e.g. after a scan grows it).
    void resort();

  private:
    /// @return The render context for @p node under the current view settings.
    [[nodiscard]] RenderCtx renderCtx(NodeId node) const;

    Tree& _tree;                             ///< The disk-usage tree.
    NodeId _currentDir;                      ///< The directory currently shown.
    std::size_t _sortMode = 0;               ///< Index into sortModes().
    SizeMode _sizeMode = SizeMode::Apparent; ///< Apparent vs disk usage.
    UnitSystem _units = UnitSystem::Binary;  ///< Unit formatting.
};

} // namespace tuidu
