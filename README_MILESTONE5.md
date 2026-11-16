# Milestone 5 — Document Ingestion

This source bundle adds a first end-to-end TXT document ingestion path to the existing C++ vector database.

## What is included

- `ingest_txt.py`: the Python embedding/chunking script supplied for this project.
- `vector_store.h/.cpp`: append vectors, normalize and validate them, and atomically persist the complete vector file.
- `metadata_store.h/.cpp`: persist chunk text and chunk index in metadata format v2; continue loading existing v1 metadata files.
- `vector_database.h/.cpp`: read the Python vector binary and JSONL metadata, validate matching counts/dimensions, assign sequential IDs, insert into HNSW, and persist.
- `hnsw.h`: reject negative, out-of-range, duplicate, or non-sequential node IDs.
- `main.cpp`: adds a command-line ingestion mode while retaining the existing search/filter demonstration.
- `distance.h`, `brute_force.h`, `search_types.h`: the project's matching headers, included for a complete source bundle.

## Build (Windows PowerShell)

Run from the project directory after replacing the corresponding source files with this bundle:

```powershell
g++ -std=c++14 -O2 main.cpp vector_database.cpp vector_store.cpp metadata_store.cpp -o vector_db.exe
```

The existing `hnsw.h` uses generic lambdas, which require C++14. Use `-std=c++14`, not `-std=c++11`.

## Back up the current database first

Use new backup names if these already exist:

```powershell
Copy-Item vectors.bin vectors_before_milestone5.bin
Copy-Item hnsw.index hnsw_before_milestone5.index
Copy-Item metadata.bin metadata_before_milestone5.bin
```

Keep `queries.bin` separate. It is an evaluation-query file and must not be appended to the corpus.

## Generate embeddings

For a TXT document:

```powershell
python ingest_txt.py .\sample.txt
```

This writes `ingestion_vectors.bin` and `ingestion_metadata.jsonl` in the current directory. The vector file uses the existing binary layout: two uint32 header values (count, dimension), followed by float32 vectors. The JSONL contains one metadata object per chunk.

## Ingest the generated batch

```powershell
.\vector_db.exe --ingest ingestion_vectors.bin ingestion_metadata.jsonl
```

The database assigns IDs starting at the current `vectors.bin` count. For the current 10,000-vector corpus and one-chunk test batch, the expected new vector ID is **10000**, and the expected corpus/index size is **10001**.

The ingestion command prints nearest-neighbor results for the new chunk, including its stored text. It also persists the vector file, metadata, and HNSW index.

Run the same command a second time to verify restart loading and duplicate protection. The same document UUID/chunk index is recognized, and no second copy is appended.

## Compatibility and behavior

- Existing metadata format v1 files load normally. When saved, they are rewritten as v2. Existing four fields are preserved; old records have empty `text` and `chunkIndex` defaults to zero.
- Metadata v2 stores `chunkIndex`, the existing title/category/source/document ID, and the chunk `text`.
- `vectors.bin` is the corpus. All vectors in it are indexed. `queries.bin` remains separate and is not included in HNSW.
- The constructor's `initialVectorCount` argument is retained for source compatibility with the existing `main.cpp` call. It is now a baseline validation value; the actual index size is taken from the current corpus file.
- If startup finds an HNSW index whose stored vector count differs from the current vector file, it rebuilds the index. Other index corruption errors are not silently ignored.
- Filter semantics remain exact, case-sensitive, and AND-based. HNSW candidate generation remains approximate; over-fetching does not guarantee exact global filtered top-K results.

## Persistence note

Each of the three files is written through a temporary file and replaced individually. The three-file update is not a single filesystem transaction. The index is written last; if a process is interrupted after the vector file is replaced, startup detects the stale index count and rebuilds it. Keep the generated ingestion batch files until you have verified the successful ingestion and reload.
