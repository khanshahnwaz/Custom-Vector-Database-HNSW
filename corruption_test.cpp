
#include <bits/stdc++.h>
#include "vector_store.h"
#include "hnsw.h"

using namespace std;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        cout << "Usage: corruption_test.exe <index_file>\n";
        return 1;
    }

    string filename = argv[1];

    try
    {
        VectorStore store("vectors.bin");

        HNSW index(store);

        cout << "Testing index: " << filename << "\n";

        index.loadIndex(filename);

        index.validateGraph();

        cout << "\nRESULT: Index was ACCEPTED.\n";

        return 0;
    }
    catch (const exception& e)
    {
        cout << "\nRESULT: Index was REJECTED.\n";
        cout << "Reason: " << e.what() << '\n';

        return 2;
    }
}
