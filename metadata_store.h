#pragma once

#include <string>
#include <map>
#include <cstdint>

using namespace std;

struct Metadata
{
    string title;
    string category;
    string source;
    string documentId;
    string text;
    uint32_t chunkIndex;

    Metadata() : chunkIndex(0) {}
};

class MetadataStore
{
private:
    map<uint32_t, Metadata> records;

public:
    void upsert(uint32_t id, const Metadata& metadata);
    const Metadata& get(uint32_t id) const;
    bool contains(uint32_t id) const;
    uint32_t size() const;
    void save(const string& filename) const;
    void load(const string& filename, uint32_t vectorCount);
};
