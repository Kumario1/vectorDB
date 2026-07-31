#include "vectordb/distance.hpp"

#include <cassert>
#include <cmath>

namespace vectordb {

float dot_product(std::span<const float> a, std::span<const float> b) {
    // assert same size
    // sum = 0; for i: sum += a[i] * b[i]
    // return sum
    assert(a.size() == b.size());
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

float cosine_similarity(std::span<const float> a, std::span<const float> b) {
    // assert same size
    // compute dot, norm_a = sqrt(sum a²), norm_b = sqrt(sum b²)
    // if norm_a == 0 or norm_b == 0 → return 0.0f
    // return dot / (norm_a * norm_b)
    assert(a.size() == b.size());
    float dot = dot_product(a, b);
    float norm_a = std::sqrt(dot_product(a, a));
    float norm_b = std::sqrt(dot_product(b, b));
    if (norm_a == 0.0f || norm_b == 0.0f) {return 0.0f;}
    return dot / (norm_a * norm_b);
}

float squared_euclidean(std::span<const float> a, std::span<const float> b) {
    // assert same size
    assert(a.size() == b.size());
    // sum (a[i] - b[i])^2
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum += (a[i] - b[i]) * (a[i] - b[i]);
    }
    return sum;
}

}  // namespace vectordb