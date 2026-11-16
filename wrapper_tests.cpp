
#include <iostream>
#include <vector>
#include <cstdio>
#include <stdexcept>

#include "vector_database.h"

using namespace std;

int main()
{
    try
    {
        cout << "========== VECTOR DATABASE TESTS ==========\n\n";

        // TEST 1: Repeated initialization
        cout << "TEST 1: Repeated initialization\n";

        VectorDatabase db(
            "vectors.bin",
            "hnsw.index",
            10000,
            16,
            100
        );

        db.initialize();
        db.initialize();

        cout << "PASSED: Repeated initialization handled.\n\n";


        // TEST 2: Wrong query dimension
        cout << "TEST 2: Wrong query dimension\n";

        vector<float> wrongQuery(db.dimension() - 1, 0.0f);

        try
        {
            db.search(wrongQuery, 10, 50);

            cout << "FAILED: Invalid query was accepted.\n";
        }
        catch (const invalid_argument& e)
        {
            cout << "PASSED: Invalid query rejected.\n";
            cout << "Reason: " << e.what() << "\n";
        }

        cout << "\n";


        // TEST 3: Index size mismatch
        cout << "TEST 3: Index size mismatch\n";

        VectorStore store("vectors.bin");

        HNSW smallIndex(store, 16, 100);

        // Build a separate index containing only 1,000 vectors.
        for (int id = 0; id < 1000; id++)
        {
            smallIndex.insert(id);
        }

        smallIndex.saveIndex("small_test.index");

        cout << "Created test index with 1,000 nodes.\n";

        try
        {
            VectorDatabase badDb(
                "vectors.bin",
                "small_test.index",
                10000,
                16,
                100
            );

            badDb.initialize();

            cout << "FAILED: Incorrect index size was accepted.\n";
        }
        catch (const runtime_error& e)
        {
            cout << "PASSED: Incorrect index size rejected.\n";
            cout << "Reason: " << e.what() << "\n";
        }

        // Remove the temporary test index.
        remove("small_test.index");

        cout << "\n========== TESTS COMPLETED ==========\n";
    }
    catch (const exception& e)
    {
        cerr << "\nTEST PROGRAM ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
