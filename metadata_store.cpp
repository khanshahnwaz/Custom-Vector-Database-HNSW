#include "metadata_store.h"

#include <fstream>
#include <stdexcept>
#include <limits>
#include <cstdio>
#include <iostream>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

namespace
{
    const char MAGIC[8] = {'V', 'E', 'C', 'M', 'E', 'T', 'A', '1'};
    const uint32_t FORMAT_VERSION = 2;
    const uint32_t MAX_STRING_SIZE = 4 * 1024 * 1024;

    template <typename T>
    void writeValue(ofstream& out, const T& value)
    {
        out.write(reinterpret_cast<const char*>(&value), sizeof(T));
        if (!out) throw runtime_error("Failed to write metadata file");
    }

    template <typename T>
    void readValue(ifstream& in, T& value)
    {
        in.read(reinterpret_cast<char*>(&value), sizeof(T));
        if (!in) throw runtime_error("Unexpected end of metadata file");
    }

    void writeString(ofstream& out, const string& value)
    {
        if (value.size() > MAX_STRING_SIZE)
            throw runtime_error("Metadata string exceeds maximum allowed size");

        uint32_t length = static_cast<uint32_t>(value.size());
        writeValue(out, length);
        if (length > 0)
        {
            out.write(value.data(), length);
            if (!out) throw runtime_error("Failed to write metadata string");
        }
    }

    string readString(ifstream& in, streamoff fileSize)
    {
        uint32_t length = 0;
        readValue(in, length);
        if (length > MAX_STRING_SIZE)
            throw runtime_error("Invalid metadata string length");

        streamoff position = in.tellg();
        if (position < 0 || position > fileSize ||
            static_cast<streamoff>(length) > fileSize - position)
            throw runtime_error("Metadata string exceeds file boundaries");

        string value(length, '\0');
        if (length > 0)
        {
            in.read(&value[0], length);
            if (!in) throw runtime_error("Failed to read metadata string");
        }
        return value;
    }

    void writeMetadata(ofstream& out, const Metadata& metadata)
    {
        writeValue(out, metadata.chunkIndex);
        writeString(out, metadata.title);
        writeString(out, metadata.category);
        writeString(out, metadata.source);
        writeString(out, metadata.documentId);
        writeString(out, metadata.text);
    }

    Metadata readMetadata(ifstream& in, streamoff fileSize, uint32_t version)
    {
        Metadata metadata;
        if (version >= 2) readValue(in, metadata.chunkIndex);
        metadata.title = readString(in, fileSize);
        metadata.category = readString(in, fileSize);
        metadata.source = readString(in, fileSize);
        metadata.documentId = readString(in, fileSize);
        if (version >= 2) metadata.text = readString(in, fileSize);
        return metadata;
    }

    void replaceFile(const string& temporary, const string& destination)
    {
#ifdef _WIN32
        if (!MoveFileExA(temporary.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw runtime_error("Failed to replace metadata file");
#else
        if (rename(temporary.c_str(), destination.c_str()) != 0)
            throw runtime_error("Failed to replace metadata file");
#endif
    }
}

void MetadataStore::upsert(uint32_t id, const Metadata& metadata)
{
    records[id] = metadata;
}

const Metadata& MetadataStore::get(uint32_t id) const
{
    map<uint32_t, Metadata>::const_iterator it = records.find(id);
    if (it == records.end())
        throw out_of_range("Metadata does not exist for this vector ID");
    return it->second;
}

bool MetadataStore::contains(uint32_t id) const
{
    return records.find(id) != records.end();
}

uint32_t MetadataStore::size() const
{
    return static_cast<uint32_t>(records.size());
}

void MetadataStore::save(const string& filename) const
{
    if (records.size() > numeric_limits<uint32_t>::max())
        throw runtime_error("Too many metadata records");

    const string temporary = filename + ".tmp";
    try
    {
        ofstream out(temporary.c_str(), ios::binary | ios::trunc);
        if (!out) throw runtime_error("Cannot create metadata temporary file");

        out.write(MAGIC, sizeof(MAGIC));
        if (!out) throw runtime_error("Failed to write metadata signature");
        writeValue(out, FORMAT_VERSION);
        uint32_t recordCount = static_cast<uint32_t>(records.size());
        writeValue(out, recordCount);

        for (map<uint32_t, Metadata>::const_iterator it = records.begin();
             it != records.end(); ++it)
        {
            writeValue(out, it->first);
            writeMetadata(out, it->second);
        }

        out.flush();
        if (!out) throw runtime_error("Failed to flush metadata file");
        out.close();
        if (!out) throw runtime_error("Failed to close metadata file");
        replaceFile(temporary, filename);

        cout << "Metadata saved successfully: " << filename << "\n";
    }
    catch (...)
    {
        remove(temporary.c_str());
        throw;
    }
}

void MetadataStore::load(const string& filename, uint32_t vectorCount)
{
    ifstream in(filename.c_str(), ios::binary);
    if (!in) throw runtime_error("Cannot open metadata file: " + filename);

    in.seekg(0, ios::end);
    streamoff fileSize = in.tellg();
    if (fileSize < 16) throw runtime_error("Metadata file is too small");
    in.seekg(0, ios::beg);

    char magic[8];
    in.read(magic, sizeof(magic));
    if (!in || memcmp(magic, MAGIC, sizeof(MAGIC)) != 0)
        throw runtime_error("Invalid metadata file signature");

    uint32_t version = 0;
    readValue(in, version);
    if (version != 1 && version != 2)
        throw runtime_error("Unsupported metadata file version");

    uint32_t recordCount = 0;
    readValue(in, recordCount);
    if (recordCount > vectorCount)
        throw runtime_error("Metadata record count exceeds vector count");

    map<uint32_t, Metadata> loadedRecords;
    for (uint32_t i = 0; i < recordCount; ++i)
    {
        uint32_t id = 0;
        readValue(in, id);
        if (id >= vectorCount)
            throw runtime_error("Metadata contains an invalid vector ID");

        Metadata metadata = readMetadata(in, fileSize, version);
        pair<map<uint32_t, Metadata>::iterator, bool> inserted =
            loadedRecords.insert(make_pair(id, metadata));
        if (!inserted.second)
            throw runtime_error("Duplicate vector ID in metadata file");
    }

    if (in.tellg() != fileSize)
        throw runtime_error("Metadata file contains unexpected trailing data");

    records.swap(loadedRecords);
    cout << "Metadata loaded successfully: " << filename << "\n";
    cout << "Metadata records: " << records.size() << "\n";
}
