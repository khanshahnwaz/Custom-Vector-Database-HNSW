
#include <iostream>
#include <vector>
#include <chrono>
#include <exception>
#include <iomanip>

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
            "metadata.bin"
        );

        // Initialize.
        auto startupStart =
            chrono::high_resolution_clock::now();

        db.initialize();

        auto startupEnd =
            chrono::high_resolution_clock::now();

        auto startupTime =
            chrono::duration_cast<chrono::milliseconds>(
                startupEnd - startupStart
            ).count();

        cout << "\nStartup time: "
             << startupTime << " milliseconds\n";


        cout << "\nDatabase information:\n";

        cout << "Total vectors: "
             << db.size() << "\n";

        cout << "Vector dimension: "
             << db.dimension() << "\n";

        cout << "Metadata records: "
             << db.metadataCount() << "\n";


        // Query vector.
        const vector<float>& query = db.getVector(10000);


        // First retrieve the normal top 10.
        vector<int> initialResults = db.search(query, 10);

        cout << "\nUnfiltered search results:\n";

        for (size_t i = 0; i < initialResults.size(); i++)
        {
            cout << initialResults[i] << "\n";
        }


        // Assign synthetic metadata for demonstration.
        // First three candidates belong to Technology.
        // Remaining candidates belong to Science.
        for (size_t i = 0; i < initialResults.size(); i++)
        {
            Metadata metadata;

            metadata.title =
                "Demo Document " + to_string(initialResults[i]);

            metadata.category =
                (i < 3) ? "Technology" : "Science";

            metadata.source = "Demo Dataset";

            metadata.documentId =
                "DOC-" + to_string(initialResults[i]);

            db.setMetadata(
                static_cast<uint32_t>(initialResults[i]),
                metadata
            );
        }


        // Configure metadata filter.
        SearchFilter filter;

        filter.category = "Technology";


        // Search for three matching results.
        auto searchStart =
            chrono::high_resolution_clock::now();

        vector<SearchResult> results =
            db.search(query, 3, filter);

        auto searchEnd =
            chrono::high_resolution_clock::now();

        auto searchLatency =
            chrono::duration_cast<chrono::microseconds>(
                searchEnd - searchStart
            ).count();


        cout << "\nFiltered search results:\n";

        cout << "Filter: category = Technology\n\n";

        for (size_t i = 0; i < results.size(); i++)
        {
            cout << "Vector ID: "
                 << results[i].id << "\n";

            cout << "Distance: "
                 << fixed << setprecision(6)
                 << results[i].distance << "\n";

            if (results[i].hasMetadata)
            {
                cout << "Title: "
                     << results[i].metadata.title << "\n";

                cout << "Category: "
                     << results[i].metadata.category << "\n";

                cout << "Source: "
                     << results[i].metadata.source << "\n";
            }

            cout << "--------------------------\n";
        }

        cout << "\nFiltered search returned: "
             << results.size() << " results\n";

        cout << "Search latency: "
             << searchLatency << " microseconds\n";


        // Save metadata and index.
        db.save();

        cout << "\nMetadata record count: "
             << db.metadataCount() << "\n";





             SearchFilter combinedFilter;

combinedFilter.category = "Technology";
combinedFilter.source = "Demo Dataset";

vector<SearchResult> combinedResults =
    db.search(query, 3, combinedFilter);

cout << "\nMultiple-filter results:\n";

for (const SearchResult& result : combinedResults)
{
    cout << result.id << " | "
         << result.metadata.category << " | "
         << result.metadata.source << "\n";
}





SearchFilter sourceFilter;

sourceFilter.source = "Example Dataset";

vector<SearchResult> sourceResults =
    db.search(query, 5, sourceFilter);

cout << "\nSource-only filter results:\n";

for (const SearchResult& result : sourceResults)
{
    cout << result.id << " | "
         << result.metadata.source << "\n";
}



SearchFilter noMatchFilter;
noMatchFilter.category = "NonExistentCategory";

vector<SearchResult> noMatchResults =
    db.search(query, 5, noMatchFilter);

cout << "\nNo-match filter results:\n";
cout << "Results returned: " << noMatchResults.size() << "\n";





SearchFilter limitedFilter;
limitedFilter.source = "Example Dataset";

vector<SearchResult> limitedResults =
    db.search(query, 5, limitedFilter);

cout << "\nFewer-than-K filter test:\n";
cout << "Requested: 5\n";
cout << "Returned: " << limitedResults.size() << "\n";

for (const SearchResult& result : limitedResults) {
    cout << result.id << " | "
         << result.metadata.source << "\n";
}



SearchFilter emptyFilter;

vector<SearchResult> emptyFilterResults =
    db.search(query, 10, emptyFilter);

cout << "\nEmpty-filter test:\n";

for (const SearchResult& result : emptyFilterResults) {
    cout << result.id << "\n";
}




SearchFilter missingMetadataFilter;
missingMetadataFilter.category = "Technology";

vector<SearchResult> metadataResults =
    db.search(query, 10, missingMetadataFilter);

cout << "\nMissing-metadata validation:\n";

for (const SearchResult& result : metadataResults) {
    cout << result.id
         << " | Has metadata: "
         << (db.hasMetadata(result.id) ? "Yes" : "No")
         << "\n";
}
    }
    catch (const exception& e)
    {
        cerr << "\nERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
