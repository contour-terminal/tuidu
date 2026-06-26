// SPDX-License-Identifier: Apache-2.0
#include <tui/completer/FuzzyMatch.hpp>
#include <tui/completer/SmartCaseMatch.hpp>

#include <algorithm>
#include <cctype>
#include <string>

#include <tuidu/Search.hpp>

namespace tuidu
{

namespace
{
    constexpr auto kNoPos = std::string_view::npos;

    /// @return Lowercased copy of @p s (ASCII), for case-insensitive substring search.
    [[nodiscard]] std::string toLower(std::string_view s)
    {
        std::string out;
        out.reserve(s.size());
        for (auto const ch: s)
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        return out;
    }

    /// Finds the contiguous-substring position of @p query in @p name under smart-case
    /// rules: case-insensitive unless the query carries an uppercase letter.
    /// @return Byte offset of the match, or npos if @p query is not a substring.
    [[nodiscard]] std::size_t substringPosition(std::string_view name, std::string_view query)
    {
        if (tui::SmartCaseMatch::hasUppercase(query))
            return name.find(query);
        return toLower(name).find(toLower(query));
    }
} // namespace

std::vector<ScoredNode> rankMatches(Tree const& tree,
                                    std::span<NodeId const> candidates,
                                    std::string_view query)
{
    std::vector<ScoredNode> results;
    results.reserve(candidates.size());

    // Empty query: pass everything through in the candidates' original order.
    if (query.empty())
    {
        for (auto const id: candidates)
            results.push_back(ScoredNode { .node = id, .substring = true, .position = 0, .score = 0 });
        return results;
    }

    for (auto const id: candidates)
    {
        auto const name = tree.name(id);

        if (auto const pos = substringPosition(name, query); pos != kNoPos)
        {
            results.push_back(ScoredNode { .node = id, .substring = true, .position = pos, .score = 0 });
            continue;
        }

        auto const fuzzy = tui::FuzzyMatch::matchSmartCase(name, query);
        if (fuzzy.matches)
        {
            auto const score = tui::FuzzyMatch::calculateScore(0, name, query, fuzzy);
            results.push_back(
                ScoredNode { .node = id, .substring = false, .position = kNoPos, .score = score });
        }
    }

    // Ranking policy: substring beats fuzzy; then position (substring) / score (fuzzy);
    // then shorter name; then name byte-order for a stable, deterministic result.
    std::ranges::sort(results, [&](ScoredNode const& a, ScoredNode const& b) {
        if (a.substring != b.substring)
            return a.substring; // substring matches first

        if (a.substring)
        {
            if (a.position != b.position)
                return a.position < b.position;
        }
        else
        {
            if (a.score != b.score)
                return a.score > b.score;
        }

        auto const nameA = tree.name(a.node);
        auto const nameB = tree.name(b.node);
        if (nameA.size() != nameB.size())
            return nameA.size() < nameB.size();
        return nameA < nameB;
    });

    return results;
}

} // namespace tuidu
