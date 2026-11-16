
#include <iostream>
#include <vector>
#include <chrono>
#include <exception>
#include <iomanip>
#include <stdexcept>

#include "vector_database.h"
#include "vector_store.h"

using namespace std;

int main(int argc, char* argv[])
{
    try
    {
        // ----------------------------------------------------
        // Document ingestion mode:
        //   vector_db.exe --ingest ingestion_vectors.bin ingestion_metadata.jsonl
        // ----------------------------------------------------
        if (argc == 4 && string(argv[1]) == "--ingest")
        {
            VectorDatabase ingestionDb(
                "vectors.bin",
                "hnsw.index",
                10000,
                16,
                100,
                50,
                "metadata.bin"
            );

            ingestionDb.initialize();
            const uint32_t firstNewId = ingestionDb.size();

            const uint32_t added = ingestionDb.ingest(argv[2], argv[3]);
            cout << "New chunks added: " << added << "\n";
            cout << "Current corpus size: " << ingestionDb.size() << "\n";
            cout << "Current HNSW node count should match corpus size.\n";

            if (added > 0)
            {
                const vector<float>& query = ingestionDb.getVector(firstNewId);
                vector<SearchResult> results = ingestionDb.search(query, 3, SearchFilter());

                cout << "\nSearch results for the newly ingested chunk:\n";
                for (size_t i = 0; i < results.size(); ++i)
                {
                    cout << "Vector ID: " << results[i].id
                         << " | Distance: " << results[i].distance << "\n";
                    if (results[i].hasMetadata)
                    {
                        cout << "Title: " << results[i].metadata.title << "\n";
                        cout << "Source: " << results[i].metadata.source << "\n";
                        cout << "Document ID: " << results[i].metadata.documentId << "\n";
                        cout << "Chunk index: " << results[i].metadata.chunkIndex << "\n";
                        cout << "Text: " << results[i].metadata.text << "\n";
                    }
                    cout << "----------------------------------------\n";
                }
            }
            return 0;
        }

        // ----------------------------------------------------
        // 1. Initialize the vector database.
        // ----------------------------------------------------

        VectorDatabase db(
            "vectors.bin",
            "hnsw.index",
            10000,
            16,
            100,
            50,
            "metadata.bin"
        );

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


        // ----------------------------------------------------
        // 2. Display database information.
        // ----------------------------------------------------

        cout << "\nDatabase information:\n";

        cout << "Total corpus vectors: "
             << db.size() << "\n";

        cout << "Vector dimension: "
             << db.dimension() << "\n";

        cout << "Metadata records: "
             << db.metadataCount() << "\n";


        // ----------------------------------------------------
        // 3. Load held-out evaluation queries separately.
        //
        // These vectors are NOT part of the HNSW index.
        // ----------------------------------------------------

        VectorStore queryStore("queries.bin");

        if (queryStore.dim() != db.dimension())
        {
            throw runtime_error(
                "Query dimension does not match corpus dimension"
            );
        }

        cout << "Evaluation query vectors: "
             << queryStore.size() << "\n";

        cout << "Query dimension: "
             << queryStore.dim() << "\n";


        // Use the first held-out query for demonstrations.
        const vector<float>& query =
            queryStore.getVector(0);


        // ----------------------------------------------------
        // 4. Perform normal top-10 vector search.
        // ----------------------------------------------------

        vector<int> initialResults =
            db.search(query, 10);

        cout << "\nUnfiltered search results:\n";

        for (size_t i = 0; i < initialResults.size(); i++)
        {
            cout << initialResults[i] << "\n";
        }


        // ----------------------------------------------------
        // 5. Assign synthetic metadata for demonstration.
        //
        // This is demonstration metadata, not metadata
        // extracted from actual documents.
        // ----------------------------------------------------

        for (size_t i = 0; i < initialResults.size(); i++)
        {
            Metadata metadata;

            metadata.title =
                "Demo Document " +
                to_string(initialResults[i]);

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


        // ----------------------------------------------------
        // 6. Search using a category filter.
        // ----------------------------------------------------

        SearchFilter filter;

        filter.category = "Technology";

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


        // ----------------------------------------------------
        // 7. Multiple metadata filters.
        // ----------------------------------------------------

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


        // ----------------------------------------------------
        // 8. Source-only filter.
        // ----------------------------------------------------

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


        // ----------------------------------------------------
        // 9. No-match filter test.
        // ----------------------------------------------------

        SearchFilter noMatchFilter;

        noMatchFilter.category = "NonExistentCategory";

        vector<SearchResult> noMatchResults =
            db.search(query, 5, noMatchFilter);

        cout << "\nNo-match filter results:\n";

        cout << "Results returned: "
             << noMatchResults.size() << "\n";


        // ----------------------------------------------------
        // 10. Fewer-than-K results test.
        // ----------------------------------------------------

        SearchFilter limitedFilter;

        limitedFilter.source = "Example Dataset";

        vector<SearchResult> limitedResults =
            db.search(query, 5, limitedFilter);

        cout << "\nFewer-than-K filter test:\n";

        cout << "Requested: 5\n";
        cout << "Returned: "
             << limitedResults.size() << "\n";

        for (const SearchResult& result : limitedResults)
        {
            cout << result.id << " | "
                 << result.metadata.source << "\n";
        }


        // ----------------------------------------------------
        // 11. Empty-filter test.
        // ----------------------------------------------------

        SearchFilter emptyFilter;

        vector<SearchResult> emptyFilterResults =
            db.search(query, 10, emptyFilter);

        cout << "\nEmpty-filter test:\n";

        for (const SearchResult& result : emptyFilterResults)
        {
            cout << result.id << "\n";
        }


        // ----------------------------------------------------
        // 12. Missing-metadata validation.
        // ----------------------------------------------------

        SearchFilter missingMetadataFilter;

        missingMetadataFilter.category = "Technology";

        vector<SearchResult> metadataResults =
            db.search(query, 10, missingMetadataFilter);

        cout << "\nMissing-metadata validation:\n";

        for (const SearchResult& result : metadataResults)
        {
            cout << result.id
                 << " | Has metadata: "
                 << (db.hasMetadata(result.id) ? "Yes" : "No")
                 << "\n";
        }


        // ----------------------------------------------------
        // 13. Persist index and metadata.
        // ----------------------------------------------------

        db.save();

        cout << "\nMetadata record count: "
             << db.metadataCount() << "\n";

        cout << "\nDatabase execution completed.\n";
    }
    catch (const exception& e)
    {
        cerr << "\nERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
