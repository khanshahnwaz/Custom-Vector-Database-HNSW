Dataset:
    Indexed vectors: 10,000
    Query vectors:   100
    Dimension:        384

HNSW:
    M = 16
    efConstruction = 100

Construction:
    ~159.6 seconds

Distance:
    Pre-normalized cosine distance

efSearch results:

efSearch    Latency    Recall@10    Speedup
------------------------------------------------
10          1.13 ms      0.772       34.21x
20          2.01 ms      0.877       19.15x
50          3.91 ms      0.953        9.86x
100         7.48 ms      0.985        5.16x

Brute Force:
    38.58 ms




## M = 8
efConstruction = 100

| efSearch | Latency | Recall@10 | Speedup |
|----------|---------|-----------|---------|
| 10       | 0.725 ms | 0.614 | 47.54x |
| 20       | 1.325 ms | 0.741 | 26.02x |
| 50       | 2.908 ms | 0.883 | 11.86x |
| 100      | 5.440 ms | 0.940 | 6.34x |

Construction: 58.5 s
Distance calculations: 6,227,333


## M = 8
efConstruction = 100

| efSearch | Latency | Recall@10 | Speedup |
|----------|---------|-----------|---------|
| 10       | 0.725 ms | 0.614 | 47.54x |
| 20       | 1.325 ms | 0.741 | 26.02x |
| 50       | 2.908 ms | 0.883 | 11.86x |
| 100      | 5.440 ms | 0.940 | 6.34x |

Construction: 58.5 s
Distance calculations: 6,227,333