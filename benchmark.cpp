#include <bits/stdc++.h>

#include "vector_store.h"
#include "distance.h"
#include "hnsw.h"
#include "brute_force.h"

using namespace std;


float calculateRecall(
    const vector<int>& groundTruth,
    const vector<int>& result,
    int k)
{
    unordered_set<int> truth(
        groundTruth.begin(),
        groundTruth.end()
    );

    int common = 0;

    for (int id : result)
    {
        if (truth.count(id))
        {
            common++;
        }
    }

    return static_cast<float>(common) / k;
}


#include <bits/stdc++.h>

#include "vector_store.h"
#include "hnsw.h"

using namespace std;


// ============================================================
// MAIN
// ============================================================
int main()
{
    // --------------------------------------------------------
    // Benchmark configuration
    // --------------------------------------------------------

    // Number of vectors inserted into the HNSW index.
    const int numberOfVectors = 10000;

    // Number of query vectors used for evaluation.
    const int numberOfQueries = 100;

    // Number of nearest neighbors requested.
    const int k = 10;

    // --------------------------------------------------------
    // Query vectors are stored after the indexed vectors.
    //
    // Indexed vectors:
    //     0 ... 9999
    //
    // Query vectors:
    //     10000 ... 10099
    //
    // The query vectors are NOT inserted into HNSW.
    // --------------------------------------------------------
    const int queryStartId =
        numberOfVectors;


    // --------------------------------------------------------
    // efSearch values we want to compare.
    //
    // Higher efSearch:
    //     More graph exploration
    //     Higher recall
    //     Higher search latency
    // --------------------------------------------------------
    const vector<int> efSearchValues = {
        10,
        20,
        50,
        100
    };


    // ========================================================
    // 1. LOAD VECTOR STORE
    // ========================================================

    VectorStore store("vectors.bin");

    cout << "Number of vectors in bin: "
         << store.size()
         << endl;


    // --------------------------------------------------------
    // Make sure the binary file contains enough vectors for
    // both the indexed dataset and the query dataset.
    // --------------------------------------------------------
    if (store.size() <
        numberOfVectors + numberOfQueries)
    {
        cerr << "Error: Not enough vectors in vectors.bin."
             << endl;

        return 1;
    }


    // ========================================================
    // 2. BUILD HNSW INDEX
    // ========================================================

    cout << "\nBuilding HNSW...\n";


    HNSW hnsw(store);


    auto constructionStart =
        chrono::high_resolution_clock::now();


    // --------------------------------------------------------
    // Insert only the indexed vectors.
    //
    // Query vectors are intentionally excluded.
    // --------------------------------------------------------
    for (int id = 0;
         id < numberOfVectors;
         id++)
    {
        hnsw.insert(id);
    }


    auto constructionEnd =
        chrono::high_resolution_clock::now();


    auto constructionTime =
        chrono::duration_cast<
            chrono::milliseconds
        >(
            constructionEnd -
            constructionStart
        ).count();


    cout << "HNSW construction time: "
         << constructionTime
         << " ms\n";


    // --------------------------------------------------------
    // Print construction statistics.
    // --------------------------------------------------------
    hnsw.printConstructionStats();


    // ========================================================
    // 3. PREPARE QUERY IDS
    // ========================================================

    vector<int> queryIds;

    queryIds.reserve(numberOfQueries);


    for (int i = 0;
         i < numberOfQueries;
         i++)
    {
        queryIds.push_back(
            queryStartId + i
        );
    }


    // ========================================================
    // 4. CALCULATE GROUND TRUTH
    // ========================================================
    //
    // Ground truth is calculated using brute force.
    //
    // This is done ONLY ONCE for every query because the
    // exact nearest neighbors do not depend on efSearch.
    //
    // We will reuse these results for efSearch = 10, 20,
    // 50 and 100.
    //
    // ========================================================

    cout << "\nCalculating ground truth...\n";


    vector<vector<int>> groundTruths;

    groundTruths.reserve(numberOfQueries);


    for (int queryId : queryIds)
    {
        const vector<float>& query =
            store.getVector(queryId);


        vector<int> groundTruth =
            bruteForceKNN(
                store,
                query,
                k,
                numberOfVectors
            );


        groundTruths.push_back(
            groundTruth
        );
    }


    cout << "Ground truth calculated for "
         << numberOfQueries
         << " queries.\n";


    // ========================================================
    // 5. MEASURE BRUTE FORCE BASELINE
    // ========================================================
    //
    // We measure brute force separately because speedup is:
    //
    //     brute force latency
    //     -------------------
    //       HNSW latency
    //
    // ========================================================

    long long totalBruteForceLatency = 0;

    long long minimumBruteForceLatency =
        LLONG_MAX;

    long long maximumBruteForceLatency = 0;


    for (int queryId : queryIds)
    {
        const vector<float>& query =
            store.getVector(queryId);


        auto start =
            chrono::high_resolution_clock::now();


        // Run exact search again only for timing.
        bruteForceKNN(
            store,
            query,
            k,
            numberOfVectors
        );


        auto end =
            chrono::high_resolution_clock::now();


        auto latency =
            chrono::duration_cast<
                chrono::microseconds
            >(
                end - start
            ).count();


        totalBruteForceLatency +=
            latency;


        minimumBruteForceLatency =
            min(
                minimumBruteForceLatency,
                latency
            );


        maximumBruteForceLatency =
            max(
                maximumBruteForceLatency,
                latency
            );
    }


    double averageBruteForceLatency =
        static_cast<double>(
            totalBruteForceLatency
        ) / numberOfQueries;


    // ========================================================
    // 6. PRINT BENCHMARK HEADER
    // ========================================================

    cout << "\n";
    cout << "========================================\n";
    cout << "        HNSW efSearch Benchmark\n";
    cout << "========================================\n";

    cout << "Vectors: "
         << numberOfVectors
         << "\n";

    cout << "Queries: "
         << numberOfQueries
         << "\n";

    cout << "K: "
         << k
         << "\n";


    // ========================================================
    // 7. BRUTE FORCE BASELINE
    // ========================================================

    cout << "\n---------- Brute Force ----------\n";

    cout << "Average latency: "
         << averageBruteForceLatency
         << " microseconds\n";

    cout << "Minimum latency: "
         << minimumBruteForceLatency
         << " microseconds\n";

    cout << "Maximum latency: "
         << maximumBruteForceLatency
         << " microseconds\n";

    cout << "Average latency: "
         << averageBruteForceLatency / 1000.0
         << " ms\n";


    // ========================================================
    // 8. RUN HNSW FOR DIFFERENT efSearch VALUES
    // ========================================================
    //
    // IMPORTANT:
    //
    // The HNSW graph is NOT rebuilt here.
    //
    // We use the same graph for every efSearch value.
    //
    // Only the amount of exploration during search changes.
    //
    // ========================================================

    for (int efSearch : efSearchValues)
    {
        long long totalHNSWLatency = 0;

        long long minimumHNSWLatency =
            LLONG_MAX;

        long long maximumHNSWLatency = 0;

        double totalRecall = 0.0;


        // ----------------------------------------------------
        // Run every query.
        // ----------------------------------------------------
        for (int i = 0;
             i < numberOfQueries;
             i++)
        {
            int queryId =
                queryIds[i];


            const vector<float>& query =
                store.getVector(queryId);


            // =================================================
            // HNSW SEARCH
            // =================================================

            auto hnswStart =
                chrono::high_resolution_clock::now();


            vector<int> result =
                hnsw.search(
                    query,
                    k,
                    efSearch
                );


            auto hnswEnd =
                chrono::high_resolution_clock::now();


            auto hnswLatency =
                chrono::duration_cast<
                    chrono::microseconds
                >(
                    hnswEnd -
                    hnswStart
                ).count();


            totalHNSWLatency +=
                hnswLatency;


            minimumHNSWLatency =
                min(
                    minimumHNSWLatency,
                    hnswLatency
                );


            maximumHNSWLatency =
                max(
                    maximumHNSWLatency,
                    hnswLatency
                );


            // =================================================
            // RECALL
            // =================================================

            float recall =
                calculateRecall(
                    groundTruths[i],
                    result,
                    k
                );


            totalRecall +=
                recall;
        }


        // ----------------------------------------------------
        // Calculate average HNSW latency.
        // ----------------------------------------------------
        double averageHNSWLatency =
            static_cast<double>(
                totalHNSWLatency
            ) / numberOfQueries;


        // ----------------------------------------------------
        // Calculate average recall.
        // ----------------------------------------------------
        double averageRecall =
            totalRecall /
            numberOfQueries;


        // ----------------------------------------------------
        // Calculate speedup.
        // ----------------------------------------------------
        double speedup =
            averageBruteForceLatency /
            averageHNSWLatency;


        // =====================================================
        // PRINT RESULTS FOR THIS efSearch
        // =====================================================

        cout << "\n----------------------------------------\n";

        cout << "efSearch: "
             << efSearch
             << "\n";

        cout << "----------------------------------------\n";


        cout << "Average HNSW latency: "
             << averageHNSWLatency
             << " microseconds\n";


        cout << "Minimum latency: "
             << minimumHNSWLatency
             << " microseconds\n";


        cout << "Maximum latency: "
             << maximumHNSWLatency
             << " microseconds\n";


        cout << "Average HNSW latency: "
             << averageHNSWLatency / 1000.0
             << " ms\n";


        cout << "Recall@"
             << k
             << ": "
             << averageRecall
             << "\n";


        cout << "Speedup: "
             << speedup
             << "x\n";
    }


    // ========================================================
    // 9. OPTIONAL GRAPH VALIDATION
    // ========================================================
    //
    // Run this when you want to verify graph correctness.
    //
    // It is intentionally disabled during normal benchmarking
    // because validation itself adds extra work.
    //
    // ========================================================

    // hnsw.validateGraph();


    return 0;
}