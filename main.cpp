#include <iostream>
#include <vector>
#include <chrono>
#include <random>

int main() {
    const size_t size = 100000;
    std::vector<int> data(size);
    int num_threads = 6;

    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(1, 1000000);
    for (size_t i = 0; i < size; ++i) {
        data[i] = dis(gen);
    }

    return 0;
}