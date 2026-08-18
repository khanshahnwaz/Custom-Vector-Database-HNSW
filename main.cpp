#include <bits/stdc++.h>
#include "vector_store.h"
#include "distance.h"
#include "brute_force.h"

using namespace std;
using namespace chrono;

int main()
{
    VectorStore store("vectors.bin");

    int k = 10;
    int numQueries = 100;

    long long totalTime = 0;
    long long minTime = LLONG_MAX;
    long long maxTime = 0;

    vector<vector<int>> groundTruth(numQueries);

for (int queryId = 0; queryId < numQueries; queryId++)
{
    const vector<float>& query = store.getVector(queryId);

    auto start = high_resolution_clock::now();

    vector<int> result =
        bruteForceKNN(store, query, k);

    auto end = high_resolution_clock::now();

    long long duration =
        duration_cast<microseconds>(end - start).count();

    totalTime += duration;
    minTime = min(minTime, duration);
    maxTime = max(maxTime, duration);

    // Save exact nearest neighbors
    groundTruth[queryId] = result;
}


cout << "\nGround truth for query 0:\n";

for (int id : groundTruth[0])
{
    cout << id << ' ';
}

cout << '\n';
    double average =
        static_cast<double>(totalTime) / numQueries;

    cout << "Brute Force Benchmark\n";
    cout << "---------------------\n";
    cout << "Vectors: " << store.size() << '\n';
    cout << "Dimension: " << store.dim() << '\n';
    cout << "Queries: " << numQueries << '\n';
    cout << "K: " << k << '\n';

    cout << "\nAverage latency: "
         << average << " microseconds\n";

    cout << "Minimum latency: "
         << minTime << " microseconds\n";

    cout << "Maximum latency: "
         << maxTime << " microseconds\n";

    cout << "Average latency: "
         << average / 1000.0 << " ms\n";

    return 0;
}