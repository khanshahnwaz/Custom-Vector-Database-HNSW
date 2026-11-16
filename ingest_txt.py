
import argparse
import json
import os
import struct
import uuid

import numpy as np
from sentence_transformers import SentenceTransformer


MODEL_NAME = "sentence-transformers/all-MiniLM-L6-v2"
DIMENSION = 384

# Chunk configuration
CHUNK_WORDS = 180
OVERLAP_WORDS = 30

VECTOR_OUTPUT = "ingestion_vectors.bin"
METADATA_OUTPUT = "ingestion_metadata.jsonl"


def read_document(filename):
    """Read a UTF-8 text document."""
    with open(filename, "r", encoding="utf-8") as file:
        text = file.read()

    text = text.strip()

    if not text:
        raise ValueError("The input document is empty.")

    return text


def split_into_chunks(text):
    """
    Split text into word-based chunks with overlap.

    Example:
        Chunk 1: words 0-179
        Chunk 2: words 150-329
        Chunk 3: words 300-479
    """
    words = text.split()

    if not words:
        return []

    chunks = []
    start = 0

    while start < len(words):
        end = min(start + CHUNK_WORDS, len(words))

        chunk = " ".join(words[start:end])

        if chunk.strip():
            chunks.append(chunk)

        if end == len(words):
            break

        start = end - OVERLAP_WORDS

    return chunks


def generate_embeddings(model, chunks):
    """Generate 384-dimensional embeddings."""
    embeddings = model.encode(
        chunks,
        convert_to_numpy=True,
        normalize_embeddings=False,
        show_progress_bar=True
    )

    if embeddings.ndim != 2:
        raise ValueError("Unexpected embedding output shape.")

    if embeddings.shape[1] != DIMENSION:
        raise ValueError(
            "Expected embedding dimension "
            f"{DIMENSION}, got {embeddings.shape[1]}"
        )

    if not np.isfinite(embeddings).all():
        raise ValueError("Embeddings contain invalid numeric values.")

    return embeddings.astype(np.float32)


def write_vector_file(embeddings, filename):
    """Write vectors using the existing VectorStore binary format."""
    temp_filename = filename + ".tmp"

    try:
        with open(temp_filename, "wb") as file:
            count = len(embeddings)

            # Binary header: vector count, dimension
            file.write(struct.pack("<II", count, DIMENSION))

            for embedding in embeddings:
                file.write(
                    struct.pack(
                        f"<{DIMENSION}f",
                        *embedding.tolist()
                    )
                )

        os.replace(temp_filename, filename)

    except Exception:
        if os.path.exists(temp_filename):
            os.remove(temp_filename)
        raise


def write_metadata(chunks, document_id, document_title, source):
    """Write one JSON metadata record per chunk."""
    temp_filename = METADATA_OUTPUT + ".tmp"

    try:
        with open(temp_filename, "w", encoding="utf-8") as file:
            for chunk_index, chunk in enumerate(chunks):
                record = {
                    "chunk_index": chunk_index,
                    "title": document_title,
                    "category": "TXT",
                    "source": source,
                    "document_id": document_id,
                    "text": chunk
                }

                file.write(json.dumps(record, ensure_ascii=False))
                file.write("\n")

        os.replace(temp_filename, METADATA_OUTPUT)

    except Exception:
        if os.path.exists(temp_filename):
            os.remove(temp_filename)
        raise


def main():
    parser = argparse.ArgumentParser(
        description="Generate vector embeddings from a TXT document."
    )

    parser.add_argument(
        "input_file",
        help="Path to the TXT document"
    )

    args = parser.parse_args()

    if not args.input_file.lower().endswith(".txt"):
        raise ValueError("Only TXT documents are supported.")

    if not os.path.isfile(args.input_file):
        raise FileNotFoundError(args.input_file)

    document_path = os.path.abspath(args.input_file)
    document_title = os.path.splitext(
        os.path.basename(document_path)
    )[0]

    document_id = str(uuid.uuid4())

    print("Loading document...")
    text = read_document(document_path)

    print("Splitting document into chunks...")
    chunks = split_into_chunks(text)

    if not chunks:
        raise ValueError("No valid chunks were generated.")

    print(f"Generated chunks: {len(chunks)}")

    print("\nLoading embedding model...")
    model = SentenceTransformer(MODEL_NAME)

    if model.get_sentence_embedding_dimension() != DIMENSION:
        raise ValueError("Loaded model has an unexpected dimension.")

    print("Generating embeddings...")
    embeddings = generate_embeddings(model, chunks)

    print("\nWriting vector file...")
    write_vector_file(embeddings, VECTOR_OUTPUT)

    print("Writing metadata file...")
    write_metadata(
        chunks,
        document_id,
        document_title,
        os.path.basename(document_path)
    )

    print("\nDocument processing completed.")
    print(f"Document ID: {document_id}")
    print(f"Chunks generated: {len(chunks)}")
    print(f"Embedding dimension: {DIMENSION}")
    print(f"Vector file: {VECTOR_OUTPUT}")
    print(f"Metadata file: {METADATA_OUTPUT}")


if __name__ == "__main__":
    main()
