#pragma once

#include <bits/stdc++.h>
#include "vector_store.h"
#include "distance.h"

using namespace std;

// vector<int> bruteForceKNN(
//     VectorStore& store,
//     const vector<float>& query,
//     int k
// )
// {
//     priority_queue<pair<float, int>> best;

//     for (int id = 0; id < store.size(); id++)
//     {
//         const vector<float>& candidate = store.getVector(id);

//         float dist = cosineDistance(query, candidate);

//         if (best.size() < k)
//         {
//             best.push({dist, id});
//         }
//         else if (dist < best.top().first)
//         {
//             best.pop();
//             best.push({dist, id});
//         }
//     }

//     vector<int> result;

//     while (!best.empty())
//     {
//         result.push_back(best.top().second);
//         best.pop();
//     }

//     reverse(result.begin(), result.end());

//     return result;
// }

// ------------------------------------------------------------
// BRUTE-FORCE KNN
//
// Finds the exact k nearest neighbors by comparing the query
// against every vector in the VectorStore.
//
// This is used as the ground truth for evaluating HNSW recall.
//
// store:
//     Vector storage containing all indexed embeddings.
//
// query:
//     Query embedding.
//
// k:
//     Number of nearest neighbors to return.
//
// numberOfVectors:
//     Number of vectors to scan.
//
// Complexity:
//     O(N * D + N log N)
//     where N = number of vectors
//     and D = embedding dimension.
// ------------------------------------------------------------
vector<int> bruteForceKNN(
    VectorStore& store,
    const vector<float>& query,
    int k,
    int numberOfVectors)
{
    vector<pair<float, int>> distances;

    // ------------------------------------------------------------
    // Calculate the distance from the query to every vector.
    // ------------------------------------------------------------
    for (int id = 0;
         id < numberOfVectors;
         id++)
    {
        float distance =
            cosineDistance(
                store.getVector(id),
                query
            );

        distances.push_back({
            distance,
            id
        });
    }

    // ------------------------------------------------------------
    // Sort from nearest to farthest.
    // The pair is sorted first by distance and then by ID.
    // ------------------------------------------------------------
    sort(
        distances.begin(),
        distances.end()
    );

    // ------------------------------------------------------------
    // Extract the IDs of the k closest vectors.
    // ------------------------------------------------------------
    vector<int> result;

    int resultSize =
        min(
            k,
            static_cast<int>(distances.size())
        );

    for (int i = 0;
         i < resultSize;
         i++)
    {
        result.push_back(
            distances[i].second
        );
    }

    return result;
}