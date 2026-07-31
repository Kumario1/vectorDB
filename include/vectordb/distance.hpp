#pragma once

#include <span>

namespace vectordb {

// Precondition: a.size() == b.size(). Behavior if not is undefined (asserted in .cpp).

float dot_product(std::span<const float> a, std::span<const float> b);
float cosine_similarity(std::span<const float> a, std::span<const float> b);
float squared_euclidean(std::span<const float> a, std::span<const float> b);

}  // namespace vectordb
