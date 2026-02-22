#include <iostream>
#include <vector>
#include <chrono>

void multiply_sequential(const std::vector<double>& a, const std::vector<double>& b, std::vector<double>& c,const int n) {
    for (int i = 0; i < n; i++) {
        double sum = 0;
        for (int j = 0; j < n; j++) {
            sum += a[i*n+j] * b[j];
        }
        c[i] = sum;
    }
}

int main() {
    srand(time(NULL));

    int n = 3;
    std::vector<double> matrix(n*n);
    std::vector<double> vec(n);
    std::vector<double>  result(n);

    for (int i = 0; i < n*n; i++ ) {
        matrix[i] = (double)rand()/RAND_MAX * 100;
    }

    for (int i = 0; i < n; i++ ) {
        vec[i] = (double)rand()/RAND_MAX * 100;
    }

    std::cout << "Matrix: " << std::endl;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            std::cout << matrix[i * n + j] << "\t";
        }
        std::cout << std::endl;
    }

    std::cout << "\nVector: " << std::endl;

    for (int i = 0; i < n; i++) {
        std::cout << vec[i] << "\t";
    }

    auto start = std::chrono::high_resolution_clock::now();

    multiply_sequential(matrix, vec, result, n);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    std::cout << "\n\nTime: " << elapsed.count() << "ns, Result[0]: " << result[0] << std::endl;

    return 0;
}