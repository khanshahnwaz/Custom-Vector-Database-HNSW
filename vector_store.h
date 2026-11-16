#pragma once

#include <vector>
#include <string>
#include <cstdint>

using namespace std;

class VectorStore
{
private:
    uint32_t numVectors;
    uint32_t dimension;
    vector<vector<float> > vectors;

public:
    explicit VectorStore(const string& filename);

    const vector<float>& getVector(uint32_t id) const;

    uint32_t size() const;
    uint32_t dim() const;

    // Adds one vector after validating its dimension and values.
    // The stored vector is normalized, just like vectors loaded from disk.
    uint32_t append(const vector<float>& vector);

    // Persists the complete vector store using the existing binary format.
    void save(const string& filename) const;
};
