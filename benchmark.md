
<!-- Same algorithm but no reference, everytime createing the new address for the vector  -->
Brute Force Benchmark
---------------------
Vectors: 10000
Dimension: 384
Queries: 100
K: 10

Average latency: 80720.2 microseconds
Minimum latency: 70834 microseconds
Maximum latency: 182783 microseconds
Average latency: 80.7202 ms

<!-- Instead of creating a copy of vector every time we called getVector, we stored the constant reference and used the memory where vector was already stored, in RAM -->

Dataset:       10,000 vectors
Dimension:     384
Metric:        Cosine distance
Queries:       100
K:             10

Brute-force:
Average:       70.97 ms/query
Minimum:       67.43 ms
Maximum:       84.53 ms