## Results Table

| Metric | Implementation A (std::map) | Implementation B (sorted vector) | Implementation C (flat array) |
| :--- | :--- | :--- | :--- |
| **Total time (ms)** | 1494.6 | 1128.7 | **572.4** |
| **ns / add** | 98.7 | 78.6 | **22.4** |
| **ns / cancel** | 265.4 | 164.3 | **26.9** |
| **ns / BBO** | 16.3 | 16.6 | 16.8 |
| **IPC** | N/A* | N/A* | N/A* |
| **Branch misses (%)** | N/A* | N/A* | N/A* |
| **L1-dcache-load-misses**| N/A* | N/A* | N/A* |
| **Cache misses** | N/A* | N/A* | N/A* |
| **Page faults** | 16,715 | 16,623 | **57,387** |

*\*Note: The benchmark was run natively on macOS and inside GitHub Codespaces (Azure). Both hypervisors restrict access to hardware Performance Monitoring Units (PMU). `perf stat` reported `<not supported>` for hardware counters (`cycles`, `cache-misses`). The analysis relies on available software counters (`task-clock`, `page-faults`), the profiler (`perf record`), and direct execution timings.*

## 11.1 Which implementation is fastest?
Implementation C is the fastest, finishing the workload in 572ms compared to A's 1494ms. It is the fastest for `add_order` (22.4 ns) and `cancel_order` (26.9 ns). 
*Why?* C uses a flat array indexed by price, resulting in O(1) lookups and zero `malloc` calls during the hot path. Implementation A relies on `std::map` tree traversal, which involves pointer chasing and heap allocations. Implementation B requires memory shifting (`memmove`) during inserts, which grows linearly with the number of active price levels.

## 11.2 Branch Prediction
*(Hardware counters were unavailable, but we can infer from the code and `perf record`.)*
Implementation A logically suffers from the most branch misses. `std::map` is a Red-Black tree, meaning every lookup and insertion traverses a different path of nodes. These data-dependent branches are highly unpredictable. Implementation C has almost no branches in its hot path, as it directly indexes into an array using arithmetic.

## 11.3 Cache Behavior
Implementation A has the worst cache behavior. Every new price level in A triggers a call to `malloc`, allocating memory at random locations on the heap. This pointer chasing constantly evicts L1 and L2 caches. The `perf record` output for A confirms heavy time spent in memory management (`malloc`, `cfree`, `do_page_fault`). Implementation C is the exact opposite: the flat array is contiguous and only ~80KB in size, fitting comfortably inside the L2 cache. The hardware prefetcher can easily predict and load adjacent memory.

## 11.4 Page Faults
Interestingly, **Implementation C has the most page faults (57,387)** while A and B have roughly 16,600. 
*Why?* This is a side effect of C's design. C pre-allocates a massive `std::vector<Order>` and resizes it dynamically to handle order IDs. Allocating this large contiguous block upfront causes the OS to page in memory immediately. A, using `std::unordered_map`, allocates many small nodes on the heap over time. While C incurs more page faults upfront, its overall execution time is so fast (572ms) that the OS handles this memory efficiently. In a true production environment, C would pre-reserve the exact order capacity to avoid this entirely.

## 11.5 Production Choice
I would choose **Implementation C** for production. 
*   **Latency:** C's average latency is 3x lower. 
*   **Predictability:** C's tail latency is predictable. It does not suffer from the unpredictable latency spikes caused by `malloc` in A. 
*   **Cache Behavior:** C's contiguous memory layout is optimal for modern CPUs and hardware prefetchers.
*   **Maintainability:** While A is easier to write, C is highly predictable, easy to reason about, and its flat-array design is the industry standard for HFT systems.

## 11.6 What Would You Try Next?
Based on the `perf record` output, a major discovery is that **`__kernel_clock_gettime` consumed 65.84% of the execution time** in Implementation A. The timer overhead (`std::chrono::steady_clock::now()`) is bottlenecking the benchmark itself. In a production system, I would use a faster hardware timer (like TSC) to avoid this overhead. 
For the algorithm, C currently uses a linear `while` loop to find the next best price when the top level is cancelled. I would optimize this by adding a two-level bitmap (bitset) to track non-zero levels. Using `std::countr_zero`, finding the next best price would become O(1) regardless of how many levels are scanned.