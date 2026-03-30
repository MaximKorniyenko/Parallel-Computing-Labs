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
    std::cout << "Execution time: " << elapsed.count() << " ms\n" << std::endl;
}

void thread_worker_mutex(const std::vector<int>& data, size_t start, size_t end) {
    int local_counter = 0;
    int local_max = INT_MIN;
    for (size_t i = start; i < end; ++i) {
        if (data[i] % 3 == 0) {
            local_counter++;
            if (data[i] > local_max) {
                local_max = data[i];
            }
        }
    }

    std::lock_guard<std::mutex> lock(mtx);
    shared_count += local_counter;
    if (local_max > shared_max) {
        shared_max = local_max;
    }
}

void thread_worker_mutex_heavy(const std::vector<int>& data, size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
        if (data[i] % 3 == 0) {
            std::lock_guard<std::mutex> lock(mtx); // Блокуємо на кожній ітерації!
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
        threads.emplace_back(thread_worker_mutex_heavy, std::ref(data), start, end);
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
    std::cout << "Time: " << elapsed.count() << " ms\n" << std::endl;
}

void thread_worker_cas(const std::vector<int>& data, size_t start, size_t end) {
    int local_counter = 0;
    int local_max = INT_MIN;
    for (size_t i = start; i < end; ++i) {
        if (data[i] % 3 == 0) {
            local_counter++;

            if (local_max < data[i]) {
                local_max = data[i];
            }

        }
    }

    int current_count = atomic_count.load();
    while (!atomic_count.compare_exchange_weak(current_count, current_count + local_counter, std::memory_order_relaxed)) {}
    int current_max = atomic_max.load();
    while (local_max > current_max && !atomic_max.compare_exchange_weak(current_max, local_max, std::memory_order_relaxed)) {}
}

void thread_worker_cas_heavy(const std::vector<int>& data, size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
        if (data[i] % 3 == 0) {

            int current_count = atomic_count.load(std::memory_order_relaxed);
            while (!atomic_count.compare_exchange_weak(current_count, current_count + 1, std::memory_order_relaxed)) {}

            int current_max = atomic_max.load(std::memory_order_relaxed);
            while (data[i] > current_max && !atomic_max.compare_exchange_weak(current_max, data[i], std::memory_order_relaxed)) {}
        }
    }
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
        threads.emplace_back(thread_worker_cas_heavy, std::ref(data), start, end);
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
    std::cout << "Time: " << elapsed.count() << " ms\n" << std::endl;
}

void run_benchmarks(const std::vector<int>& data) {
    std::vector<int> thread_counts = {3, 6, 12, 24};

    std::cout << "===========================================" << std::endl;
    std::cout << "STARTING AUTOMATED BENCHMARK" << std::endl;
    std::cout << "Data size: " << data.size() << " elements" << std::endl;
    std::cout << "===========================================\n" << std::endl;

    run_sequential_test(data);
    std::cout << std::endl;

    for (int threads : thread_counts) {
        std::cout << ">>> TESTING WITH " << threads << " THREADS <<<" << std::endl;

        run_mutex_test(data, threads);
        run_cas_test(data, threads);

        std::cout << "-------------------------------------------" << std::endl;
    }

    std::cout << "BENCHMARK COMPLETED\n" << std::endl;
}

int main() {
    std::vector<size_t> sizes = {100000, 1000000, 10000000, 100000000, 100000000};

    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(1, 1000000);

    for (size_t size : sizes) {
        std::vector<int> data(size);

        for (size_t i = 0; i < size; ++i) {
            data[i] = dis(gen);
        }

        run_benchmarks(data);
    }

    return 0;
}