
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdio>
#include <limits>

using namespace std;

bool fileExists(const string& filename) {
    ifstream file(filename.c_str(), ios::binary);
    return file.good();
}

uint64_t getExpectedSize(uint32_t count, uint32_t dimension) {
    return sizeof(uint32_t) * 2ULL +
           static_cast<uint64_t>(count) * dimension * sizeof(float);
}

void writeHeader(ofstream& output, uint32_t count, uint32_t dimension) {
    output.write(reinterpret_cast<const char*>(&count), sizeof(count));
    output.write(reinterpret_cast<const char*>(&dimension), sizeof(dimension));

    if (!output) {
        throw runtime_error("Failed to write output header.");
    }
}

void copyVectors(
    ifstream& input,
    ofstream& output,
    uint32_t count,
    uint32_t dimension
) {
    vector<float> buffer(dimension);

    for (uint32_t i = 0; i < count; i++) {
        input.read(
            reinterpret_cast<char*>(buffer.data()),
            static_cast<streamsize>(dimension * sizeof(float))
        );

        if (!input) {
            throw runtime_error("Failed to read vector " + to_string(i));
        }

        output.write(
            reinterpret_cast<const char*>(buffer.data()),
            static_cast<streamsize>(dimension * sizeof(float))
        );

        if (!output) {
            throw runtime_error("Failed to write vector " + to_string(i));
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        cout << "Usage:\n";
        cout << "split_vectors.exe <input> <corpus_output> "
             << "<queries_output> <corpus_count> <query_count>\n";
        return 1;
    }

    string inputFile = argv[1];
    string corpusFile = argv[2];
    string queriesFile = argv[3];

    uint32_t corpusCount = static_cast<uint32_t>(stoul(argv[4]));
    uint32_t queryCount = static_cast<uint32_t>(stoul(argv[5]));

    string corpusTemp = corpusFile + ".tmp";
    string queriesTemp = queriesFile + ".tmp";

    try {
        if (inputFile == corpusFile || inputFile == queriesFile ||
            corpusFile == queriesFile) {
            throw runtime_error("Input and output filenames must be different.");
        }

        if (fileExists(corpusFile) || fileExists(queriesFile) ||
            fileExists(corpusTemp) || fileExists(queriesTemp)) {
            throw runtime_error(
                "An output or temporary file already exists. "
                "Rename or remove it before running."
            );
        }

        ifstream input(inputFile.c_str(), ios::binary);

        if (!input) {
            throw runtime_error("Could not open input file: " + inputFile);
        }

        uint32_t totalCount = 0;
        uint32_t dimension = 0;

        input.read(reinterpret_cast<char*>(&totalCount), sizeof(totalCount));
        input.read(reinterpret_cast<char*>(&dimension), sizeof(dimension));

        if (!input || totalCount == 0 || dimension == 0) {
            throw runtime_error("Invalid input header.");
        }

        if (corpusCount > totalCount ||
            queryCount > totalCount - corpusCount ||
            corpusCount + queryCount != totalCount) {
            throw runtime_error(
                "Corpus count + query count must equal input vector count."
            );
        }

        uint64_t expectedSize = getExpectedSize(totalCount, dimension);

        input.seekg(0, ios::end);
        streamoff actualSize = input.tellg();

        if (actualSize < 0 ||
            static_cast<uint64_t>(actualSize) != expectedSize) {
            throw runtime_error(
                "Input file size does not match its header."
            );
        }

        input.seekg(sizeof(uint32_t) * 2, ios::beg);

        ofstream corpusOutput(corpusTemp.c_str(), ios::binary);
        ofstream queriesOutput(queriesTemp.c_str(), ios::binary);

        if (!corpusOutput || !queriesOutput) {
            throw runtime_error("Could not create temporary output files.");
        }

        writeHeader(corpusOutput, corpusCount, dimension);
        writeHeader(queriesOutput, queryCount, dimension);

        copyVectors(input, corpusOutput, corpusCount, dimension);
        copyVectors(input, queriesOutput, queryCount, dimension);

        corpusOutput.flush();
        queriesOutput.flush();

        if (!corpusOutput || !queriesOutput) {
            throw runtime_error("Failed to flush output files.");
        }

        corpusOutput.close();
        queriesOutput.close();
        input.close();

        if (corpusOutput.fail() || queriesOutput.fail()) {
            throw runtime_error("Failed to close output files correctly.");
        }

        if (rename(corpusTemp.c_str(), corpusFile.c_str()) != 0) {
            throw runtime_error("Could not finalize corpus output file.");
        }

        if (rename(queriesTemp.c_str(), queriesFile.c_str()) != 0) {
            remove(corpusFile.c_str());
            throw runtime_error("Could not finalize queries output file.");
        }

        cout << "\nMigration completed successfully.\n";
        cout << "Original vectors : " << totalCount << "\n";
        cout << "Dimensions       : " << dimension << "\n";
        cout << "Corpus vectors   : " << corpusCount << "\n";
        cout << "Query vectors    : " << queryCount << "\n";
        cout << "Corpus file      : " << corpusFile << "\n";
        cout << "Queries file     : " << queriesFile << "\n";

    } catch (const exception& e) {
        remove(corpusTemp.c_str());
        remove(queriesTemp.c_str());

        cerr << "\nMigration failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
