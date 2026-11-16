#include "vector_store.h"

#include <fstream>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

namespace
{
    void replaceFile(const string& temporary, const string& destination)
    {
#ifdef _WIN32
        if (!MoveFileExA(temporary.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            throw runtime_error("Could not replace vector file");
        }
#else
        if (rename(temporary.c_str(), destination.c_str()) != 0)
        {
            throw runtime_error("Could not replace vector file");
        }
#endif
    }

    vector<float> normalizedCopy(const vector<float>& input, uint32_t expectedDimension)
    {
        if (input.size() != expectedDimension)
        {
            throw invalid_argument("Vector dimension does not match VectorStore dimension");
        }

        double squaredNorm = 0.0;
        for (size_t i = 0; i < input.size(); ++i)
        {
            if (!isfinite(input[i]))
            {
                throw invalid_argument("Vector contains a non-finite value");
            }
            squaredNorm += static_cast<double>(input[i]) * input[i];
        }

        if (!(squaredNorm > 0.0) || !isfinite(squaredNorm))
        {
            throw invalid_argument("Cannot store a zero or invalid vector");
        }

        const double norm = sqrt(squaredNorm);
        vector<float> output(input);
        for (size_t i = 0; i < output.size(); ++i)
        {
            output[i] = static_cast<float>(output[i] / norm);
        }
        return output;
    }
}

VectorStore::VectorStore(const string& filename)
    : numVectors(0), dimension(0)
{
    ifstream in(filename.c_str(), ios::binary);
    if (!in)
    {
        throw runtime_error("Could not open vector file: " + filename);
    }

    in.read(reinterpret_cast<char*>(&numVectors), sizeof(numVectors));
    in.read(reinterpret_cast<char*>(&dimension), sizeof(dimension));

    if (!in || numVectors == 0 || dimension == 0)
    {
        throw runtime_error("Invalid or incomplete vector file header");
    }

    const uint64_t expectedSize = 2ULL * sizeof(uint32_t) +
        static_cast<uint64_t>(numVectors) * dimension * sizeof(float);

    in.seekg(0, ios::end);
    const streamoff actualSize = in.tellg();
    if (actualSize < 0 || static_cast<uint64_t>(actualSize) != expectedSize)
    {
        throw runtime_error("Vector file size does not match its header");
    }
    in.seekg(2 * sizeof(uint32_t), ios::beg);

    vectors.resize(numVectors);
    for (uint32_t i = 0; i < numVectors; ++i)
    {
        vectors[i].resize(dimension);
        in.read(reinterpret_cast<char*>(&vectors[i][0]),
                static_cast<streamsize>(dimension * sizeof(float)));
        if (!in)
        {
            throw runtime_error("Could not read vector data");
        }
        vectors[i] = normalizedCopy(vectors[i], dimension);
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

uint32_t VectorStore::append(const vector<float>& value)
{
    if (numVectors == numeric_limits<uint32_t>::max())
    {
        throw overflow_error("VectorStore has reached its maximum vector count");
    }

    vector<float> normalized = normalizedCopy(value, dimension);
    vectors.push_back(normalized);
    ++numVectors;
    return numVectors - 1;
}

void VectorStore::save(const string& filename) const
{
    const string temporary = filename + ".tmp";
    try
    {
        ofstream out(temporary.c_str(), ios::binary | ios::trunc);
        if (!out)
        {
            throw runtime_error("Could not create temporary vector file");
        }

        out.write(reinterpret_cast<const char*>(&numVectors), sizeof(numVectors));
        out.write(reinterpret_cast<const char*>(&dimension), sizeof(dimension));

        for (uint32_t i = 0; i < numVectors; ++i)
        {
            out.write(reinterpret_cast<const char*>(&vectors[i][0]),
                      static_cast<streamsize>(dimension * sizeof(float)));
            if (!out)
            {
                throw runtime_error("Failed while writing vector data");
            }
        }

        out.flush();
        if (!out)
        {
            throw runtime_error("Failed to flush vector file");
        }
        out.close();
        if (!out)
        {
            throw runtime_error("Failed to close vector file");
        }

        replaceFile(temporary, filename);
    }
    catch (...)
    {
        remove(temporary.c_str());
        throw;
    }
}
