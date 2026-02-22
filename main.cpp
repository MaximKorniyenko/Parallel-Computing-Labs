#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

std::pair<std::vector<double>, std::vector<double>> generate_matrix_vector(int n) {
    std::vector<double> matrix(n * n);
    std::vector<double> vec(n);

    for (int i = 0; i < n * n; i++) {
        matrix[i] = (double)rand() / RAND_MAX;
    }
    for (int i = 0; i < n; i++) {
        vec[i] = (double)rand() / RAND_MAX;
    }

    return {std::move(matrix), std::move(vec)};
}


double multiply_sequential(const std::vector<double>& a, const std::vector<double>& b, std::vector<double>& c,const int n) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        double sum = 0;
        int row_offset = i * n;
        for (int j = 0; j < n; j++) {
            sum += a[row_offset + j] * b[j];
        }
        c[i] = sum;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count();
}

void run_benchmarks_sequential(const std::vector<int>& sizes) {
    std::cout << std::setw(10) << "N" << std::setw(20) << "Time (ms)" << std::setw(20) << "Result[0]" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    for (int n : sizes) {
        auto [matrix, vec] = generate_matrix_vector(n);
        std::vector<double> result(n);

        // warm-up
        multiply_sequential(matrix, vec, result, n);

        double time = multiply_sequential(matrix, vec, result, n);

        std::cout << std::setw(10) << n
                  << std::setw(20) << std::fixed << std::setprecision(3) << time
                  << std::setw(20) << result[0] << std::endl;
    }
}

int main() {
    srand(time(NULL));

    std::cout << "DEMONSTRATION (N=3)" << std::endl;
    int demo_n = 3;
    auto [m_demo, v_demo] = generate_matrix_vector(demo_n);
    std::vector<double> r_demo(demo_n);

    multiply_sequential(m_demo, v_demo, r_demo, demo_n);

    std::cout << "Matrix:" << std::endl;
    for(int i=0; i<demo_n; i++) {
        for(int j=0; j<demo_n; j++) std::cout << std::fixed << std::setprecision(2) << m_demo[i*demo_n+j] << "\t";
        std::cout << std::endl;
    }
    std::cout << "\nVector: ";
    for(auto x : v_demo) std::cout << x << "\t";
    std::cout << "\n\nResult: ";
    for(auto x : r_demo) std::cout << x << "\t";
    std::cout << "\n\n";

    std::cout << "BENCHMARK" << std::endl;
    std::vector<int> sizes = {100, 500, 1000, 2000, 3000};
    run_benchmarks_sequential(sizes);

    return 0;
}