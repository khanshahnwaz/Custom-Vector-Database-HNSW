#include "vector_database.h"
#include "distance.h"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <map>
#include <sstream>
#include <cctype>

using namespace std;

namespace
{
    bool fileExists(const string& filename)
    {
        ifstream file(filename.c_str(), ios::binary);
        return file.good();
    }

    bool matchesFilter(const Metadata& metadata, const SearchFilter& filter)
    {
        if (!filter.title.empty() && metadata.title != filter.title) return false;
        if (!filter.category.empty() && metadata.category != filter.category) return false;
        if (!filter.source.empty() && metadata.source != filter.source) return false;
        if (!filter.documentId.empty() && metadata.documentId != filter.documentId) return false;
        return true;
    }

    bool hasActiveFilter(const SearchFilter& filter)
    {
        return !filter.title.empty() || !filter.category.empty() ||
               !filter.source.empty() || !filter.documentId.empty();
    }

    void appendUtf8(string& output, unsigned long codepoint)
    {
        if (codepoint <= 0x7F)
        {
            output.push_back(static_cast<char>(codepoint));
        }
        else if (codepoint <= 0x7FF)
        {
            output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        else if (codepoint <= 0xFFFF)
        {
            output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        else if (codepoint <= 0x10FFFF)
        {
            output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        else
        {
            throw runtime_error("Invalid Unicode code point in metadata JSONL");
        }
    }

    class JsonObjectParser
    {
    private:
        const string& input;
        size_t position;

        void skipWhitespace()
        {
            while (position < input.size() &&
                   (input[position] == ' ' || input[position] == '\t' ||
                    input[position] == '\r' || input[position] == '\n'))
                ++position;
        }

        void expect(char expected)
        {
            skipWhitespace();
            if (position >= input.size() || input[position] != expected)
                throw runtime_error("Malformed JSONL metadata record");
            ++position;
        }

        unsigned long readHex4()
        {
            if (position + 4 > input.size())
                throw runtime_error("Incomplete Unicode escape in metadata JSONL");
            unsigned long value = 0;
            for (int i = 0; i < 4; ++i)
            {
                char c = input[position++];
                value <<= 4;
                if (c >= '0' && c <= '9') value += c - '0';
                else if (c >= 'a' && c <= 'f') value += 10 + c - 'a';
                else if (c >= 'A' && c <= 'F') value += 10 + c - 'A';
                else throw runtime_error("Invalid Unicode escape in metadata JSONL");
            }
            return value;
        }

        string readString()
        {
            expect('"');
            string output;
            while (position < input.size())
            {
                unsigned char c = static_cast<unsigned char>(input[position++]);
                if (c == '"') return output;
                if (c < 0x20) throw runtime_error("Control character in JSON string");
                if (c != '\\')
                {
                    output.push_back(static_cast<char>(c));
                    continue;
                }

                if (position >= input.size())
                    throw runtime_error("Incomplete escape in metadata JSONL");
                char escaped = input[position++];
                switch (escaped)
                {
                    case '"': output.push_back('"'); break;
                    case '\\': output.push_back('\\'); break;
                    case '/': output.push_back('/'); break;
                    case 'b': output.push_back('\b'); break;
                    case 'f': output.push_back('\f'); break;
                    case 'n': output.push_back('\n'); break;
                    case 'r': output.push_back('\r'); break;
                    case 't': output.push_back('\t'); break;
                    case 'u':
                    {
                        unsigned long cp = readHex4();
                        if (cp >= 0xD800 && cp <= 0xDBFF)
                        {
                            if (position + 2 > input.size() ||
                                input[position] != '\\' || input[position + 1] != 'u')
                                throw runtime_error("Missing low surrogate in metadata JSONL");
                            position += 2;
                            unsigned long low = readHex4();
                            if (low < 0xDC00 || low > 0xDFFF)
                                throw runtime_error("Invalid low surrogate in metadata JSONL");
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        }
                        else if (cp >= 0xDC00 && cp <= 0xDFFF)
                        {
                            throw runtime_error("Unexpected low surrogate in metadata JSONL");
                        }
                        appendUtf8(output, cp);
                        break;
                    }
                    default: throw runtime_error("Unsupported escape in metadata JSONL");
                }
            }
            throw runtime_error("Unterminated string in metadata JSONL");
        }

        string readUnsignedInteger()
        {
            skipWhitespace();
            size_t start = position;
            while (position < input.size() && input[position] >= '0' && input[position] <= '9')
                ++position;
            if (start == position) throw runtime_error("Expected unsigned integer in metadata JSONL");
            return input.substr(start, position - start);
        }

    public:
        explicit JsonObjectParser(const string& value) : input(value), position(0) {}

        map<string, string> parse()
        {
            map<string, string> fields;
            expect('{');
            skipWhitespace();
            if (position < input.size() && input[position] == '}')
            {
                ++position;
                skipWhitespace();
                if (position != input.size()) throw runtime_error("Trailing data in JSONL record");
                return fields;
            }

            while (true)
            {
                string key = readString();
                expect(':');
                skipWhitespace();
                string value;
                if (position < input.size() && input[position] == '"')
                    value = readString();
                else
                    value = readUnsignedInteger();

                if (fields.find(key) != fields.end())
                    throw runtime_error("Duplicate JSON key in metadata record: " + key);
                fields[key] = value;

                skipWhitespace();
                if (position < input.size() && input[position] == ',')
                {
                    ++position;
                    continue;
                }
                expect('}');
                break;
            }

            skipWhitespace();
            if (position != input.size()) throw runtime_error("Trailing data in JSONL record");
            return fields;
        }
    };

    string requiredField(const map<string, string>& fields, const string& name)
    {
        map<string, string>::const_iterator it = fields.find(name);
        if (it == fields.end()) throw runtime_error("Missing metadata JSON field: " + name);
        return it->second;
    }

    Metadata parseMetadataLine(const string& line)
    {
        JsonObjectParser parser(line);
        map<string, string> fields = parser.parse();

        Metadata metadata;
        metadata.title = requiredField(fields, "title");
        metadata.category = requiredField(fields, "category");
        metadata.source = requiredField(fields, "source");
        metadata.documentId = requiredField(fields, "document_id");
        metadata.text = requiredField(fields, "text");

        string chunkIndex = requiredField(fields, "chunk_index");
        unsigned long long parsed = 0;
        for (size_t i = 0; i < chunkIndex.size(); ++i)
        {
            if (chunkIndex[i] < '0' || chunkIndex[i] > '9')
                throw runtime_error("chunk_index must be an unsigned integer");
            parsed = parsed * 10 + static_cast<unsigned long long>(chunkIndex[i] - '0');
            if (parsed > numeric_limits<uint32_t>::max())
                throw runtime_error("chunk_index exceeds supported range");
        }
        metadata.chunkIndex = static_cast<uint32_t>(parsed);

        if (metadata.documentId.empty())
            throw runtime_error("document_id cannot be empty");
        if (metadata.text.empty())
            throw runtime_error("Document chunk text cannot be empty");
        return metadata;
    }

    vector<Metadata> readMetadataJsonl(const string& filename)
    {
        ifstream in(filename.c_str());
        if (!in) throw runtime_error("Cannot open ingestion metadata file: " + filename);

        vector<Metadata> records;
        string line;
        uint32_t expectedChunkIndex = 0;
        while (getline(in, line))
        {
            if (line.empty()) throw runtime_error("Blank line in ingestion metadata JSONL");
            Metadata metadata = parseMetadataLine(line);
            if (metadata.chunkIndex != expectedChunkIndex)
                throw runtime_error("chunk_index values must be sequential starting at zero");
            records.push_back(metadata);
            ++expectedChunkIndex;
        }
        if (!in.eof()) throw runtime_error("Error while reading ingestion metadata JSONL");
        if (records.empty()) throw runtime_error("Ingestion metadata file contains no records");
        return records;
    }

    bool sameIngestionMetadata(const Metadata& a, const Metadata& b)
    {
        return a.title == b.title && a.category == b.category &&
               a.source == b.source && a.documentId == b.documentId &&
               a.text == b.text && a.chunkIndex == b.chunkIndex;
    }
}

VectorDatabase::VectorDatabase(
    const string& vectorFilename,
    const string& indexFilename,
    uint32_t initialVectorCount,
    int M,
    int efConstruction,
    int efSearch,
    const string& metadataFilename
)
    : store(vectorFilename),
      index(store, M, efConstruction),
      vectorFilename(vectorFilename),
      indexFilename(indexFilename),
      metadataFilename(metadataFilename),
      efSearch(efSearch),
      initialized(false),
      initializationAttempted(false)
{
    // initialVectorCount is retained for source compatibility with the
    // previous constructor. The current corpus file is authoritative:
    // every vector in vectors.bin is now an indexable corpus vector.
    if (initialVectorCount == 0 || initialVectorCount > store.size())
        throw invalid_argument("Invalid initial vector count");

    if (store.size() > static_cast<uint32_t>(numeric_limits<int>::max()))
        throw invalid_argument("Vector count exceeds supported HNSW node ID range");

    if (M <= 0 || efConstruction <= 0 || efSearch <= 0)
        throw invalid_argument("HNSW parameters must be positive");
}

void VectorDatabase::initialize()
{
    if (initialized)
    {
        cout << "Database is already initialized.\n";
        return;
    }
    if (initializationAttempted)
        throw logic_error("Initialization was already attempted. Create a new VectorDatabase object.");

    initializationAttempted = true;
    bool rebuildIndex = !fileExists(indexFilename);

    if (!rebuildIndex)
    {
        cout << "Existing HNSW index found.\nLoading index...\n";
        try
        {
            index.loadIndex(indexFilename);
        }
        catch (const runtime_error& error)
        {
            // Only a dataset-count/dimension mismatch is recoverable here.
            // Invalid signatures, truncated files, and graph corruption remain fatal.
            if (string(error.what()) == "Index does not match vector dataset")
                rebuildIndex = true;
            else
                throw;
        }

        if (!rebuildIndex && index.getNodeCount() != static_cast<int>(store.size()))
            throw runtime_error("Loaded index node count does not match current vector count");

        if (!rebuildIndex) cout << "Index loaded successfully.\n";
    }

    if (rebuildIndex)
    {
        cout << "Building HNSW index for " << store.size() << " corpus vectors...\n";
        for (uint32_t id = 0; id < store.size(); ++id)
            index.insert(static_cast<int>(id));

        if (index.getNodeCount() != static_cast<int>(store.size()))
            throw runtime_error("Constructed index has an unexpected node count");

        index.saveIndex(indexFilename);
        cout << "Index construction completed.\n";
    }

    index.validateGraph();

    if (fileExists(metadataFilename))
        metadataStore.load(metadataFilename, store.size());
    else
        cout << "No metadata file found. Starting with empty metadata storage.\n";

    initialized = true;
    cout << "VectorDatabase is ready.\n";
}

uint32_t VectorDatabase::ingest(
    const string& embeddingFilename,
    const string& metadataJsonlFilename
)
{
    if (!initialized)
        throw logic_error("Database must be initialized before ingestion");

    VectorStore incoming(embeddingFilename);
    vector<Metadata> incomingMetadata = readMetadataJsonl(metadataJsonlFilename);

    if (incoming.dim() != store.dim())
        throw invalid_argument("Ingestion embedding dimension does not match database dimension");
    if (incoming.size() != incomingMetadata.size())
        throw invalid_argument("Embedding count does not match metadata record count");

    if (incoming.size() > static_cast<uint32_t>(numeric_limits<int>::max()) - store.size())
        throw overflow_error("Ingestion batch exceeds supported HNSW node ID range");

    // Make ingestion repeat-safe: the same document UUID and chunk indexes
    // are recognized as already present instead of being appended twice.
    uint32_t alreadyPresent = 0;
    for (size_t i = 0; i < incomingMetadata.size(); ++i)
    {
        bool found = false;
        for (uint32_t id = 0; id < store.size(); ++id)
        {
            if (!metadataStore.contains(id)) continue;
            const Metadata& existing = metadataStore.get(id);
            if (existing.documentId == incomingMetadata[i].documentId &&
                existing.chunkIndex == incomingMetadata[i].chunkIndex)
            {
                if (!sameIngestionMetadata(existing, incomingMetadata[i]))
                    throw runtime_error("Document chunk already exists with different metadata or text");
                found = true;
                break;
            }
        }
        if (found) ++alreadyPresent;
    }

    if (alreadyPresent == incomingMetadata.size())
    {
        cout << "This document batch is already present. No vectors were appended.\n";
        return 0;
    }
    if (alreadyPresent != 0)
        throw runtime_error("Partial duplicate document batch detected; refusing to append an incomplete duplicate");

    const uint32_t firstNewId = store.size();
    try
    {
        for (uint32_t i = 0; i < incoming.size(); ++i)
        {
            uint32_t newId = store.append(incoming.getVector(i));
            if (newId != static_cast<uint32_t>(index.getNodeCount()))
                throw logic_error("Vector ID and next HNSW node ID are out of sync");

            index.insert(static_cast<int>(newId));
            metadataStore.upsert(newId, incomingMetadata[i]);
        }

        cout << "Ingested " << incoming.size() << " chunks. New vector IDs: "
             << firstNewId << " through " << (store.size() - 1) << ".\n";

        save();
        return incoming.size();
    }
    catch (...)
    {
        // A failed in-memory insertion may have partially changed the graph.
        // Force the caller to discard this object; disk files are only replaced
        // during save(), after the entire batch has been inserted successfully.
        initialized = false;
        throw;
    }
}

vector<int> VectorDatabase::search(const vector<float>& query, int k)
{
    if (!initialized) throw logic_error("Database must be initialized before searching");
    if (query.size() != store.dim()) throw invalid_argument("Query dimension does not match vector dimension");
    if (k <= 0) throw invalid_argument("k must be positive");
    return index.search(query, k, efSearch);
}

vector<SearchResult> VectorDatabase::search(
    const vector<float>& query,
    int k,
    const SearchFilter& filter
)
{
    if (!initialized) throw logic_error("Database must be initialized before searching");
    if (query.size() != store.dim()) throw invalid_argument("Query dimension does not match vector dimension");
    if (k <= 0) throw invalid_argument("k must be positive");

    const bool filtering = hasActiveFilter(filter);
    int candidateCount = k;
    if (filtering)
    {
        long long desired = max(static_cast<long long>(k) * 10,
                                static_cast<long long>(efSearch));
        candidateCount = static_cast<int>(min(static_cast<long long>(store.size()), desired));
    }

    vector<SearchResult> results;
    while (true)
    {
        int currentEfSearch = filtering ? max(efSearch, candidateCount) : efSearch;
        vector<int> candidates = index.search(query, candidateCount, currentEfSearch);
        results.clear();

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            int id = candidates[i];
            bool metadataExists = metadataStore.contains(static_cast<uint32_t>(id));
            if (filtering && !metadataExists) continue;

            Metadata metadata;
            if (metadataExists) metadata = metadataStore.get(static_cast<uint32_t>(id));
            if (filtering && !matchesFilter(metadata, filter)) continue;

            SearchResult result;
            result.id = static_cast<uint32_t>(id);
            result.distance = cosineDistance(query, store.getVector(static_cast<uint32_t>(id)));
            result.hasMetadata = metadataExists;
            result.metadata = metadata;
            results.push_back(result);
        }

        sort(results.begin(), results.end(),
             [](const SearchResult& a, const SearchResult& b)
             {
                 if (a.distance != b.distance) return a.distance < b.distance;
                 return a.id < b.id;
             });

        if (static_cast<int>(results.size()) >= k || !filtering ||
            candidateCount >= static_cast<int>(store.size()))
            break;

        long long nextCount = static_cast<long long>(candidateCount) * 2;
        candidateCount = static_cast<int>(min(static_cast<long long>(store.size()), nextCount));
    }

    if (static_cast<int>(results.size()) > k) results.resize(k);
    return results;
}

void VectorDatabase::save() const
{
    if (!initialized) throw logic_error("Database must be initialized before saving");

    // The index header records the vector-store size. Write vectors first,
    // then metadata, then index. If interrupted before the final index write,
    // startup detects the stale index count and rebuilds it.
    store.save(vectorFilename);
    metadataStore.save(metadataFilename);
    index.saveIndex(indexFilename);
    cout << "VectorDatabase saved successfully.\n";
}

const vector<float>& VectorDatabase::getVector(uint32_t id) const
{
    return store.getVector(id);
}

void VectorDatabase::setMetadata(uint32_t id, const Metadata& metadata)
{
    if (!initialized) throw logic_error("Database must be initialized before setting metadata");
    if (id >= store.size()) throw out_of_range("Vector ID does not exist");
    metadataStore.upsert(id, metadata);
}

const Metadata& VectorDatabase::getMetadata(uint32_t id) const
{
    if (!initialized) throw logic_error("Database must be initialized before retrieving metadata");
    return metadataStore.get(id);
}

bool VectorDatabase::hasMetadata(uint32_t id) const
{
    if (!initialized) throw logic_error("Database must be initialized before checking metadata");
    return metadataStore.contains(id);
}

uint32_t VectorDatabase::metadataCount() const { return metadataStore.size(); }
uint32_t VectorDatabase::size() const { return store.size(); }
uint32_t VectorDatabase::dimension() const { return store.dim(); }
bool VectorDatabase::isInitialized() const { return initialized; }
