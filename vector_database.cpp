
#include "vector_database.h"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <limits>

using namespace std;


static bool fileExists(const string& filename)
{
    ifstream file(filename.c_str(), ios::binary);
    return file.good();
}


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
                "the configured indexed vector count"
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

    // Load metadata if the file exists.
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


// Search.
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


// Save both HNSW and metadata.
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


// Number of metadata records.
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
