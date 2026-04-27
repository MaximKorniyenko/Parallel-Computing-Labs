#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <memory>
#include <windows.h>
#include "ThreadPool.h"

std::atomic<int> completed_tasks{0};
std::atomic<long long> total_execution_time_ms{0};
std::atomic<long long> total_wait_in_queue_ms{0};

std::atomic<bool> is_testing{true};
std::atomic<int> monitor_samples{0};

std::unique_ptr<std::atomic<long long>[]> sum_queue_lengths;

void generate_tasks(ThreadPoolV5& pool, int generator_id, int num_tasks) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> time_dist(4, 15);

    for (int i = 0; i < num_tasks; ++i) {
        int exec_time = time_dist(gen);

        auto enqueue_time = std::chrono::steady_clock::now();

        pool.add_task([exec_time, generator_id, i, enqueue_time]() {
            auto start_time = std::chrono::steady_clock::now();
            auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(start_time - enqueue_time).count();
            total_wait_in_queue_ms += wait_time;

            std::cout << "[Потік: " << std::this_thread::get_id() << "] "
                      << "Початок задачі " << i << " (Generator_Id " << generator_id << "). "
                      << "Час в черзі: " << wait_time / 1000.0 << "с.\n";

            std::this_thread::sleep_for(std::chrono::seconds(exec_time));

            auto end_time = std::chrono::steady_clock::now();
            auto actual_exec_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            total_execution_time_ms += actual_exec_time;
            completed_tasks++;

            std::cout << "[Потік: " << std::this_thread::get_id() << "] "
                      << "Завершено задачу " << i << " (Generator_Id " << generator_id << ") за "
                      << actual_exec_time / 1000.0 << "с.\n";
        });

        // імітація інтервалу надходження нових задач
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void monitor_queues(ThreadPoolV5& pool, size_t num_queues) {
    while (is_testing.load()) {
        for (size_t i = 0; i < num_queues; ++i) {
            sum_queue_lengths[i] += pool.get_queue_size(i);
        }
        monitor_samples++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "--- Запуск тестування ---\n";
    ThreadPoolV5 pool;

    size_t num_queues = 2;
    size_t workers_per_queue = 2;
    size_t total_workers = num_queues * workers_per_queue;

    sum_queue_lengths = std::make_unique<std::atomic<long long>[]>(num_queues);
    for (size_t i = 0; i < num_queues; ++i) {
        sum_queue_lengths[i].store(0);
    }

    pool.initialize(num_queues, workers_per_queue);

    auto test_start_time = std::chrono::steady_clock::now();

    std::thread monitor(monitor_queues, std::ref(pool), num_queues);

    const int num_generators = 3;
    const int tasks_per_generator = 10;
    std::vector<std::thread> generators;

    for (int i = 0; i < num_generators; ++i) {
        generators.emplace_back(generate_tasks, std::ref(pool), i, tasks_per_generator);
    }

    for (auto& gen : generators) {
        gen.join();
    }
    std::cout << "\n>>> Всі генератори завершили додавання задач.\n";
    std::cout << ">>> Очікуємо виконання залишку задач...\n\n";

    pool.terminate(true);

    is_testing.store(false);
    monitor.join();

    auto test_end_time = std::chrono::steady_clock::now();
    auto total_test_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_end_time - test_start_time).count();

    std::cout << "\n=== РЕЗУЛЬТАТИ ТЕСТУВАННЯ (МЕТРИКИ) ===\n";
    std::cout << "1. Кількість створених робочих потоків пулу: " << total_workers << "\n";
    std::cout << "2. Загальна кількість виконаних задач: " << completed_tasks << "\n";

    if (completed_tasks > 0) {
        std::cout << "3. Середній час виконання однієї задачі: "
                  << (total_execution_time_ms / completed_tasks) / 1000.0 << " секунд\n";
        std::cout << "4. Середній час знаходження задачі в черзі: "
                  << (total_wait_in_queue_ms / completed_tasks) / 1000.0 << " секунд\n";
    }

    if (monitor_samples > 0) {
        std::cout << "5. Середня довжина черг:\n";
        for (size_t i = 0; i < num_queues; ++i) {
            std::cout << "   - Черга " << i << ": "
                      << static_cast<double>(sum_queue_lengths[i].load()) / monitor_samples << " задач\n";
        }
    }

    // загальний доступний процесорний час = час тесту * кількість потоків
    long long total_available_worker_time_ms = total_test_duration_ms * total_workers;
    // час простою
    long long total_idle_time_ms = total_available_worker_time_ms - total_execution_time_ms;

    if (total_idle_time_ms < 0) total_idle_time_ms = 0;

    std::cout << "\n6. Середній час знаходження потоку в стані очікування (простою): "
              << (total_idle_time_ms / total_workers) / 1000.0 << " секунд\n";

    return 0;
}