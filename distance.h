#pragma once

#include <bits/stdc++.h>
using namespace std;

float euclidean(
    const vector<float>& a,
    const vector<float>& b)
{
    if (a.size() != b.size())
        throw invalid_argument("Dimension mismatch");

    float distance = 0.0f;

    for (size_t i = 0; i < a.size(); i++)
    {
        float diff = a[i] - b[i];
        distance += diff * diff;
    }

    return sqrt(distance);
}


float cosineDistance(
    const vector<float>& a,
    const vector<float>& b)
{
    if (a.size() != b.size())
        throw invalid_argument("Dimension mismatch");

    float dot = 0.0f;
   

    for (size_t i = 0; i < a.size(); i++)
    {
        dot += a[i] * b[i];
       
    }

// we have normalized our vector so we do not need to divide with the length of the vectors
// normalized vectors have length =1 




    float similarity = dot;

    // cosine distance theoritically lies in [0,2]

    return max(0.0f,1.0f - similarity);
}