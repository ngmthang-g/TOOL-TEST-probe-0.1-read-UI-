#include <cassert>
#include <cstdint>
#include <vector>
#include <string>
#include "probe_logic.h"

using namespace probe_logic;

int main() {
    {
        std::vector<HitRank> hits{
            {0, 400.0f, 3, 10},
            {1, 100.0f, 1, 20},
            {2, 250.0f, 7, 30},
        };
        const PickResult r = ChooseHit(hits);
        assert(r.status == PickStatus::Selected);
        assert(r.index == 1);
    }
    {
        std::vector<HitRank> hits{
            {5, 100.0f, 2, 40},
            {6, 100.2f, 5, 50},
        };
        const PickResult r = ChooseHit(hits, 0.5f);
        assert(r.status == PickStatus::Selected);
        assert(r.index == 6);
    }
    {
        std::vector<HitRank> hits{
            {8, 80.0f, 4, 111},
            {9, 80.1f, 4, 222},
        };
        const PickResult r = ChooseHit(hits, 0.5f);
        assert(r.status == PickStatus::Ambiguous);
        assert(r.index == -1);
    }
    {
        std::vector<HitRank> hits;
        const PickResult r = ChooseHit(hits);
        assert(r.status == PickStatus::None);
    }
    {
        std::vector<HitRank> hits{
            {11, 50.0f, 4, 900},
            {12, 50.0f, 4, 100},
        };
        const PickResult r = ChooseBestHit(hits);
        assert(r.status == PickStatus::Selected);
        assert(r.index == 12);
    }
    {
        const std::vector<std::uint64_t> before{1, 2, 3};
        const std::vector<std::uint64_t> after{2, 3, 4, 5};
        const SnapshotDiff d = DiffIdentities(before, after);
        assert(d.added == 2);
        assert(d.removed == 1);
        assert(d.unchanged == 2);
        assert(d.addedIds.size() == 2);
        assert(d.removedIds.size() == 1);
        assert(d.addedIds[0] == 4 && d.addedIds[1] == 5);
        assert(d.removedIds[0] == 1);
    }
    {
        assert(Fnv1a64(L"UIButton|Bag|", 0) != 0);
        assert(Fnv1a64(L"UIButton|Bag|", 0) == Fnv1a64(L"UIButton|Bag|", 0));
        assert(Fnv1a64(L"UIButton|Bag|", 0) != Fnv1a64(L"UIButton|Close|", 0));
    }
    return 0;
}
