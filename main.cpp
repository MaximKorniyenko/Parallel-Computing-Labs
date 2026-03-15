#include <iostream>
#include <vector>
#include <chrono>
#include <random>

void run_sequential_test(const std::vector<int>& data) {
    int count = 0;
    int max_val = INT_MIN;

    auto start = std::chrono::high_resolution_clock::now();

    for (int x : data) {
        if (x % 3 == 0) {
            count++;
            if (x > max_val) {
                max_val = x;
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    std::cout << "--- Sequential Execution  ---" << std::endl;
    std::cout << "Count: " << count << std::endl;
    std::cout << "Maximum element: " << max_val << std::endl;
    std::cout << "Execution time: " << elapsed.count() << " ms" << std::endl;
}

int main() {
    const size_t size = 100000;
    std::vector<int> data(size);
    int num_threads = 6;

    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(1, 1000000);
    for (size_t i = 0; i < size; ++i) {
        data[i] = dis(gen);
    }

    run_sequential_test(data);

    return 0;
}