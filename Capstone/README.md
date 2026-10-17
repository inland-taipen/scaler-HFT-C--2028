# Capstone: Low-Latency Order Book

**SST Email:** anika.24bcs10409@sst.scaler.com
**Roll Number:** 10409
**Name:** Anika Tripathi

## Overview
This repository contains a single-instrument, low-latency order book implemented in three distinct ways using C++17. The implementations are benchmarked and verified against each other to ensure identical behavior.

## Data Structures
*   **Implementation A (`main_ob/book_map.hpp`)**: Uses `std::map` for price levels and `std::unordered_map` for orders.
*   **Implementation B (`main_ob/book_vec.hpp`)**: Uses a sorted `std::vector` for price levels and `std::unordered_map` for orders.
*   **Implementation C (`main_ob/book_array.hpp`)**: Uses a pre-allocated flat array indexed by price tick, a dynamically growing vector for orders, and a linear rescan for finding the next best price.

## Interface Changes (Important)
The required API specifies `double` for prices. To avoid floating-point representation errors (e.g., `0.29 * 100` truncating to `28` instead of `29`), **this implementation uses integer ticks internally (`int32_t`)**. 
* Prices are converted using `std::llround(price * 100.0)`.
* `Level` and `BBO` structs store `Tick` (int32) instead of `double`.
* The conversion back to `double` only happens for printing/display.

## How to Build

**Option 1: CMake**
```bash
mkdir -p build
cd build
cmake ..
make