
#pragma once

#include <string>
#include <cstdint>

#include "metadata_store.h"

using namespace std;


// Conditions applied during filtered search.
struct SearchFilter
{
    string title;
    string category;
    string source;
    string documentId;
};


// Search result returned to the application.
struct SearchResult
{
    uint32_t id;
    float distance;

    // Indicates whether metadata exists for this vector.
    bool hasMetadata;

    Metadata metadata;
};
