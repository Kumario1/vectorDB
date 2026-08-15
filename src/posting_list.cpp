#include "vectordb/posting_list.hpp"

#include <algorithm>
#include <numeric>

namespace vectordb {

void PostingList::insert(std::uint64_t id) {
    auto it = std::lower_bound(ids_.begin(), ids_.end(), id);
    if (it != ids_.end() && *it == id) {
        return;
    }
    ids_.insert(it, id);
}

bool PostingList::erase(std::uint64_t id) {
    auto it = std::lower_bound(ids_.begin(), ids_.end(), id);
    if (it == ids_.end() || *it != id) {
        return false;
    }
    ids_.erase(it);
    return true;
}

bool PostingList::contains(std::uint64_t id) const {
    auto it = std::lower_bound(ids_.begin(), ids_.end(), id);
    return it != ids_.end() && *it == id;
}

PostingList intersect(const PostingList& a, const PostingList& b) {
    PostingList out;
    const auto& left = a.ids_;
    const auto& right = b.ids_;
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < left.size() && j < right.size()) {
        if (left[i] == right[j]) {
            out.ids_.push_back(left[i]);
            ++i;
            ++j;
        } else if (left[i] < right[j]) {
            ++i;
        } else {
            ++j;
        }
    }
    return out;
}

PostingList intersect_all(const std::vector<PostingList>& lists) {
    if (lists.empty()) {
        return PostingList{};
    }
    for (const auto& pl : lists) {
        if (pl.empty()) {
            return PostingList{};
        }
    }

    std::vector<std::size_t> order(lists.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
        return lists[x].size() < lists[y].size();
    });

    PostingList result = lists[order[0]];
    for (std::size_t k = 1; k < order.size(); ++k) {
        result = intersect(result, lists[order[k]]);
        if (result.empty()) {
            return result;
        }
    }
    return result;
}

}  // namespace vectordb
