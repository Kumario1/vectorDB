#include "vectordb/vector_store.hpp"
#include <iostream>
#include <vector>

int main() {
    vectordb::VectorStore store(3);

    const std::vector<float> values = {1.0f, 2.0f, 3.0f};
    auto position = store.append(101, values);
    if (!position) {
        std::cerr << "Failed to append vector" << std::endl;
        return 1;
    }

    const auto& record = store.at(*position);
    std::cout << "id=" << record.id
              << " position=" << *position
              << " values=[" << record.values[0] << ", "
              << record.values[1] << ", "
              << record.values[2] << "]\n";
    return 0;
}