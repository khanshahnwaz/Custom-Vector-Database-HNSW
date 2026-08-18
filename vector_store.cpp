#include "vector_store.h"

#include <fstream>
#include <stdexcept>
#include<math.h>

VectorStore::VectorStore(const string& filename)
{
    ifstream fin(filename, ios::binary);

    if (!fin)
    {
        throw runtime_error("Could not open vector file");
    }

    fin.read(
        reinterpret_cast<char*>(&numVectors),
        sizeof(numVectors)
    );

    fin.read(
        reinterpret_cast<char*>(&dimension),
        sizeof(dimension)
    );

    if (!fin)
    {
        throw runtime_error("Could not read vector file header");
    }

    vectors.resize(numVectors);

    for (uint32_t i = 0; i < numVectors; i++)
    {
        vectors[i].resize(dimension);

        fin.read(
            reinterpret_cast<char*>(vectors[i].data()),
            dimension * sizeof(float)
        );

        if (!fin)
        {
            throw runtime_error("Could not read vector data");
        }



        // ----------------------------------------------------
        // Calculate L2 norm.
        //
        // norm = sqrt(x1² + x2² + ... + xn²)
        // ----------------------------------------------------
        float norm = 0.0f;

        for (float value : vectors[i])
        {
            norm += value * value;
        }

        norm = sqrt(norm);

        if (norm == 0.0f)
        {
            throw runtime_error(
                "Encountered zero vector"
            );
        }

        // ----------------------------------------------------
        // Normalize vector.
        //
        // After this:
        //
        //     ||vectors[i]|| = 1
        //
        // Therefore cosine similarity between two vectors
        // becomes simply their dot product.
        // ----------------------------------------------------
        for (float& value : vectors[i])
        {
            value /= norm;
        }
        
    }




}


const vector<float>& VectorStore::getVector(uint32_t id) const
{
    if (id >= numVectors)
    {
        throw out_of_range("Vector ID out of range");
    }

    return vectors[id];
}


uint32_t VectorStore::size() const
{
    return numVectors;
}


uint32_t VectorStore::dim() const
{
    return dimension;
}

