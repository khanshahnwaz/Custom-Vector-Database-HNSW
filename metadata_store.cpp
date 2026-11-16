
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
    const char MAGIC[8] = {
        'V', 'E', 'C', 'M', 'E', 'T', 'A', '1'
    };

    const uint32_t FORMAT_VERSION = 1;

    // Limit individual metadata fields to 1 MB.
    const uint32_t MAX_STRING_SIZE = 1024 * 1024;


    template <typename T>
    void writeValue(ofstream& out, const T& value)
    {
        out.write(
            reinterpret_cast<const char*>(&value),
            sizeof(T)
        );

        if (!out)
        {
            throw runtime_error(
                "Failed to write metadata file"
            );
        }
    }


    template <typename T>
    void readValue(ifstream& in, T& value)
    {
        in.read(
            reinterpret_cast<char*>(&value),
            sizeof(T)
        );

        if (!in)
        {
            throw runtime_error(
                "Unexpected end of metadata file"
            );
        }
    }


    void writeString(ofstream& out, const string& value)
    {
        if (value.size() > MAX_STRING_SIZE)
        {
            throw runtime_error(
                "Metadata string exceeds maximum allowed size"
            );
        }

        uint32_t length = static_cast<uint32_t>(
            value.size()
        );

        writeValue(out, length);

        if (length > 0)
        {
            out.write(value.data(), length);

            if (!out)
            {
                throw runtime_error(
                    "Failed to write metadata string"
                );
            }
        }
    }


    string readString(
        ifstream& in,
        streamoff fileSize
    )
    {
        uint32_t length;

        readValue(in, length);

        if (length > MAX_STRING_SIZE)
        {
            throw runtime_error(
                "Invalid metadata string length"
            );
        }

        streamoff position = in.tellg();

        if (position < 0 ||
            static_cast<streamoff>(length) >
            fileSize - position)
        {
            throw runtime_error(
                "Metadata string exceeds file boundaries"
            );
        }

        string value(length, '\0');

        if (length > 0)
        {
            in.read(&value[0], length);

            if (!in)
            {
                throw runtime_error(
                    "Failed to read metadata string"
                );
            }
        }

        return value;
    }


    void writeMetadata(
        ofstream& out,
        const Metadata& metadata
    )
    {
        writeString(out, metadata.title);
        writeString(out, metadata.category);
        writeString(out, metadata.source);
        writeString(out, metadata.documentId);
    }


    Metadata readMetadata(
        ifstream& in,
        streamoff fileSize
    )
    {
        Metadata metadata;

        metadata.title = readString(in, fileSize);
        metadata.category = readString(in, fileSize);
        metadata.source = readString(in, fileSize);
        metadata.documentId = readString(in, fileSize);

        return metadata;
    }
}


// Insert or update metadata.
void MetadataStore::upsert(
    uint32_t id,
    const Metadata& metadata
)
{
    records[id] = metadata;
}


// Retrieve metadata.
const Metadata& MetadataStore::get(uint32_t id) const
{
    map<uint32_t, Metadata>::const_iterator it =
        records.find(id);

    if (it == records.end())
    {
        throw out_of_range(
            "Metadata does not exist for this vector ID"
        );
    }

    return it->second;
}


// Check metadata existence.
bool MetadataStore::contains(uint32_t id) const
{
    return records.find(id) != records.end();
}


// Return metadata record count.
uint32_t MetadataStore::size() const
{
    return static_cast<uint32_t>(records.size());
}


// Save metadata to disk.
void MetadataStore::save(const string& filename) const
{
    if (records.size() >
        numeric_limits<uint32_t>::max())
    {
        throw runtime_error(
            "Too many metadata records"
        );
    }

    const string tempFilename = filename + ".tmp";

    try
    {
        ofstream out(
            tempFilename.c_str(),
            ios::binary | ios::trunc
        );

        if (!out)
        {
            throw runtime_error(
                "Cannot create metadata temporary file"
            );
        }

        // Header
        out.write(MAGIC, sizeof(MAGIC));

        if (!out)
        {
            throw runtime_error(
                "Failed to write metadata signature"
            );
        }

        writeValue(out, FORMAT_VERSION);

        uint32_t recordCount =
            static_cast<uint32_t>(records.size());

        writeValue(out, recordCount);

        // Records are written in vector ID order.
        for (
            map<uint32_t, Metadata>::const_iterator it =
                records.begin();
            it != records.end();
            ++it
        )
        {
            writeValue(out, it->first);
            writeMetadata(out, it->second);
        }

        out.flush();

        if (!out)
        {
            throw runtime_error(
                "Failed to flush metadata file"
            );
        }

        out.close();

        if (!out)
        {
            throw runtime_error(
                "Failed to close metadata file"
            );
        }

#ifdef _WIN32

        if (!MoveFileExA(
                tempFilename.c_str(),
                filename.c_str(),
                MOVEFILE_REPLACE_EXISTING |
                MOVEFILE_WRITE_THROUGH))
        {
            throw runtime_error(
                "Failed to replace metadata file"
            );
        }

#else

        if (rename(
                tempFilename.c_str(),
                filename.c_str()) != 0)
        {
            throw runtime_error(
                "Failed to replace metadata file"
            );
        }

#endif

        cout << "Metadata saved successfully: "
             << filename << "\n";
    }
    catch (...)
    {
        remove(tempFilename.c_str());
        throw;
    }
}


// Load metadata from disk.
void MetadataStore::load(
    const string& filename,
    uint32_t vectorCount
)
{
    ifstream in(filename.c_str(), ios::binary);

    if (!in)
    {
        throw runtime_error(
            "Cannot open metadata file: " + filename
        );
    }

    // Determine physical file size.
    in.seekg(0, ios::end);

    streamoff fileSize = in.tellg();

    if (fileSize < 16)
    {
        throw runtime_error(
            "Metadata file is too small"
        );
    }

    in.seekg(0, ios::beg);

    // Read and validate signature.
    char magic[8];

    in.read(magic, sizeof(magic));

    if (!in || memcmp(magic, MAGIC, sizeof(MAGIC)) != 0)
    {
        throw runtime_error(
            "Invalid metadata file signature"
        );
    }

    // Read version.
    uint32_t version;

    readValue(in, version);

    if (version != FORMAT_VERSION)
    {
        throw runtime_error(
            "Unsupported metadata file version"
        );
    }

    // Read record count.
    uint32_t recordCount;

    readValue(in, recordCount);

    if (recordCount > vectorCount)
    {
        throw runtime_error(
            "Metadata record count exceeds vector count"
        );
    }

    // Build temporary records.
    map<uint32_t, Metadata> loadedRecords;

    for (uint32_t i = 0; i < recordCount; i++)
    {
        uint32_t id;

        readValue(in, id);

        if (id >= vectorCount)
        {
            throw runtime_error(
                "Metadata contains an invalid vector ID"
            );
        }

        Metadata metadata = readMetadata(in, fileSize);

        pair<
            map<uint32_t, Metadata>::iterator,
            bool
        > result = loadedRecords.insert(
            make_pair(id, metadata)
        );

        if (!result.second)
        {
            throw runtime_error(
                "Duplicate vector ID in metadata file"
            );
        }
    }

    // Reject unexpected trailing data.
    if (in.tellg() != fileSize)
    {
        throw runtime_error(
            "Metadata file contains unexpected trailing data"
        );
    }

    // Commit only after complete validation.
    records.swap(loadedRecords);

    cout << "Metadata loaded successfully: "
         << filename << "\n";

    cout << "Metadata records: "
         << records.size() << "\n";
}
