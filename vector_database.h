#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "vector_store.h"
#include "hnsw.h"
#include "metadata_store.h"
#include "search_types.h"

using namespace std;

class VectorDatabase
{
private:
    // Keep this order: HNSW stores a reference to VectorStore.
    VectorStore store;
    HNSW index;
    MetadataStore metadataStore;

    string vectorFilename;
    string indexFilename;
    string metadataFilename;

    int efSearch;
    bool initialized;
    bool initializationAttempted;

public:
    VectorDatabase(
        const string& vectorFilename,
        const string& indexFilename,
        uint32_t initialVectorCount,
        int M = 16,
        int efConstruction = 100,
        int efSearch = 50,
        const string& metadataFilename = "metadata.bin"
    );

    void initialize();

    // Ingests a vector batch and matching JSONL metadata batch.
    // New IDs are assigned sequentially after the existing corpus.
    // The operation persists the updated vector store, metadata, and index.
    uint32_t ingest(
        const string& embeddingFilename,
        const string& metadataJsonlFilename
    );

    vector<int> search(const vector<float>& query, int k);
    vector<SearchResult> search(
        const vector<float>& query,
        int k,
        const SearchFilter& filter
    );

    void save() const;

    const vector<float>& getVector(uint32_t id) const;
    void setMetadata(uint32_t id, const Metadata& metadata);
    const Metadata& getMetadata(uint32_t id) const;
    bool hasMetadata(uint32_t id) const;
    uint32_t metadataCount() const;
    uint32_t size() const;
    uint32_t dimension() const;
    bool isInitialized() const;
};
