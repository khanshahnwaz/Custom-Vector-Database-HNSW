#include <bits/stdc++.h>
using namespace std;

int main() {

    ifstream fin("vectors.bin", ios::binary);

    if (!fin) {
        cerr << "Could not open vectors.bin\n";
        return 1;
    }

    uint32_t numVectors;
    uint32_t dimension;

    // Read header
    fin.read(reinterpret_cast<char*>(&numVectors), sizeof(numVectors));
    fin.read(reinterpret_cast<char*>(&dimension), sizeof(dimension));

    cout << "Number of vectors: " << numVectors << '\n';
    cout << "Dimension: " << dimension << '\n';

    // Read first vector
    vector<float> embedding(dimension);

    fin.read(
        reinterpret_cast<char*>(embedding.data()),
        dimension * sizeof(float)
    );

    cout << "\nFirst vector:\n";

    for (int i = 0; i < 10; i++) {
        cout << embedding[i] << ' ';
    }

    cout << '\n';

    return 0;
}