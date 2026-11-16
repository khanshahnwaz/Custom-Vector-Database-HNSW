
#include "vector_database.h"
#include "distance.h"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <limits>
#include <algorithm>

using namespace std;


// Check whether a file exists.
static bool fileExists(const string& filename)
{
    ifstream file(filename.c_str(), ios::binary);
    return file.good();
}


// Check whether a metadata record satisfies all filters.
static bool matchesFilter(
    const Metadata& metadata,
    const SearchFilter& filter
)
{
    if (!filter.title.empty() &&
        metadata.title != filter.title)
        return false;

    if (!filter.category.empty() &&
        metadata.category != filter.category)
        return false;

    if (!filter.source.empty() &&
        metadata.source != filter.source)
        return false;

    if (!filter.documentId.empty() &&
        metadata.documentId != filter.documentId)
        return false;

    return true;
}


// Check whether at least one filter is specified.
static bool hasActiveFilter(const SearchFilter& filter)
{
    return !filter.title.empty() ||
           !filter.category.empty() ||
           !filter.source.empty() ||
           !filter.documentId.empty();
}


// Constructor.
VectorDatabase::VectorDatabase(
    const string& vectorFilename,
    const string& indexFilename,
    uint32_t indexedVectorCount,
    int M,
    int efConstruction,
    int efSearch,
    const string& metadataFilename
)
    : store(vectorFilename),
      index(store, M, efConstruction),
      indexFilename(indexFilename),
      metadataFilename(metadataFilename),
      indexedVectorCount(indexedVectorCount),
      efSearch(efSearch),
      initialized(false),
      initializationAttempted(false)
{
    if (indexedVectorCount == 0 ||
        indexedVectorCount > store.size())
    {
        throw invalid_argument(
            "Invalid number of vectors to index"
        );
    }

    if (indexedVectorCount >
        static_cast<uint32_t>(numeric_limits<int>::max()))
    {
        throw invalid_argument(
            "Vector count exceeds supported node ID range"
        );
    }

    if (M <= 0 || efConstruction <= 0 || efSearch <= 0)
    {
        throw invalid_argument(
            "HNSW parameters must be positive"
        );
    }
}


// Initialize database.
void VectorDatabase::initialize()
{
    if (initialized)
    {
        cout << "Database is already initialized.\n";
        return;
    }

    if (initializationAttempted)
    {
        throw logic_error(
            "Initialization was already attempted. "
            "Create a new VectorDatabase object."
        );
    }

    initializationAttempted = true;

    // Load or build HNSW.
    if (fileExists(indexFilename))
    {
        cout << "Existing HNSW index found.\n";
        cout << "Loading index...\n";

        index.loadIndex(indexFilename);

        if (index.getNodeCount() !=
            static_cast<int>(indexedVectorCount))
        {
            throw runtime_error(
                "Loaded index node count does not match "
                "configured indexed vector count"
            );
        }

        cout << "Index loaded successfully.\n";
    }
    else
    {
        cout << "No existing index found.\n";
        cout << "Building HNSW index...\n";

        for (uint32_t id = 0; id < indexedVectorCount; id++)
        {
            index.insert(static_cast<int>(id));
        }

        if (index.getNodeCount() !=
            static_cast<int>(indexedVectorCount))
        {
            throw runtime_error(
                "Constructed index has an unexpected node count"
            );
        }

        cout << "Index construction completed.\n";

        index.saveIndex(indexFilename);
    }

    index.validateGraph();

    // Load metadata.
    if (fileExists(metadataFilename))
    {
        metadataStore.load(
            metadataFilename,
            store.size()
        );
    }
    else
    {
        cout << "No metadata file found. "
             << "Starting with empty metadata storage.\n";
    }

    initialized = true;

    cout << "VectorDatabase is ready.\n";
}


// Existing unfiltered search.
vector<int> VectorDatabase::search(
    const vector<float>& query,
    int k
)
{
    if (!initialized)
    {
        throw logic_error(
            "Database must be initialized before searching"
        );
    }

    if (query.size() != store.dim())
    {
        throw invalid_argument(
            "Query dimension does not match vector dimension"
        );
    }

    if (k <= 0)
    {
        throw invalid_argument(
            "k must be positive"
        );
    }

    return index.search(query, k, efSearch);
}


