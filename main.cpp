#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <thread>
#include <mutex>

std::mutex mtx;
int shared_count = 0;
int shared_max = INT_MIN;

std::atomic<int> atomic_count{0};
std::atomic<int> atomic_max{INT_MIN};

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

void thread_worker_mutex(const std::vector<int>& data, size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
        if (data[i] % 3 == 0) {
            std::lock_guard<std::mutex> lock(mtx);
            shared_count++;
            if (data[i] > shared_max) {
                shared_max = data[i];
            }
        }
    }
}

void run_mutex_test(const std::vector<int>& data, int num_threads) {
    shared_count = 0;
    shared_max = INT_MIN;
    std::vector<std::thread> threads;
    size_t chunk_size = data.size() / num_threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        size_t start = i * chunk_size;
        size_t end = (i == num_threads - 1) ? data.size() : (i + 1) * chunk_size;
        threads.emplace_back(thread_worker_mutex, std::ref(data), start, end);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    std::cout << "--- Mutex Execution  ---" << std::endl;
    std::cout << "Threads used: " << num_threads << std::endl;
    std::cout << "Count: " << shared_count << std::endl;
    std::cout << "Maximum element: " << shared_max << std::endl;
    std::cout << "Time: " << elapsed.count() << " ms" << std::endl;
}

void thread_worker_cas(const std::vector<int>& data, size_t start, size_t end) {
    int local_counter = 0;
    for (size_t i = start; i < end; ++i) {
        if (data[i] % 3 == 0) {
            int val = data[i];
            local_counter++;

            int current_max = atomic_max.load();
            while (val > current_max && !atomic_max.compare_exchange_weak(current_max, val)) {}
        }
    }

    int current_count = atomic_count.load();
    while (!atomic_count.compare_exchange_weak(current_count, current_count + local_counter)) {}
}

void run_cas_test(const std::vector<int>& data, int num_threads) {
    atomic_count.store(0);
    atomic_max.store(INT_MIN);
    std::vector<std::thread> threads;
    size_t chunk_size = data.size() / num_threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        size_t start = i * chunk_size;
        size_t end = (i == num_threads - 1) ? data.size() : (i + 1) * chunk_size;
        threads.emplace_back(thread_worker_cas, std::ref(data), start, end);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    std::cout << "--- CAS Execution ---" << std::endl;
    std::cout << "Threads used: " << num_threads << std::endl;
    std::cout << "Count: " << atomic_count.load() << std::endl;
    std::cout << "Maximum element: " << atomic_max.load() << std::endl;
    std::cout << "Time: " << elapsed.count() << " ms" << std::endl;
}

int main() {
    const size_t size = 1000000000;
    std::vector<int> data(size);
    int num_threads = 6;

    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(1, 1000000);
    for (size_t i = 0; i < size; ++i) {
        data[i] = dis(gen);
    }

    run_sequential_test(data);

    run_mutex_test(data, num_threads);

    run_cas_test(data, num_threads);

    return 0;
}