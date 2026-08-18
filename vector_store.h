#pragma once

#include <vector>
#include <string>
#include <cstdint>

using namespace std;


// we try to optimize
// since dealing with 10k vectors, 15.36 MB
// we can load the vectors directly inside the RAM
// RAM will improve access speed 

class VectorStore
{
private:
    uint32_t numVectors;
    uint32_t dimension;

    vector<vector<float>> vectors;

public:
    VectorStore(const string& filename);

    const vector<float>& getVector(uint32_t id) const;

    uint32_t size() const;
    uint32_t dim() const;
};




// this implementation, causes opening of vectors.bin file every time getVector is called



// class VectorStore
// {
// private:

//     uint32_t numVectors;
//     uint32_t dimension;

//     string filename;

// public:

//     VectorStore(const string& filename);

//     vector<float> getVector(uint32_t id) const;

//     uint32_t size() const;
//     uint32_t dim() const;
// };