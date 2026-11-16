
#include <iostream>
#include <stdexcept>

#include "vector_database.h"

using namespace std;

int main()
{
    try
    {
        VectorDatabase db(
            "vectors.bin",
            "hnsw.index",
            10000,
            16,
            100,
            50,
            "metadata_corrupt.bin"
        );

        db.initialize();

        cout << "FAILED: Corrupted metadata was accepted.\n";
        return 1;
    }
    catch (const exception& e)
    {
        cout << "PASSED: Corrupted metadata rejected.\n";
        cout << "Reason: " << e.what() << "\n";
    }

    return 0;
}
