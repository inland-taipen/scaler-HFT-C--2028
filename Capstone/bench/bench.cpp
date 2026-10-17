#include "types.hpp"
#include "book_map.hpp"
#include "book_vec.hpp"
#include "workload.hpp"
#include "book_array.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

using namespace ob;
using Clock = std::chrono::steady_clock;

struct RunResult {
    double ns_add;
    double ns_cancel;
    double ns_bbo;
    double ns_depth;
    long long checksum;
    BBO final_bbo;
};

template <class Book>
RunResult run_once(const std::vector<Op>& ops) {
    Book book;

    uint64_t add_ns = 0, cancel_ns = 0, bbo_ns = 0, depth_ns = 0;
    uint64_t add_n  = 0, cancel_n  = 0, bbo_n  = 0, depth_n  = 0;
    long long checksum = 0;
    BBO final_bbo{};

    for (const Op& op : ops) {
        auto t0 = Clock::now();
        switch (op.type) {
            case OpType::Add:
                book.add_order(op.id, op.side, op.price, op.qty);
                ++add_n;
                break;
            case OpType::Cancel:
                book.cancel_order(op.id);
                ++cancel_n;
                break;
            case OpType::Bbo: {
                BBO b = book.get_bbo();
                checksum += b.bid_qty + b.ask_qty;
                final_bbo = b;
                ++bbo_n;
                break;
            }
            case OpType::Depth: {
                auto bids = book.get_bids(10);
                auto asks = book.get_asks(10);
                for (auto& l : bids) checksum += l.qty;
                for (auto& l : asks) checksum += l.qty;
                ++depth_n;
                break;
            }
        }
        auto t1 = Clock::now();
        uint64_t dt = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        switch (op.type) {
            case OpType::Add:    add_ns    += dt; break;
            case OpType::Cancel: cancel_ns += dt; break;
            case OpType::Bbo:    bbo_ns    += dt; break;
            case OpType::Depth:  depth_ns  += dt; break;
        }
    }

    RunResult r{};
    r.ns_add    = add_n    ? (double)add_ns    / add_n    : 0.0;
    r.ns_cancel = cancel_n ? (double)cancel_ns / cancel_n : 0.0;
    r.ns_bbo    = bbo_n    ? (double)bbo_ns    / bbo_n    : 0.0;
    r.ns_depth  = depth_n  ? (double)depth_ns  / depth_n  : 0.0;
    r.checksum  = checksum;
    r.final_bbo = final_bbo;
    return r;
}

static double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

template <class Book>
void bench_impl(const std::vector<Op>& ops, int runs, const char* name) {
    std::vector<double> a, c, b, d;
    RunResult last{};

    for (int i = 0; i < runs; ++i) {
        last = run_once<Book>(ops);
        a.push_back(last.ns_add);
        c.push_back(last.ns_cancel);
        b.push_back(last.ns_bbo);
        d.push_back(last.ns_depth);
    }

    std::printf("impl=%s runs=%d\n", name, runs);
    std::printf("  ns/add    %.1f\n", median(a));
    std::printf("  ns/cancel %.1f\n", median(c));
    std::printf("  ns/bbo    %.1f\n", median(b));
    std::printf("  ns/depth  %.1f\n", median(d));
    std::printf("  final bid: has=%d px=%d qty=%lld\n",
                last.final_bbo.has_bid,
                last.final_bbo.bid_price,
                (long long)last.final_bbo.bid_qty);
    std::printf("  final ask: has=%d px=%d qty=%lld\n",
                last.final_bbo.has_ask,
                last.final_bbo.ask_price,
                (long long)last.final_bbo.ask_qty);
    std::printf("  checksum %lld\n", last.checksum);
}

template <class BookA, class BookB>
bool verify_pair(const std::vector<Op>& ops, const char* nameA, const char* nameB) {
    BookA a;
    BookB b;

    for (size_t i = 0; i < ops.size(); ++i) {
        const Op& op = ops[i];

        switch (op.type) {
            case OpType::Add:
                a.add_order(op.id, op.side, op.price, op.qty);
                b.add_order(op.id, op.side, op.price, op.qty);
                break;

            case OpType::Cancel: {
                bool ra = a.cancel_order(op.id);
                bool rb = b.cancel_order(op.id);
                if (ra != rb) {
                    std::printf("MISMATCH cancel return at op %zu (id=%llu): %s=%d %s=%d\n",
                                i, (unsigned long long)op.id, nameA, ra, nameB, rb);
                    return false;
                }
                break;
            }

            case OpType::Bbo:
                break;   // covered by the post-op check below

            case OpType::Depth:
                break;   // covered by the post-op check below
        }

        BBO ba = a.get_bbo();
        BBO bb = b.get_bbo();
        if (!(ba == bb)) {
            std::printf("MISMATCH bbo after op %zu (type=%d id=%llu)\n",
                        i, (int)op.type, (unsigned long long)op.id);
            std::printf("  %s: has_bid=%d bid=%d x %lld  has_ask=%d ask=%d x %lld\n",
                        nameA, ba.has_bid, ba.bid_price, (long long)ba.bid_qty,
                        ba.has_ask, ba.ask_price, (long long)ba.ask_qty);
            std::printf("  %s: has_bid=%d bid=%d x %lld  has_ask=%d ask=%d x %lld\n",
                        nameB, bb.has_bid, bb.bid_price, (long long)bb.bid_qty,
                        bb.has_ask, bb.ask_price, (long long)bb.ask_qty);
            return false;
        }

        std::vector<Level> ba_bids = a.get_bids(10);
        std::vector<Level> bb_bids = b.get_bids(10);
        if (ba_bids != bb_bids) {
            std::printf("MISMATCH get_bids(10) after op %zu (type=%d)\n",
                        i, (int)op.type);
            return false;
        }

        std::vector<Level> ba_asks = a.get_asks(10);
        std::vector<Level> bb_asks = b.get_asks(10);
        if (ba_asks != bb_asks) {
            std::printf("MISMATCH get_asks(10) after op %zu (type=%d)\n",
                        i, (int)op.type);
            return false;
        }
    }

    std::printf("verify %s vs %s: PASS\n", nameA, nameB);
    return true;
}

int main(int argc, char** argv) {
    std::string impl = "A";
    int runs = 5;
    uint32_t seed = 42;
    size_t n = 1'000'000;
    bool verify = false;

    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--impl") && i + 1 < argc) impl = argv[++i];
        else if (!std::strcmp(argv[i], "--runs") && i + 1 < argc) runs = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--verify")) verify = true;
    }

    if (verify) {
        auto ops = make_workload(100'000, seed);
        bool ok = true;
        ok &= verify_pair<BookMap, BookVec>(ops, "A", "B");
        ok &= verify_pair<BookMap, BookArray>(ops, "A", "C");
        ok &= verify_pair<BookVec, BookArray>(ops, "B", "C"); 
        return ok ? 0 : 1;
    }

    auto ops = make_workload(n, seed);

    if (impl == "A") bench_impl<BookMap>(ops, runs, "A");
    else if (impl == "B") bench_impl<BookVec>(ops, runs, "B");
    else if (impl == "C") bench_impl<BookArray>(ops, runs, "C");
    else {
        std::fprintf(stderr, "unknown impl: %s\n", impl.c_str());
        return 1;
    }
    return 0;
}