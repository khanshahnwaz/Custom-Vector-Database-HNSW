
#include <bits/stdc++.h>
#include "vector_store.h"
#include "hnsw.h"

using namespace std;

int main()
{
    try
    {
        VectorStore store("vectors.bin");

        cout << "Building a fresh HNSW index...\n";

        HNSW index(store, 16, 100);

        for (int id = 0; id < 1000; id++)
        {
            index.insert(id);
        }

        index.validateGraph();

        cout << "\nSaving index over the existing destination...\n";

        index.saveIndex("hnsw.index");

        cout << "\nReplacement test completed.\n";
    }
    catch (const exception& e)
    {
        cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
