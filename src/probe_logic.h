#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace probe_logic {

enum class PickStatus : std::uint8_t { None = 0, Selected = 1, Ambiguous = 2 };

struct HitRank {
    int index = -1;
    float area = 0.0f;
    int depth = 0;
    std::uint64_t stableId = 0;
};

struct PickResult {
    PickStatus status = PickStatus::None;
    int index = -1;
};

inline PickResult ChooseBestHit(std::vector<HitRank> hits, float areaEpsilon = 0.5f) {
    hits.erase(std::remove_if(hits.begin(), hits.end(), [](const HitRank& h) {
        return h.index < 0 || !std::isfinite(h.area) || h.area <= 0.0f;
    }), hits.end());
    if (hits.empty()) return {};

    std::stable_sort(hits.begin(), hits.end(), [areaEpsilon](const HitRank& a, const HitRank& b) {
        if (std::fabs(a.area - b.area) > areaEpsilon) return a.area < b.area;
        if (a.depth != b.depth) return a.depth > b.depth;
        return a.stableId < b.stableId;
    });
    return {PickStatus::Selected, hits.front().index};
}

inline PickResult ChooseHit(std::vector<HitRank> hits, float areaEpsilon = 0.5f) {
    hits.erase(std::remove_if(hits.begin(), hits.end(), [](const HitRank& h) {
        return h.index < 0 || !std::isfinite(h.area) || h.area <= 0.0f;
    }), hits.end());
    if (hits.empty()) return {};

    std::stable_sort(hits.begin(), hits.end(), [areaEpsilon](const HitRank& a, const HitRank& b) {
        if (std::fabs(a.area - b.area) > areaEpsilon) return a.area < b.area;
        if (a.depth != b.depth) return a.depth > b.depth;
        return a.stableId < b.stableId;
    });

    if (hits.size() > 1 &&
        std::fabs(hits[0].area - hits[1].area) <= areaEpsilon &&
        hits[0].depth == hits[1].depth &&
        hits[0].stableId != hits[1].stableId) {
        return {PickStatus::Ambiguous, -1};
    }
    return {PickStatus::Selected, hits.front().index};
}

struct SnapshotDiff {
    std::size_t added = 0;
    std::size_t removed = 0;
    std::size_t unchanged = 0;
    std::vector<std::uint64_t> addedIds;
    std::vector<std::uint64_t> removedIds;
};

inline SnapshotDiff DiffIdentities(std::vector<std::uint64_t> before,
                                   std::vector<std::uint64_t> after) {
    std::sort(before.begin(), before.end());
    before.erase(std::unique(before.begin(), before.end()), before.end());
    std::sort(after.begin(), after.end());
    after.erase(std::unique(after.begin(), after.end()), after.end());

    SnapshotDiff out{};
    std::size_t i = 0, j = 0;
    while (i < before.size() || j < after.size()) {
        if (i == before.size()) {
            out.addedIds.push_back(after[j++]);
        } else if (j == after.size()) {
            out.removedIds.push_back(before[i++]);
        } else if (before[i] == after[j]) {
            ++out.unchanged; ++i; ++j;
        } else if (before[i] < after[j]) {
            out.removedIds.push_back(before[i++]);
        } else {
            out.addedIds.push_back(after[j++]);
        }
    }
    out.added = out.addedIds.size();
    out.removed = out.removedIds.size();
    return out;
}

inline std::uint64_t Fnv1a64(std::wstring_view text,
                             std::uint64_t seed = 14695981039346656037ull) {
    std::uint64_t hash = seed ? seed : 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    for (wchar_t ch : text) {
        const std::uint32_t v = static_cast<std::uint32_t>(ch);
        for (unsigned shift = 0; shift < 32; shift += 8) {
            hash ^= static_cast<std::uint8_t>((v >> shift) & 0xffu);
            hash *= prime;
        }
    }
    return hash ? hash : 1ull;
}

} // namespace probe_logic
