#pragma once
#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdio>
#include <bits/stdc++.h>
#include "distance.h"
#include "vector_store.h"
#include "brute_force.h"

#include <fstream>
#include <cstdint>
#include <stdexcept>

using namespace std;

class Node
{
public:
    int id;
    vector<vector<int>> neighbors;
};

class HNSW
{
    VectorStore &store;

    vector<Node> nodes;

    int enterPoint = -1;
    int maxLevel = -1;

    int M;
int efConstruction;



    long long searchLayerCalls = 0;
long long selectNeighborsCalls = 0;
long long pruneNeighborsCalls = 0;


long long searchLayerTime = 0;
long long selectNeighborsTime = 0;
long long pruneNeighborsTime = 0;

long long distanceCalculations = 0;

long long actualPruneCount = 0;

long long neighborSelectionCandidates = 0;

long long neighborSelectionExamined = 0;


public:
    HNSW(VectorStore &store,int M=16,int efConstruction=100) : store(store),M(M),efConstruction(efConstruction) { }



int getNodeCount() const
{
    return static_cast<int>(nodes.size());
}




void saveIndex(const string& filename) const
{
    if (nodes.empty())
    {
        throw runtime_error("Cannot save an empty HNSW index");
    }

    if (enterPoint < 0 ||
        enterPoint >= static_cast<int>(nodes.size()) ||
        maxLevel < 0 || maxLevel > 5 ||
        M <= 0 || efConstruction <= 0)
    {
        throw runtime_error("Invalid HNSW state. Cannot save index");
    }

    // Save to a temporary file first.
    const string tempFilename = filename + ".tmp";

    ofstream out(tempFilename, ios::binary | ios::trunc);

    if (!out.is_open())
    {
        throw runtime_error("Could not open temporary index file");
    }

    try
    {
        // ------------------------------------------------
        // 1. Header
        // ------------------------------------------------

        const char magic[8] =
            {'H', 'N', 'S', 'W', 'I', 'D', 'X', '1'};

        uint32_t version = 1;
        uint32_t MValue = static_cast<uint32_t>(M);
        uint32_t efValue = static_cast<uint32_t>(efConstruction);

        uint32_t vectorCount = store.size();
        uint32_t dimension = store.dim();
        uint32_t nodeCount = static_cast<uint32_t>(nodes.size());

        int32_t entryPoint = enterPoint;
        int32_t highestLevel = maxLevel;

        // Helper to check every write operation.
        auto writeValue = [&](const auto& value)
        {
            out.write(
                reinterpret_cast<const char*>(&value),
                sizeof(value)
            );

            if (!out)
            {
                throw runtime_error("Error while writing HNSW index");
            }
        };

        out.write(magic, sizeof(magic));

        if (!out)
        {
            throw runtime_error("Error while writing index signature");
        }

        writeValue(version);
        writeValue(MValue);
        writeValue(efValue);

        writeValue(vectorCount);
        writeValue(dimension);
        writeValue(nodeCount);

        writeValue(entryPoint);
        writeValue(highestLevel);

        // ------------------------------------------------
        // 2. Serialize every node
        // ------------------------------------------------

        for (size_t i = 0; i < nodes.size(); i++)
        {
            const Node& node = nodes[i];

            if (node.id != static_cast<int>(i))
            {
                throw runtime_error("Node ID/index mismatch");
            }

            if (node.neighbors.empty() ||
                node.neighbors.size() > 6)
            {
                throw runtime_error("Invalid node layer count");
            }

            int32_t nodeId = node.id;

            uint32_t layerCount =
                static_cast<uint32_t>(node.neighbors.size());

            writeValue(nodeId);
            writeValue(layerCount);

            // Serialize every layer.
            for (const auto& neighborList : node.neighbors)
            {
                if (neighborList.size() > static_cast<size_t>(M))
                {
                    throw runtime_error("Invalid neighbor count");
                }

                uint32_t neighborCount =
                    static_cast<uint32_t>(neighborList.size());

                writeValue(neighborCount);

                for (int neighborId : neighborList)
                {
                    if (neighborId < 0 ||
                        neighborId >= static_cast<int>(nodes.size()) ||
                        neighborId == node.id)
                    {
                        throw runtime_error("Invalid neighbor ID");
                    }

                    int32_t id = neighborId;

                    writeValue(id);
                }
            }
        }

        // ------------------------------------------------
        // 3. Flush and close temporary file
        // ------------------------------------------------

        out.flush();

        if (!out)
        {
            throw runtime_error("Failed to flush temporary index file");
        }

        out.close();

        if (out.fail())
        {
            throw runtime_error("Failed to close temporary index file");
        }

        // ------------------------------------------------
        // 4. Replace the original index
        // ------------------------------------------------

#ifdef _WIN32

        // Windows: replace existing file atomically.
        // MoveFileExW is used through the Windows API.
        if (!MoveFileExA(
                tempFilename.c_str(),
                filename.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            throw runtime_error(
                "Could not replace existing HNSW index file"
            );
        }

#else

        // POSIX: rename replaces the destination atomically
        // when both files are on the same filesystem.
        if (rename(tempFilename.c_str(), filename.c_str()) != 0)
        {
            throw runtime_error(
                "Could not replace existing HNSW index file"
            );
        }

#endif

        cout << "HNSW index saved successfully: "
             << filename << '\n';
    }
    catch (...)
    {
        if (out.is_open())
        {
            out.close();
        }

        // Remove incomplete temporary file.
        remove(tempFilename.c_str());

        throw;
    }
}





void loadIndex(const string& filename)
{
    ifstream in(filename, ios::binary);

    if (!in.is_open())
    {
        throw runtime_error("Could not open HNSW index file");
    }

    // ------------------------------------------------
    // 1. Validate file size
    // ------------------------------------------------

    in.seekg(0, ios::end);

    streamoff fileSize = in.tellg();

    if (fileSize < 0)
    {
        throw runtime_error("Could not determine index file size");
    }

    constexpr streamoff headerSize =
        8 + 6 * sizeof(uint32_t) + 2 * sizeof(int32_t);

    if (fileSize < headerSize)
    {
        throw runtime_error("Index file is too small");
    }

    in.seekg(0, ios::beg);

    if (!in)
    {
        throw runtime_error("Could not reset index file position");
    }

    // ------------------------------------------------
    // 2. Read and validate header
    // ------------------------------------------------

    char magic[8];

    const char expectedMagic[8] =
        {'H', 'N', 'S', 'W', 'I', 'D', 'X', '1'};

    in.read(magic, sizeof(magic));

    if (!in || memcmp(magic, expectedMagic, 8) != 0)
    {
        throw runtime_error("Invalid HNSW index file");
    }

    uint32_t version;
    uint32_t loadedM;
    uint32_t loadedEfConstruction;

    uint32_t vectorCount;
    uint32_t dimension;
    uint32_t nodeCount;

    int32_t loadedEntryPoint;
    int32_t loadedMaxLevel;

    auto readValue = [&](auto& value)
    {
        in.read(
            reinterpret_cast<char*>(&value),
            sizeof(value)
        );

        if (!in)
        {
            throw runtime_error("Corrupted or incomplete index file");
        }
    };

    readValue(version);
    readValue(loadedM);
    readValue(loadedEfConstruction);

    readValue(vectorCount);
    readValue(dimension);
    readValue(nodeCount);

    readValue(loadedEntryPoint);
    readValue(loadedMaxLevel);

    // ------------------------------------------------
    // 3. Validate header values
    // ------------------------------------------------

    if (version != 1)
    {
        throw runtime_error("Unsupported HNSW index version");
    }

    if (loadedM == 0 || loadedEfConstruction == 0)
    {
        throw runtime_error("Invalid HNSW configuration");
    }

    if (loadedM > static_cast<uint32_t>(INT32_MAX) ||
        loadedEfConstruction > static_cast<uint32_t>(INT32_MAX))
    {
        throw runtime_error("HNSW configuration exceeds supported integer range");
    }

    if (vectorCount != store.size() ||
        dimension != store.dim())
    {
        throw runtime_error("Index does not match vector dataset");
    }

    if (nodeCount == 0 || nodeCount > vectorCount ||
        nodeCount > static_cast<uint32_t>(INT32_MAX))
    {
        throw runtime_error("Invalid node count");
    }

    // Each node requires at least 12 bytes:
    // nodeId + layerCount + one neighborCount.

    uint64_t remainingBytes =
        static_cast<uint64_t>(fileSize - headerSize);

    uint64_t minimumRequiredBytes =
        static_cast<uint64_t>(nodeCount) * 12;

    if (minimumRequiredBytes > remainingBytes)
    {
        throw runtime_error(
            "Index file is too small for declared node count"
        );
    }

    if (loadedEntryPoint < 0 ||
        loadedEntryPoint >= static_cast<int32_t>(nodeCount))
    {
        throw runtime_error("Invalid entry point");
    }

    if (loadedMaxLevel < 0 || loadedMaxLevel > 5)
    {
        throw runtime_error("Invalid maximum level");
    }

    // ------------------------------------------------
    // 4. Reconstruct nodes into temporary storage
    // ------------------------------------------------

    vector<Node> loadedNodes(nodeCount);

    for (uint32_t i = 0; i < nodeCount; i++)
    {
        int32_t nodeId;
        uint32_t layerCount;

        readValue(nodeId);
        readValue(layerCount);

        // Current implementation requires node ID == index.
        if (nodeId != static_cast<int32_t>(i))
        {
            throw runtime_error("Node ID/index mismatch");
        }

        if (layerCount == 0 || layerCount > 6)
        {
            throw runtime_error("Invalid node layer count");
        }

        Node node;

        node.id = nodeId;
        node.neighbors.resize(layerCount);

        // Read every layer.
        for (uint32_t level = 0; level < layerCount; level++)
        {
            uint32_t neighborCount;

            readValue(neighborCount);

            if (neighborCount > loadedM)
            {
                throw runtime_error("Invalid neighbor count");
            }

            // Ensure the file contains enough bytes for this list.
            uint64_t requiredBytes =
                static_cast<uint64_t>(neighborCount) * sizeof(int32_t);

            streamoff currentPosition = in.tellg();

            if (currentPosition < 0 ||
                requiredBytes >
                    static_cast<uint64_t>(fileSize - currentPosition))
            {
                throw runtime_error(
                    "Incomplete neighbor list in index file"
                );
            }

            node.neighbors[level].resize(neighborCount);

            for (uint32_t j = 0; j < neighborCount; j++)
            {
                int32_t neighborId;

                readValue(neighborId);

                if (neighborId < 0 ||
                    neighborId >= static_cast<int32_t>(nodeCount))
                {
                    throw runtime_error("Invalid neighbor ID");
                }

                if (neighborId == nodeId)
                {
                    throw runtime_error("Self-loop detected");
                }

                node.neighbors[level][j] = neighborId;
            }
        }

        loadedNodes[i] = move(node);
    }

    // ------------------------------------------------
    // 5. Check for unexpected trailing data
    // ------------------------------------------------

    if (in.tellg() != fileSize)
    {
        throw runtime_error("Unexpected trailing data in index file");
    }

    // ------------------------------------------------
    // 6. Validate graph structure
    // ------------------------------------------------

    int actualMaxLevel = -1;

    for (const Node& node : loadedNodes)
    {
        actualMaxLevel = max(
            actualMaxLevel,
            static_cast<int>(node.neighbors.size()) - 1
        );

        for (int level = 0;
             level < static_cast<int>(node.neighbors.size());
             level++)
        {
            const auto& neighbors = node.neighbors[level];

            unordered_set<int> seen;

            for (int neighborId : neighbors)
            {
                if (!seen.insert(neighborId).second)
                {
                    throw runtime_error("Duplicate neighbor detected");
                }

                const Node& neighbor = loadedNodes[neighborId];

                if (level >= static_cast<int>(neighbor.neighbors.size()))
                {
                    throw runtime_error(
                        "Neighbor does not exist at this level"
                    );
                }

                const auto& reverseList = neighbor.neighbors[level];

                if (find(
                        reverseList.begin(),
                        reverseList.end(),
                        node.id
                    ) == reverseList.end())
                {
                    throw runtime_error(
                        "Bidirectional edge validation failed"
                    );
                }
            }
        }
    }

    if (actualMaxLevel != loadedMaxLevel)
    {
        throw runtime_error("Maximum level mismatch");
    }

    if (static_cast<int>(
            loadedNodes[loadedEntryPoint].neighbors.size()
        ) - 1 != loadedMaxLevel)
    {
        throw runtime_error("Entry point level mismatch");
    }

    // ------------------------------------------------
    // 7. Commit reconstructed graph
    // ------------------------------------------------

    // The active graph is modified only after all checks pass.
    nodes = move(loadedNodes);

    enterPoint = loadedEntryPoint;
    maxLevel = loadedMaxLevel;

    M = loadedM;
    efConstruction = loadedEfConstruction;

    cout << "HNSW index loaded successfully: "
         << filename << '\n';

    cout << "Nodes: " << nodes.size() << '\n';
    cout << "Entry point: " << enterPoint << '\n';
    cout << "Maximum level: " << maxLevel << '\n';
}




void printConstructionStats() const
{
    cout << "\n========== Construction Statistics ==========\n";

    cout << "searchLayer calls: "
         << searchLayerCalls
         << endl;

    cout << "searchLayer time: "
         << searchLayerTime
         << " us\n";

    cout << "\nselectNeighbors calls: "
         << selectNeighborsCalls
         << endl;

    cout << "selectNeighbors time: "
         << selectNeighborsTime
         << " us\n";

    cout << "\npruneNeighbors calls: "
         << pruneNeighborsCalls
         << endl;

    cout << "pruneNeighbors time: "
         << pruneNeighborsTime
         << " us\n";



         cout << "Distance calculations: "
     << distanceCalculations
     << endl;


     cout << "Actual pruning operations: "
     << actualPruneCount
     << endl;


     cout<<"Actually entered diversity loop in selectNeighbors "<<neighborSelectionCandidates<<endl;


     cout<<"Actually examined :"<<neighborSelectionExamined<<endl;
}

// ------------------------------------------------------------
// INSERT
//
// Inserts a vector into the HNSW graph.
//
// The insertion process has three main stages:
//
// 1. Generate a random level for the new node.
// 2. Traverse existing upper layers to find good entry points.
// 3. From the highest common layer down to layer 0:
//      - Search for candidate neighbors.
//      - Select the best neighbors.
//      - Create bidirectional edges.
//      - Prune neighbors if their degree exceeds M.
//
// id:
//     ID of the vector being inserted.
// ------------------------------------------------------------
void insert(int id)
{
    // Vector IDs must remain identical to their positions in both
    // VectorStore and the HNSW nodes array. This is essential when
    // appending document chunks to an existing index.
    if (id < 0)
    {
        throw invalid_argument("HNSW node ID cannot be negative");
    }

    if (static_cast<uint32_t>(id) >= store.size())
    {
        throw out_of_range("HNSW node ID has no corresponding stored vector");
    }

    if (id != static_cast<int>(nodes.size()))
    {
        throw logic_error("HNSW nodes must be inserted using the next sequential vector ID");
    }

    // ------------------------------------------------------------
    // Create the node and assign its randomly generated maximum
    // level.
    //
    // neighbors.size() = nodeLevel + 1
    //
    // For example:
    //
    //     nodeLevel = 2
    //     neighbors = { layer 0, layer 1, layer 2 }
    // ------------------------------------------------------------
    Node node = createNode(id);

    int nodeLevel =
        static_cast<int>(node.neighbors.size()) - 1;


    // ------------------------------------------------------------
    // Handle the first node separately.
    //
    // There are no existing nodes to search or connect to, so the
    // first node simply becomes the entry point of the graph.
    // ------------------------------------------------------------
    if (nodes.empty())
    {
        nodes.push_back(
            move(node)
        );

        enterPoint = id;
        maxLevel = nodeLevel;

        return;
    }


    // ------------------------------------------------------------
    // Start traversal from the current entry point.
    //
    // At the beginning of insertion, there is normally only one
    // entry point: the current global entry point.
    //
    // As we move through the hierarchy, searchLayer() may return
    // multiple candidates that become entry points for the next
    // layer.
    // ------------------------------------------------------------
    vector<int> entryPoints = {
        enterPoint
    };


    // ------------------------------------------------------------
    // Phase 1: Traverse upper layers
    //
    // If the existing graph has layers above the new node's
    // maximum level, the new node cannot be inserted into those
    // layers.
    //
    // We therefore perform greedy searches through those layers
    // to find a good starting point for the layers where the new
    // node will actually exist.
    //
    // ef = 1 is used for greedy traversal at upper layers.
    // ------------------------------------------------------------
    for (int level = maxLevel;
         level > nodeLevel;
         level--)
    {
        entryPoints =
            searchLayer(
                store.getVector(id),
                entryPoints,
                1,
                level
            );
    }


    // ------------------------------------------------------------
    // Add the new node to the graph BEFORE creating edges.
    //
    // This is important because once we start creating
    // bidirectional connections, all referenced node IDs must
    // already exist in the nodes vector.
    // ------------------------------------------------------------
    nodes.push_back(
        move(node)
    );


    // ------------------------------------------------------------
    // Phase 2: Construct the new node's connections.
    //
    // Start from the highest layer where both:
    //
    //     new node
    //
    // and
    //
    //     existing graph
    //
    // are present.
    //
    // Then move downward until layer 0.
    // ------------------------------------------------------------
    for (int level =
             min(nodeLevel, maxLevel);
         level >= 0;
         level--)
    {
        // --------------------------------------------------------
        // Search the current layer for potential neighbors.
        //
        // efConstruction controls how many candidates are
        // considered during graph construction.
        // --------------------------------------------------------
        vector<int> candidates =
            searchLayer(
                store.getVector(id),
                entryPoints,
                efConstruction,
                level
            );


        // --------------------------------------------------------
        // Select up to M neighbors using the HNSW neighbor
        // selection / diversity heuristic.
        // --------------------------------------------------------
        vector<int> selected =
            selectNeighbors(
                id,
                candidates,
                M
            );


        // --------------------------------------------------------
        // Create outgoing edges:
        //
        //     new node -> selected neighbors
        // --------------------------------------------------------
        nodes[id].neighbors[level] =
            selected;


        // --------------------------------------------------------
        // Create the reverse edges:
        //
        //     selected neighbor -> new node
        //
        // This keeps the graph bidirectional.
        //
        // Adding the new edge may cause an existing node to have
        // more than M neighbors, so that neighbor may need to be
        // pruned.
        // --------------------------------------------------------
        for (int neighborId : selected)
        {
 // --------------------------------------------------------
    // Add the reverse edge:
    //
    //     neighbor -> new node
    // --------------------------------------------------------
    auto& neighborList =
        nodes[neighborId].neighbors[level];

    neighborList.push_back(id);

    // --------------------------------------------------------
    // Prune only when the neighbor actually exceeds M.
    //
    // This avoids calling pruneNeighbors() when there is
    // nothing to prune.
    // --------------------------------------------------------
    if (neighborList.size() > M)
    {
        pruneNeighbors(
            neighborId,
            level
        );
    }
        }


        // --------------------------------------------------------
        // The candidates found at this layer become the entry
        // points used when searching the next lower layer.
        //
        // This allows the search to continue from the region of
        // the graph discovered at the current level.
        // --------------------------------------------------------
        entryPoints = candidates;
    }


    // ------------------------------------------------------------
    // Phase 3: Update the global entry point.
    //
    // If the new node has a level higher than the previous
    // maximum level, it becomes the new entry point and the graph
    // height increases.
    // ------------------------------------------------------------
    if (nodeLevel > maxLevel)
    {
        enterPoint = id;
        maxLevel = nodeLevel;
    }
}


// ------------------------------------------------------------
// PRUNE-NEIGHBORS
//
// Limits the number of neighbors of a node at a particular
// HNSW layer to at most M.
//
// When a new edge is added, a node may temporarily have more
// than M neighbors. In that case:
//
//     1. Collect the current neighbors.
//     2. Select the best M neighbors using the HNSW
//        neighbor-selection heuristic.
//     3. Remove reverse edges for neighbors that were pruned.
//     4. Replace the old neighbor list with the selected list.
//
// nodeId:
//     Node whose neighbors need to be pruned.
//
// level:
//     HNSW layer at which pruning is performed.
// ------------------------------------------------------------
void pruneNeighbors(int nodeId, int level)
{
auto start =
    chrono::high_resolution_clock::now();

pruneNeighborsCalls++;    // ------------------------------------------------------------
    // Nothing to prune if the node already satisfies the maximum
    // neighbor limit.
    // ------------------------------------------------------------
    if (nodes[nodeId].neighbors[level].size() <=
        static_cast<size_t>(M))
    {
        return;
    }

    
 actualPruneCount++;
    // ------------------------------------------------------------
    // Keep a copy of the current neighbors because the original
    // list will be replaced after selection.
    // ------------------------------------------------------------
    vector<int> oldNeighbors =
        nodes[nodeId].neighbors[level];

    // ------------------------------------------------------------
    // Select at most M neighbors using the HNSW diversity
    // heuristic.
    // ------------------------------------------------------------
    vector<int> selected =
        selectNeighbors(
            nodeId,
            oldNeighbors,
            M
        );

    // Used to determine which old neighbors were retained.
    unordered_set<int> selectedSet(
        selected.begin(),
        selected.end()
    );

    // ------------------------------------------------------------
    // Remove reverse edges from neighbors that were pruned.
    //
    // If:
    //
    //     nodeId -> neighbor
    //
    // was removed, then:
    //
    //     neighbor -> nodeId
    //
    // must also be removed to keep the graph bidirectional.
    // ------------------------------------------------------------
    for (int neighborId : oldNeighbors)
    {
        if (selectedSet.count(neighborId))
        {
            continue;
        }

        auto& reverseNeighbors =
            nodes[neighborId].neighbors[level];

        reverseNeighbors.erase(
            remove(
                reverseNeighbors.begin(),
                reverseNeighbors.end(),
                nodeId
            ),
            reverseNeighbors.end()
        );
    }

    // ------------------------------------------------------------
    // Replace the old neighbor list with the selected neighbors.
    // ------------------------------------------------------------
    nodes[nodeId].neighbors[level] =
        selected;



        auto end =
    chrono::high_resolution_clock::now();

pruneNeighborsTime +=
    chrono::duration_cast<
        chrono::microseconds
    >(end - start).count();
}


// ------------------------------------------------------------
// SELECT-NEIGHBORS
//
// Selects up to M neighbors for a node from a set of candidates.
//
// The selection happens in two phases:
//
// Phase 1:
//     Apply the HNSW diversity heuristic to select useful and
//     diverse neighbors.
//
// Phase 2:
//     If fewer than M neighbors were selected, fill the remaining
//     slots with the closest candidates that were not selected.
//
// queryId:
//     ID of the node for which we are selecting neighbors.
//
// candidates:
//     Candidate neighbor IDs obtained from searchLayer().
//
// M:
//     Maximum number of neighbors to select.
// ------------------------------------------------------------
vector<int> selectNeighbors(
    int queryId,
    const vector<int>& candidates,
    int M)
{
auto start =
    chrono::high_resolution_clock::now();

selectNeighborsCalls++;    // ------------------------------------------------------------
    // Store candidates along with their distance from the query.
    // ------------------------------------------------------------
    vector<pair<float, int>> sortedCandidates;
    sortedCandidates.reserve(candidates.size());

    const vector<float>& query =
        store.getVector(queryId);

    // ------------------------------------------------------------
    // Calculate the distance between the query node and every
    // candidate.
    //
    // A node must not select itself as its own neighbor.
    // ------------------------------------------------------------
    for (int candidateId : candidates)
    {
        if (candidateId == queryId)
        {
            continue;
        }

        distanceCalculations++;

        float distance =
            cosineDistance(
                query,
                store.getVector(candidateId)
            );

        sortedCandidates.push_back({
            distance,
            candidateId
        });
    }


    neighborSelectionCandidates +=
    sortedCandidates.size();


    // ------------------------------------------------------------
    // Process candidates from nearest to farthest.
    // ------------------------------------------------------------
    sort(
        sortedCandidates.begin(),
        sortedCandidates.end()
    );

    vector<int> selected;
    selected.reserve(M);

    // Used to quickly check whether a candidate has already
    // been selected.
    unordered_set<int> selectedSet;

    // ------------------------------------------------------------
    // Phase 1: Diversity-based selection
    //
    // A candidate is selected if it provides a useful connection
    // that is not already sufficiently represented by the nodes
    // selected earlier.
    //
    // For candidate c and an already selected node s:
    //
    //     d(c, query) < d(c, s)
    //
    // If this condition is false, the candidate is considered
    // redundant because it is closer to an existing neighbor than
    // it is to the query.
    // ------------------------------------------------------------
    for (const auto& it
         : sortedCandidates)
    {

         neighborSelectionExamined++;


        auto queryToCandidate=it.first;
        auto candidateId=it.second;
        bool goodCandidate = true;


const vector<float>& candidateVector =
    store.getVector(candidateId);

        for (int selectedId : selected)
        {
            const vector<float>& selectedVector =
        store.getVector(selectedId);

        distanceCalculations++;

    float candidateToSelected =
        cosineDistance(
            candidateVector,
            selectedVector
        );

            // ----------------------------------------------------
            // If the candidate is closer to an already selected
            // neighbor than to the query, it does not provide
            // enough additional diversity.
            // ----------------------------------------------------
            if (queryToCandidate >= candidateToSelected)
            {
                goodCandidate = false;
                break;
            }
        }

        // --------------------------------------------------------
        // Candidate passed the diversity test.
        // --------------------------------------------------------
        if (goodCandidate)
        {
            selected.push_back(candidateId);
            selectedSet.insert(candidateId);
        }

        // Stop once we have enough neighbors.
        if (selected.size() >= static_cast<size_t>(M))
        {
            break;
        }
    }

    // ------------------------------------------------------------
    // Phase 2: Fallback
    //
    // The diversity heuristic may select fewer than M candidates.
    // Fill the remaining slots with the closest candidates that
    // were not selected during Phase 1.
    // ------------------------------------------------------------
    if (selected.size() < static_cast<size_t>(M))
    {
        for (const auto& it
             : sortedCandidates)
        {
            auto distance=it.first;
            auto candidateId=it.second;
            if (selectedSet.count(candidateId))
            {
                continue;
            }

            selected.push_back(candidateId);
            selectedSet.insert(candidateId);

            if (selected.size() >= static_cast<size_t>(M))
            {
                break;
            }
        }
    }

auto end =
    chrono::high_resolution_clock::now();

selectNeighborsTime +=
    chrono::duration_cast<
        chrono::microseconds
    >(end - start).count();

return selected;

}





    vector<int> selectClosest(
    const vector<float>& query,
    const vector<int>& candidates,
    int M)
{
    vector<pair<float, int>> distances;

    for (int id : candidates)
    {
        float dist =
            cosineDistance(
                store.getVector(id),
                query
            );

        distances.push_back({dist, id});
    }

    sort(
        distances.begin(),
        distances.end()
    );

    vector<int> result;

    for (int i = 0;
         i < min(M, (int)distances.size());
         i++)
    {
        result.push_back(distances[i].second);
    }

    return result;
}

// ------------------------------------------------------------
// RANDOM LEVEL
//
// Assigns a random maximum level to a new HNSW node.
//
// HNSW uses an exponentially decreasing probability for higher
// levels. Most nodes therefore appear only in layer 0, while a
// small number of nodes are promoted to higher layers.
//
// mL controls the level distribution.
// ------------------------------------------------------------
int randomLevel(float mL)
{
    // Generate a random value in the interval (0, 1].
    float u =
        static_cast<float>(rand()) /
        static_cast<float>(RAND_MAX);

    // Prevent log(0).
    if (u == 0.0f)
    {
        u = 1e-7f;
    }

    // Inverse-transform sampling for the exponential
    // distribution used by HNSW.
    //
    // Small u  -> large -log(u) -> higher level
    // Large u  -> small -log(u) -> lower level
    return static_cast<int>(
        -log(u) * mL
    );
}


// create the node, get the random maximum level
// now that node belongs to all levels from this to zero 

 // ------------------------------------------------------------
// CREATE NODE
//
// Creates a new HNSW node and assigns its random maximum level.
//
// If the assigned level is L, the node exists in:
//
//     Layer 0, 1, ..., L
//
// Therefore it needs L + 1 neighbor lists.
// ------------------------------------------------------------
Node createNode(int id)
{
    Node node;

    node.id = id;

    // --------------------------------------------------------
    // HNSW uses an exponentially decreasing probability for
    // higher levels.
    //
    // mL = 1 / ln(2)
    // controls the level distribution.
    // --------------------------------------------------------
    float mL =
        1.0f / log(2.0f);

    int level =
        randomLevel(mL);

    // --------------------------------------------------------
    // Limit the maximum level for our current implementation.
    // --------------------------------------------------------
    level =
        min(level, 5);

    // --------------------------------------------------------
    // A node with level L participates in L + 1 layers.
    //
    // Example:
    //
    // level = 0 -> neighbors[0]
    // level = 1 -> neighbors[0], neighbors[1]
    // level = 2 -> neighbors[0], neighbors[1], neighbors[2]
    // --------------------------------------------------------
    node.neighbors.resize(level + 1);

    return node;
}


// ------------------------------------------------------------
// SEARCH-LAYER
//
// Searches a single layer of the HNSW graph.
//
// query:
//     Query embedding.
//
// entryPoints:
//     Starting nodes for this layer.
//
// ef:
//     Maximum number of candidates maintained in W.
//
// layer:
//     HNSW layer being searched.
//
// Returns:
//     The candidate nodes currently contained in W.
//
// HNSW uses two priority queues:
//
// candidates:
//     Min-heap.
//     The closest unexplored candidate is processed first.
//
// W:
//     Max-heap.
//     Contains the best ef candidates found so far.
//     The farthest candidate is at the top and can be removed
//     when W exceeds ef.
// ------------------------------------------------------------
vector<int> searchLayer(
    const vector<float>& query,
    const vector<int>& entryPoints,
    int ef,
    int layer)
{

auto start =
    chrono::high_resolution_clock::now();

searchLayerCalls++;    // ------------------------------------------------------------
    // visited prevents the same node from being processed more
    // than once.
    // ------------------------------------------------------------
    unordered_set<int> visited;

    // ------------------------------------------------------------
    // candidates:
    // Min-heap ordered by distance.
    //
    // We always expand the currently closest unexplored node.
    // ------------------------------------------------------------
    priority_queue<
        pair<float, int>,
        vector<pair<float, int>>,
        greater<>
    > candidates;

    // ------------------------------------------------------------
    // W:
    // Max-heap ordered by distance.
    //
    // The farthest candidate currently stored in W is always
    // available at w.top().
    // ------------------------------------------------------------
    priority_queue<
        pair<float, int>
    > w;

    // ------------------------------------------------------------
    // Initialize both heaps with the supplied entry points.
    // ------------------------------------------------------------
    for (int entryPoint : entryPoints)
    {
        if (visited.count(entryPoint))
        {
            continue;
        }

        visited.insert(entryPoint);

        float distance =
            cosineDistance(
                store.getVector(entryPoint),
                query
            );

        candidates.push({
            distance,
            entryPoint
        });

        w.push({
            distance,
            entryPoint
        });
    }

    // ------------------------------------------------------------
    // Main search loop.
    //
    // At every iteration:
    //
    // 1. Take the closest unexplored candidate.
    // 2. If it is already farther than the worst candidate in W,
    //    no further improvement is possible, so stop.
    // 3. Explore its neighbors.
    // 4. Add promising neighbors to candidates and W.
    // 5. Keep W limited to ef elements.
    // ------------------------------------------------------------
    while (!candidates.empty())
    {
        // Closest unexplored candidate.
        float currentDistance =
            candidates.top().first;

        int currentNode =
            candidates.top().second;

        candidates.pop();

        // --------------------------------------------------------
        // W should normally never be empty here because the
        // current candidate was initially inserted into W.
        // This check simply protects against invalid states.
        // --------------------------------------------------------
        if (w.empty())
        {
            break;
        }

        // Distance of the farthest candidate currently in W.
        float farthestDistance =
            w.top().first;

        // --------------------------------------------------------
        // Termination condition from HNSW Algorithm 2.
        //
        // If the closest unexplored candidate is already farther
        // than the farthest candidate we have retained, exploring
        // further cannot improve W.
        // --------------------------------------------------------
        if (currentDistance > farthestDistance)
        {
            break;
        }

        // --------------------------------------------------------
        // Explore all neighbors of the current node at this layer.
        // --------------------------------------------------------
        for (int neighborId :
             nodes[currentNode].neighbors[layer])
        {
            // Skip nodes that have already been visited.
            if (visited.count(neighborId))
            {
                continue;
            }

            visited.insert(neighborId);

            // ----------------------------------------------------
            // Calculate the neighbor's distance from the query.
            // ----------------------------------------------------
            float neighborDistance =
                cosineDistance(
                    store.getVector(neighborId),
                    query
                );

            // Current farthest distance in W.
            farthestDistance =
                w.top().first;

            // ----------------------------------------------------
            // Add the neighbor if:
            //
            // 1. It is closer than the current worst candidate, OR
            //
            // 2. W has not reached ef elements yet.
            // ----------------------------------------------------
            if (neighborDistance < farthestDistance ||
                w.size() < static_cast<size_t>(ef))
            {
                candidates.push({
                    neighborDistance,
                    neighborId
                });

                w.push({
                    neighborDistance,
                    neighborId
                });
            }

            // ----------------------------------------------------
            // W can contain at most ef candidates.
            //
            // Since W is a max-heap, pop() removes the farthest
            // candidate.
            // ----------------------------------------------------
            if (w.size() > static_cast<size_t>(ef))
            {
                w.pop();
            }
        }
    }

    // ------------------------------------------------------------
    // Convert W from a priority queue into a vector.
    //
    // Note:
    // W is a max-heap, so the returned order is not nearest-first.
    // The public search() function sorts the final candidates
    // before returning the top-k results.
    // ------------------------------------------------------------
    vector<int> result;

    while (!w.empty())
    {
        result.push_back(
            w.top().second
        );

        w.pop();
    }
auto end =
    chrono::high_resolution_clock::now();

searchLayerTime +=
    chrono::duration_cast<
        chrono::microseconds
    >(end - start).count();


    return result;
}





    void printState()
{
    cout << "\n\n========== HNSW ==========\n";

    cout << "Number of nodes: "
         << nodes.size() << endl;

    cout << "Entry point: "
         << enterPoint << endl;

    cout << "Maximum level: "
         << maxLevel << endl;

    for (const auto& node : nodes)
    {
        // cout << "\nNode " << node.id << endl;

        for (int level = 0;
             level < node.neighbors.size();
             level++)
        {
            cout << "  Layer " << level << ": ";

            for (int neighbor : node.neighbors[level])
            {
                cout << neighbor << " ";
            }

            cout << endl;
        }
    }
}




int getEntryPoint() const
{
    return enterPoint;
}


vector<int> search(
    const vector<float>& query,
    int k,
    int efSearch)
{
    // ------------------------------------------------------------
    // Basic validation
    // ------------------------------------------------------------
    if (nodes.empty() || k <= 0 || efSearch <= 0)
    {
        return {};
    }

    // ------------------------------------------------------------
    // Start the search from the current entry point.
    //
    // The entry point belongs to the highest layer of the graph.
    // As we move down the hierarchy, the result of the previous
    // layer becomes the entry point for the next layer.
    // ------------------------------------------------------------
    vector<int> entryPoints = {enterPoint};

    // ------------------------------------------------------------
    // Search upper layers.
    //
    // At higher layers we perform a greedy search using ef = 1.
    //
    // Higher layers contain fewer nodes, so we use them mainly
    // to quickly move toward the region containing the query.
    // ------------------------------------------------------------
    for (int level = maxLevel;
         level > 0;
         level--)
    {
        entryPoints =
            searchLayer(
                query,
                entryPoints,
                1,
                level
            );
    }

    // ------------------------------------------------------------
    // Search layer 0.
    //
    // Layer 0 contains the complete graph and therefore requires
    // a wider search.
    //
    // efSearch controls how many candidates are considered during
    // this search:
    //
    //     larger efSearch -> better recall
    //                       -> higher search cost
    // ------------------------------------------------------------
    vector<int> candidates =
        searchLayer(
            query,
            entryPoints,
            efSearch,
            0
        );

    // ------------------------------------------------------------
    // searchLayer() returns the candidate set, not necessarily
    // ordered from nearest to farthest.
    //
    // Sort candidates using their actual distance from the query.
    // ------------------------------------------------------------
    sort(
        candidates.begin(),
        candidates.end(),
        [&](int a, int b)
        {
            return cosineDistance(
                       store.getVector(a),
                       query
                   )
                 <
                   cosineDistance(
                       store.getVector(b),
                       query
                   );
        }
    );

    // ------------------------------------------------------------
    // Keep only the k nearest neighbors requested by the caller.
    // ------------------------------------------------------------
    if (candidates.size() > static_cast<size_t>(k))
    {
        candidates.resize(k);
    }

    return candidates;
}



void printNeighbors(int id)
{
    cout << "\nNeighbors of node "
         << id << ":\n";

    for (int level = 0;
         level < nodes[id].neighbors.size();
         level++)
    {
        cout << "Layer "
             << level
             << ": ";

        for (int neighbor :
             nodes[id].neighbors[level])
        {
            cout << neighbor << " ";
        }

        cout << endl;
    }
}



void validateGraph() const
{
    cout << "\n========== Graph Validation ==========\n";

    bool valid = true;

    // ------------------------------------------------------------
    // Check every node at every level.
    // ------------------------------------------------------------
    for (const auto& node : nodes)
    {
        for (int level = 0;
             level < static_cast<int>(node.neighbors.size());
             level++)
        {
            const auto& neighbors =
                node.neighbors[level];

            // ----------------------------------------------------
            // 1. Degree check
            //
            // Each node should have at most M neighbors at a
            // particular level.
            // ----------------------------------------------------
            if (neighbors.size() > M)
            {
                cout << "ERROR: Node "
                     << node.id
                     << " has "
                     << neighbors.size()
                     << " neighbors at level "
                     << level
                     << " (maximum allowed: "
                     << M
                     << ")\n";

                valid = false;
            }

            // Used to detect duplicate neighbor IDs.
            unordered_set<int> seen;

            // ----------------------------------------------------
            // Validate every neighbor of the current node.
            // ----------------------------------------------------
            for (int neighborId : neighbors)
            {
                // ------------------------------------------------
                // 2. Valid neighbor ID
                //
                // Every neighbor must refer to an existing node.
                // ------------------------------------------------
                if (neighborId < 0 ||
                    neighborId >= static_cast<int>(nodes.size()))
                {
                    cout << "ERROR: Invalid neighbor "
                         << neighborId
                         << " from node "
                         << node.id
                         << " at level "
                         << level
                         << "\n";

                    valid = false;
                    continue;
                }

                // ------------------------------------------------
                // 3. Self-loop check
                //
                // A node should never be its own neighbor.
                // ------------------------------------------------
                if (neighborId == node.id)
                {
                    cout << "ERROR: Self-loop at node "
                         << node.id
                         << " at level "
                         << level
                         << "\n";

                    valid = false;
                }

                // ------------------------------------------------
                // 4. Duplicate edge check
                //
                // The same neighbor should not appear twice in
                // the same node's neighbor list.
                // ------------------------------------------------
                if (!seen.insert(neighborId).second)
                {
                    cout << "ERROR: Duplicate neighbor "
                         << neighborId
                         << " for node "
                         << node.id
                         << " at level "
                         << level
                         << "\n";

                    valid = false;
                }

                // ------------------------------------------------
                // 5. Bidirectional edge check
                //
                // If:
                //
                //     A -> B
                //
                // then B should also contain:
                //
                //     B -> A
                //
                // at the same level.
                // ------------------------------------------------
                const auto& neighborNode =
                    nodes[neighborId];

                // A neighbor must exist at this level in order
                // to have a reverse edge at this level.
                if (level >=
                    static_cast<int>(
                        neighborNode.neighbors.size()))
                {
                    cout << "ERROR: Node "
                         << neighborId
                         << " does not exist at level "
                         << level
                         << ", but node "
                         << node.id
                         << " points to it\n";

                    valid = false;
                    continue;
                }

                const auto& reverseNeighbors =
                    neighborNode.neighbors[level];

                if (find(
                        reverseNeighbors.begin(),
                        reverseNeighbors.end(),
                        node.id
                    ) == reverseNeighbors.end())
                {
                    cout << "ERROR: Edge "
                         << node.id
                         << " -> "
                         << neighborId
                         << " is not bidirectional at level "
                         << level
                         << "\n";

                    valid = false;
                }
            }
        }
    }

    // ------------------------------------------------------------
    // Final validation result.
    // ------------------------------------------------------------
    if (valid)
    {
        cout << "Graph validation PASSED\n";
    }
    else
    {
        cout << "Graph validation FAILED\n";
    }
}

};



// int main()
// {
//     // ------------------------------------------------------------
//     // Load the vector dataset.
//     // Each vector has 384 dimensions and is accessed by its ID.
//     // ------------------------------------------------------------
//     VectorStore store("vectors.bin");

//     // Create the HNSW index using the vector store.
//     HNSW test(store);

//     // ------------------------------------------------------------
//     // Build the HNSW graph.
//     //
//     // For this test, we insert the first 1000 vectors.
//     // ------------------------------------------------------------
//     auto start = chrono::high_resolution_clock::now();

//     for (int id = 0; id < 1000; id++)
//     {
//         test.insert(id);
//     }

//     auto end = chrono::high_resolution_clock::now();

//     auto constructionTime =
//         chrono::duration_cast<chrono::milliseconds>(
//             end - start
//         ).count();

//     cout << "Inserted all nodes\n";

//     cout << "\nHNSW construction time: "
//          << constructionTime
//          << " ms\n";


//     // ------------------------------------------------------------
//     // Select a query vector.
//     //
//     // We use vector 4 as the query. Since vector 4 is part of
//     // the indexed dataset, its exact nearest neighbor is itself.
//     // ------------------------------------------------------------
//     const vector<float>& query =
//         store.getVector(4);

//     // ------------------------------------------------------------
//     // Search parameters.
//     //
//     // k:
//     //     Number of nearest neighbors required.
//     //
//     // efSearch:
//     //     Number of candidates maintained during HNSW search.
//     //     A larger value generally improves recall at the cost
//     //     of higher search latency.
//     // ------------------------------------------------------------
//     int k = 3;
//     int efSearch = 10;


//     // ------------------------------------------------------------
//     // Ground Truth
//     //
//     // Brute force checks every one of the 1000 indexed vectors.
//     // This gives us the exact nearest neighbors against which
//     // we can evaluate HNSW.
//     // ------------------------------------------------------------
//     vector<int> groundTruth =
//         bruteForceKNN(
//             store,
//             query,
//             k,
//             1000
//         );

//     cout << "\nBrute Force:\n";

//     for (int id : groundTruth)
//     {
//         cout << id << " ";
//     }

//     cout << endl;


//     // ------------------------------------------------------------
//     // HNSW Search
//     //
//     // The public search() function handles:
//     //
//     // 1. Starting from the entry point.
//     // 2. Traversing the upper layers.
//     // 3. Performing a wider search at layer 0.
//     // 4. Sorting the final candidates.
//     // 5. Returning the top-k results.
//     // ------------------------------------------------------------
//     vector<int> hnswResult =
//         test.search(
//             query,
//             k,
//             efSearch
//         );

//     cout << "\nHNSW:\n";

//     for (int id : hnswResult)
//     {
//         cout << id << " ";
//     }

//     cout << endl;


//     // ------------------------------------------------------------
//     // Recall@K
//     //
//     // Recall measures how many of the true nearest neighbors
//     // were successfully returned by HNSW.
//     //
//     // Example:
//     //
//     // Ground truth: 4 2 1
//     // HNSW result:  4 2 3
//     //
//     // Common results = 2
//     // Recall@3 = 2 / 3 = 0.6667
//     // ------------------------------------------------------------
//     int common = 0;

//     unordered_set<int> truth(
//         groundTruth.begin(),
//         groundTruth.end()
//     );

//     for (int id : hnswResult)
//     {
//         if (truth.count(id))
//         {
//             common++;
//         }
//     }

//     float recall =
//         static_cast<float>(common) / k;

//     cout << "\nRecall@"
//          << k
//          << ": "
//          << recall
//          << endl;


//     // ------------------------------------------------------------
//     // Validate HNSW graph structure.
//     //
//     // This checks structural invariants such as:
//     // - Valid neighbor IDs
//     // - No self-loops
//     // - No duplicate edges
//     // - Degree does not exceed M
//     // - Bidirectional edges
//     // ------------------------------------------------------------
//     test.validateGraph();


//     // ------------------------------------------------------------
//     // Inspect the neighbors of selected nodes.
//     //
//     // These are useful while debugging and understanding how
//     // the HNSW graph is constructed.
//     // ------------------------------------------------------------
//     // test.printNeighbors(4);
//     // test.printNeighbors(2);
//     // test.printNeighbors(1);

//     return 0;
// }
