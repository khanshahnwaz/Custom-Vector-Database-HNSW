
#include <iostream>
#include <vector>
#include <chrono>
#include <exception>

#include "vector_database.h"

using namespace std;

int main()
{
    try
    {
        // Create database.
        VectorDatabase db(
            "vectors.bin",
            "hnsw.index",
            10000,
            16,              // M
            100,             // efConstruction
            50,              // efSearch
            "metadata.bin"
        );

        // Initialize database.
        auto startupStart =
            chrono::high_resolution_clock::now();

        db.initialize();


        try
{
    db.getMetadata(500);

    cout << "FAILED: Missing metadata was accepted.\n";
}
catch (const out_of_range& e)
{
    cout << "PASSED: Missing metadata rejected.\n";
    cout << e.what() << "\n";
}

        auto startupEnd =
            chrono::high_resolution_clock::now();

        auto startupTime =
            chrono::duration_cast<chrono::milliseconds>(
                startupEnd - startupStart
            ).count();

        cout << "\nStartup time: "
             << startupTime << " milliseconds\n";


        // Database information.
        cout << "\nDatabase information:\n";

        cout << "Total vectors: "
             << db.size() << "\n";

        cout << "Vector dimension: "
             << db.dimension() << "\n";

        cout << "Metadata records: "
             << db.metadataCount() << "\n";


        // Add metadata to an existing vector.
        // These are demonstration records, not actual
        // descriptions of the underlying embeddings.
        Metadata metadata;

        metadata.title = "Update Document 100";
        metadata.category = "Technology";
        metadata.source = "Example Dataset";
        metadata.documentId = "DOC-100";

        db.setMetadata(100, metadata);


        Metadata secondMetadata;

        secondMetadata.title = "Example Document 200";
        secondMetadata.category = "Science";
        secondMetadata.source = "Example Dataset";
        secondMetadata.documentId = "DOC-200";

        db.setMetadata(200, secondMetadata);


        // Retrieve metadata.
        cout << "\nMetadata for vector ID 100:\n";

        if (db.hasMetadata(100))
        {
            const Metadata& result = db.getMetadata(100);

            cout << "Title: " << result.title << "\n";
            cout << "Category: " << result.category << "\n";
            cout << "Source: " << result.source << "\n";
            cout << "Document ID: "
                 << result.documentId << "\n";
        }


        // Search using a held-out vector.
        const vector<float>& query = db.getVector(10000);

        auto searchStart =
            chrono::high_resolution_clock::now();

        vector<int> results = db.search(query, 10);

        auto searchEnd =
            chrono::high_resolution_clock::now();

        auto searchLatency =
            chrono::duration_cast<chrono::microseconds>(
                searchEnd - searchStart
            ).count();


        cout << "\nSearch results:\n";

        for (int id : results)
        {
            cout << id;

            if (db.hasMetadata(id))
            {
                const Metadata& metadata =
                    db.getMetadata(id);

                cout << " | " << metadata.title;
            }

            cout << "\n";
        }

        cout << "\nSearch latency: "
             << searchLatency << " microseconds\n";


        // Persist HNSW and metadata.
        db.save();

        cout << "\nMetadata record count: "
             << db.metadataCount() << "\n";
    }
    catch (const exception& e)
    {
        cerr << "\nERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
