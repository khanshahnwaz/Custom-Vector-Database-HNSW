
#include <bits/stdc++.h>
#include "vector_store.h"
#include "hnsw.h"

using namespace std;

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 2)
        {
            cout << "Usage:\n";
            cout << "  main.exe build\n";
            cout << "  main.exe load\n";
            return 0;
        }

        VectorStore store("vectors.bin");

        string mode = argv[1];

        if (mode == "build")
        {
            cout << "Building HNSW index...\n";

            HNSW index(store, 16, 100);

            for (int id = 0; id < 1000; id++)
            {
                index.insert(id);
            }

            index.validateGraph();

            index.saveIndex("hnsw.index");
        }
        else if (mode == "load")
        {
            cout << "Loading existing HNSW index...\n";

            HNSW index(store);

            index.loadIndex("hnsw.index");

            index.validateGraph();

            // Query vector is outside the indexed range.
            const vector<float>& query =
                store.getVector(5000);

            auto start = chrono::high_resolution_clock::now();

            vector<int> results =
                index.search(query, 10, 50);

            auto end = chrono::high_resolution_clock::now();

            auto latency =
                chrono::duration_cast<chrono::microseconds>(
                    end - start
                ).count();

            cout << "\nSearch results:\n";

            for (int id : results)
            {
                cout << id << " ";
            }

            cout << "\nSearch latency: "
                 << latency << " microseconds\n";
        }
        else
        {
            cout << "Invalid mode. Use build or load.\n";
            return 1;
        }
    }
    catch (const exception& e)
    {
        cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
