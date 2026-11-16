
#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "vector_store.h"
#include "hnsw.h"
#include "metadata_store.h"

using namespace std;

class VectorDatabase
{
private:
    VectorStore store;
    HNSW index;
    MetadataStore metadataStore;

    string indexFilename;
    string metadataFilename;

    uint32_t indexedVectorCount;

    int efSearch;

    bool initialized;
    bool initializationAttempted;

public:

    VectorDatabase(
        const string& vectorFilename,
        const string& indexFilename,
        uint32_t indexedVectorCount,
        int M = 16,
        int efConstruction = 100,
        int efSearch = 50,
        const string& metadataFilename = "metadata.bin"
    );

    void initialize();

    vector<int> search(
        const vector<float>& query,
        int k
    );

    void save() const;

    const vector<float>& getVector(uint32_t id) const;

    // Metadata operations.
    void setMetadata(
        uint32_t id,
        const Metadata& metadata
    );

    const Metadata& getMetadata(uint32_t id) const;

    bool hasMetadata(uint32_t id) const;

    uint32_t metadataCount() const;

    uint32_t size() const;

    uint32_t dimension() const;

    bool isInitialized() const;
};
