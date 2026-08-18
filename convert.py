import gzip
import json
import struct

INPUT_FILE = "wiki_minilm.ndjson.gz"
OUTPUT_FILE = "vectors.bin"

NUM_VECTORS = 10_100
DIMENSION = 384

with gzip.open(INPUT_FILE, "rt", encoding="utf-8") as fin:
    with open(OUTPUT_FILE, "wb") as fout:

        # Header
        fout.write(struct.pack("<II", NUM_VECTORS, DIMENSION))

        count = 0

        for line in fin:
            record = json.loads(line)

            embedding = record["all-MiniLM-L6-v2"]

            if len(embedding) != DIMENSION:
                raise ValueError(
                    f"Expected {DIMENSION} dimensions, "
                    f"got {len(embedding)}"
                )

            # Convert embedding values to float32
            fout.write(struct.pack(
                f"<{DIMENSION}f",
                *embedding
            ))

            count += 1

            if count % 1000 == 0:
                print(f"Processed {count} vectors")

            if count == NUM_VECTORS:
                break

print(f"\nDone. Wrote {count} vectors to {OUTPUT_FILE}")