// New filtered search.
vector<SearchResult> VectorDatabase::search(
    const vector<float>& query,
    int k,
    const SearchFilter& filter
)
{
    if (!initialized)
    {
        throw logic_error(
            "Database must be initialized before searching"
        );
    }

    if (query.size() != store.dim())
    {
        throw invalid_argument(
            "Query dimension does not match vector dimension"
        );
    }

    if (k <= 0)
    {
        throw invalid_argument(
            "k must be positive"
        );
    }

    bool filtering = hasActiveFilter(filter);

    // Initial candidate count.
    int candidateCount;

    if (filtering)
    {
        long long desiredCount = max(
            static_cast<long long>(k) * 10,
            static_cast<long long>(efSearch)
        );

        candidateCount = static_cast<int>(
            min(
                static_cast<long long>(indexedVectorCount),
                desiredCount
            )
        );
    }
    else
    {
        candidateCount = k;
    }

    vector<SearchResult> results;

    while (true)
    {
        // When filtering, increase efSearch to allow
        // HNSW to return the larger candidate pool.
        int currentEfSearch = filtering
            ? max(efSearch, candidateCount)
            : efSearch;

        vector<int> candidates = index.search(
            query,
            candidateCount,
            currentEfSearch
        );

        results.clear();

        // Examine retrieved candidates.
        for (size_t i = 0; i < candidates.size(); i++)
        {
            int id = candidates[i];

            bool metadataExists =
                metadataStore.contains(
                    static_cast<uint32_t>(id)
                );

            // If filters are active, missing metadata
            // cannot satisfy the filter.
            if (filtering && !metadataExists)
            {
                continue;
            }

            Metadata metadata;

            if (metadataExists)
            {
                metadata = metadataStore.get(
                    static_cast<uint32_t>(id)
                );
            }

            // Apply metadata conditions.
            if (filtering &&
                !matchesFilter(metadata, filter))
            {
                continue;
            }

            SearchResult result;

            result.id = static_cast<uint32_t>(id);

            result.distance = cosineDistance(
                query,
                store.getVector(
                    static_cast<uint32_t>(id)
                )
            );

            result.hasMetadata = metadataExists;
            result.metadata = metadata;

            results.push_back(result);
        }

        // Sort by ascending cosine distance.
        sort(
            results.begin(),
            results.end(),
            [](const SearchResult& a, const SearchResult& b)
            {
                return a.distance < b.distance;
            }
        );

        // Stop if enough results were found.
        if (static_cast<int>(results.size()) >= k)
        {
            break;
        }

        // Stop if all configured index candidates
        // have been requested.
        if (!filtering ||
            candidateCount >= static_cast<int>(indexedVectorCount))
        {
            break;
        }

        // Progressively increase candidate count.
        long long nextCount =
            static_cast<long long>(candidateCount) * 2;

        candidateCount = static_cast<int>(
            min(
                static_cast<long long>(indexedVectorCount),
                nextCount
            )
        );
    }

    // Return no more than k results.
    if (static_cast<int>(results.size()) > k)
    {
        results.resize(k);
    }

    return results;
}


// Save HNSW and metadata.
void VectorDatabase::save() const
{
    if (!initialized)
    {
        throw logic_error(
            "Database must be initialized before saving"
        );
    }

    index.saveIndex(indexFilename);

    metadataStore.save(metadataFilename);

    cout << "VectorDatabase saved successfully.\n";
}


// Retrieve vector.
const vector<float>& VectorDatabase::getVector(uint32_t id) const
{
    return store.getVector(id);
}


// Insert or update metadata.
void VectorDatabase::setMetadata(
    uint32_t id,
    const Metadata& metadata
)
{
    if (!initialized)
    {
        throw logic_error(
            "Database must be initialized before setting metadata"
        );
    }

    if (id >= store.size())
    {
        throw out_of_range(
            "Vector ID does not exist"
        );
    }

    metadataStore.upsert(id, metadata);
}


// Retrieve metadata.
const Metadata& VectorDatabase::getMetadata(uint32_t id) const
{
    if (!initialized)
    {
        throw logic_error(
            "Database must be initialized before retrieving metadata"
        );
    }

    return metadataStore.get(id);
}


// Check metadata existence.
bool VectorDatabase::hasMetadata(uint32_t id) const
{
    if (!initialized)
    {
        throw logic_error(
            "Database must be initialized before checking metadata"
        );
    }

    return metadataStore.contains(id);
}


// Metadata record count.
uint32_t VectorDatabase::metadataCount() const
{
    return metadataStore.size();
}


// Dataset size.
uint32_t VectorDatabase::size() const
{
    return store.size();
}


// Vector dimension.
uint32_t VectorDatabase::dimension() const
{
    return store.dim();
}


// Initialization status.
bool VectorDatabase::isInitialized() const
{
    return initialized;
}
