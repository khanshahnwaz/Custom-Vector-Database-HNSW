
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
};

class MetadataStore
{
private:
    // Vector ID -> Metadata
    map<uint32_t, Metadata> records;

public:
    // Insert new metadata or update existing metadata.
    void upsert(uint32_t id, const Metadata& metadata);

    // Retrieve metadata for a vector ID.
    const Metadata& get(uint32_t id) const;

    // Check whether metadata exists.
    bool contains(uint32_t id) const;

    // Number of metadata records.
    uint32_t size() const;

    // Save metadata to disk.
    void save(const string& filename) const;

    // Load metadata from disk.
    // vectorCount is used to validate vector IDs.
    void load(
        const string& filename,
        uint32_t vectorCount
    );
};
