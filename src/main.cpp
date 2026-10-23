#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Original deterministic multilevel hypergraph bipartitioner.
// Every contracted vertex retains its number of original vertices as its weight.
namespace partition {
using Gain = long long;
using Clock = std::chrono::steady_clock;

struct Budget {
    Clock::time_point start = Clock::now();
    bool expired() const {
        return std::chrono::duration<double>(Clock::now() - start).count() > 25.0;
    }
};

struct Edge {
    std::vector<int> pins;
    int weight = 1;
};

struct Graph {
    int n = 0;
    int total = 0;
    std::vector<int> weight;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> incident;
    std::vector<Gain> degree;

    void index() {
        incident.assign(n, {});
        degree.assign(n, 0);
        for (int e = 0; e < static_cast<int>(edges.size()); ++e) {
            for (int v : edges[e].pins) {
                incident[v].push_back(e);
                degree[v] += edges[e].weight;
            }
        }
    }
};

Graph read_graph(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) throw std::runtime_error("cannot open input: " + filename);
    std::string line;
    long long m = -1, n = -1;
    while (std::getline(in, line)) {
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        if (line[line.find_first_not_of(" \t\r")] == '%') continue;
        std::istringstream header(line);
        std::string extra;
        if (!(header >> m >> n) || (header >> extra) || m < 0 || n <= 0 ||
            m > std::numeric_limits<int>::max() || n > std::numeric_limits<int>::max())
            throw std::runtime_error("expected two nonnegative header counts (nets, nodes), with nodes > 0");
        break;
    }
    if (m < 0) throw std::runtime_error("missing input header");
    Graph g;
    g.n = g.total = static_cast<int>(n);
    g.weight.assign(g.n, 1);
    for (long long i = 0; i < m; ++i) {
        do {
            if (!std::getline(in, line)) throw std::runtime_error("fewer net lines than declared");
        } while (line.find_first_not_of(" \t\r") != std::string::npos &&
                 line[line.find_first_not_of(" \t\r")] == '%');
        std::istringstream row(line);
        Edge edge;
        long long v;
        for (;;) {
            row >> std::ws;
            if (row.eof()) break;
            if (!(row >> v)) throw std::runtime_error("noninteger or overflowing net entry");
            if (v < 1 || v > n) throw std::runtime_error("node identifier outside 1..nodes");
            edge.pins.push_back(static_cast<int>(v - 1));
        }
        std::sort(edge.pins.begin(), edge.pins.end());
        edge.pins.erase(std::unique(edge.pins.begin(), edge.pins.end()), edge.pins.end());
        // Empty and singleton nets can never be cut.
        if (edge.pins.size() > 1) g.edges.push_back(std::move(edge));
    }
    while (std::getline(in, line)) {
        const auto p = line.find_first_not_of(" \t\r");
        if (p != std::string::npos && line[p] != '%')
            throw std::runtime_error("more net lines than declared");
    }
    g.index();
    return g;
}

Gain cut(const Graph& g, const std::vector<int>& side) {
    Gain result = 0;
    for (const auto& e : g.edges) {
        const int s = side[e.pins.front()];
        for (int v : e.pins) {
            if (side[v] != s) { result += e.weight; break; }
        }
    }
    return result;
}

int side_weight(const Graph& g, const std::vector<int>& side) {
    int result = 0;
    for (int v = 0; v < g.n; ++v) if (side[v]) result += g.weight[v];
    return result;
}

// Indexed binary heaps keep one current entry per unlocked vertex. There is no
// accumulation of stale priority-queue entries during large-net gain updates.
class Heap {
    std::vector<int> heap_, position_;
    const std::vector<Gain>* gain_;
    bool better(int a, int b) const {
        return (*gain_)[a] != (*gain_)[b] ? (*gain_)[a] > (*gain_)[b] : a < b;
    }
    void swap_at(int a, int b) {
        std::swap(heap_[a], heap_[b]);
        position_[heap_[a]] = a;
        position_[heap_[b]] = b;
    }
    void up(int p) {
        while (p && better(heap_[p], heap_[(p - 1) / 2])) {
            const int q = (p - 1) / 2; swap_at(p, q); p = q;
        }
    }
    void down(int p) {
        for (;;) {
            int q = 2 * p + 1;
            if (q >= static_cast<int>(heap_.size())) break;
            if (q + 1 < static_cast<int>(heap_.size()) && better(heap_[q + 1], heap_[q])) ++q;
            if (!better(heap_[q], heap_[p])) break;
            swap_at(p, q); p = q;
        }
    }
public:
    Heap(int n, const std::vector<Gain>& gains) : position_(n, -1), gain_(&gains) {}
    bool empty() const { return heap_.empty(); }
    int top() const { return heap_.empty() ? -1 : heap_[0]; }
    void add(int v) { position_[v] = static_cast<int>(heap_.size()); heap_.push_back(v); up(position_[v]); }
    void erase(int v) {
        int p = position_[v];
        if (p < 0) return;
        const int last = heap_.back(); heap_.pop_back(); position_[v] = -1;
        if (p < static_cast<int>(heap_.size())) {
            heap_[p] = last; position_[last] = p;
            up(p); down(position_[last]);
        }
    }
    void changed(int v) {
        const int p = position_[v];
        if (p >= 0) { up(p); down(position_[v]); }
    }
    int feasible(int capacity, const std::vector<int>& weights) {
        if (capacity <= 0) return -1;
        std::vector<int> held;
        while (!empty() && weights[top()] > capacity) {
            const int v = top(); erase(v); held.push_back(v);
        }
        const int result = top();
        for (int v : held) add(v);
        return result;
    }
};

bool fm_pass(const Graph& g, std::vector<int>& side, int low, int high, const Budget& budget) {
    const auto start_side = side;
    const int m = static_cast<int>(g.edges.size());
    std::vector<std::array<int, 2>> count(m, {0, 0}), unique_xor(m, {0, 0});
    std::vector<unsigned char> locked(g.n, 0), locked_sides(m, 0);
    std::vector<Gain> gain(g.n, 0);
    for (int e = 0; e < m; ++e) {
        for (int v : g.edges[e].pins) {
            ++count[e][side[v]]; unique_xor[e][side[v]] ^= v + 1;
        }
        for (int v : g.edges[e].pins) {
            const int s = side[v];
            gain[v] += static_cast<Gain>(g.edges[e].weight) *
                       ((count[e][s] == 1) - (count[e][1 - s] == 0));
        }
    }
    Heap heap0(g.n, gain), heap1(g.n, gain);
    std::array<Heap*, 2> heaps = {&heap0, &heap1};
    for (int v = 0; v < g.n; ++v) heaps[side[v]]->add(v);
    int weight1 = side_weight(g, side);
    std::vector<int> moves;
    moves.reserve(g.n);
    Gain cumulative = 0, best_gain = 0;
    std::size_t best_prefix = 0;
    auto adjust = [&](int v, Gain amount) {
        if (v >= 0 && !locked[v] && amount) {
            gain[v] += amount; heaps[side[v]]->changed(v);
        }
    };
    for (int step = 0; step < g.n; ++step) {
        if ((step & 255) == 0 && budget.expired()) break;
        const int a = heap0.feasible(high - weight1, g.weight);
        const int b = heap1.feasible(weight1 - low, g.weight);
        if (a < 0 && b < 0) break;
        int v = a;
        if (v < 0 || (b >= 0 && (gain[b] > gain[v] ||
            (gain[b] == gain[v] &&
             (std::abs(2LL * (weight1 - g.weight[b]) - g.total) <
              std::abs(2LL * (weight1 + g.weight[v]) - g.total) ||
              (std::abs(2LL * (weight1 - g.weight[b]) - g.total) ==
               std::abs(2LL * (weight1 + g.weight[v]) - g.total) && b < v)))))) v = b;
        const int from = side[v], to = 1 - from;
        heaps[from]->erase(v); locked[v] = 1;
        cumulative += gain[v];
        moves.push_back(v);
        weight1 += (to ? 1 : -1) * g.weight[v];
        side[v] = to;
        for (int e : g.incident[v]) {
            const Gain ew = g.edges[e].weight;
            if (locked_sides[e] != 3) {
                if (count[e][to] == 0) {
                    for (int u : g.edges[e].pins) adjust(u, ew);
                } else if (count[e][to] == 1) {
                    adjust(unique_xor[e][to] - 1, -ew);
                }
            }
            --count[e][from]; ++count[e][to];
            unique_xor[e][from] ^= v + 1; unique_xor[e][to] ^= v + 1;
            if (locked_sides[e] != 3) {
                if (count[e][from] == 0) {
                    for (int u : g.edges[e].pins) adjust(u, -ew);
                } else if (count[e][from] == 1) {
                    adjust(unique_xor[e][from] - 1, ew);
                }
            }
            locked_sides[e] |= static_cast<unsigned char>(1 << to);
        }
        if (cumulative > best_gain) {
            best_gain = cumulative; best_prefix = moves.size();
        }
    }
    side = start_side;
    for (std::size_t i = 0; i < best_prefix; ++i) side[moves[i]] ^= 1;
    return best_gain > 0;
}

void refine(const Graph& g, std::vector<int>& side, int low, int high,
            int passes, const Budget& budget) {
    for (int p = 0; p < passes && !budget.expired(); ++p)
        if (!fm_pass(g, side, low, high, budget)) break;
}

Graph coarsen(const Graph& g, std::vector<int>& map, int round, const Budget& budget) {
    std::vector<int> order(g.n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        if (g.degree[a] != g.degree[b]) return g.degree[a] < g.degree[b];
        return round % 2 ? a > b : a < b;
    });
    const int max_weight = std::max(2, g.total / 120);
    map.assign(g.n, -1);
    std::vector<double> score(g.n, 0.0);
    std::vector<int> touched;
    Graph next;
    next.total = g.total;
    for (int v : order) {
        if ((v & 255) == 0 && budget.expired()) return {};
        if (map[v] >= 0) continue;
        touched.clear();
        for (int e : g.incident[v]) {
            const auto& edge = g.edges[e];
            // Expanding a giant hyperedge into all vertex pairs is quadratic;
            // it remains a hyperedge for the objective, but is not a matcher.
            if (edge.pins.size() > 64) continue;
            const double contribution = static_cast<double>(edge.weight) / (edge.pins.size() - 1);
            for (int u : edge.pins) {
                if (u == v || map[u] >= 0 || g.weight[u] + g.weight[v] > max_weight) continue;
                if (score[u] == 0.0) touched.push_back(u);
                score[u] += contribution;
            }
        }
        int partner = -1;
        for (int u : touched) {
            if (partner < 0 || score[u] > score[partner] ||
                (score[u] == score[partner] &&
                 (g.weight[u] < g.weight[partner] ||
                  (g.weight[u] == g.weight[partner] && u < partner)))) partner = u;
        }
        for (int u : touched) score[u] = 0.0;
        const int id = next.n++;
        map[v] = id;
        int w = g.weight[v];
        if (partner >= 0) { map[partner] = id; w += g.weight[partner]; }
        next.weight.push_back(w);
    }
    next.edges.reserve(g.edges.size());
    int edge_number = 0;
    for (const auto& edge : g.edges) {
        if ((edge_number++ & 255) == 0 && budget.expired()) return {};
        Edge e;
        e.weight = edge.weight;
        e.pins.reserve(edge.pins.size());
        for (int v : edge.pins) e.pins.push_back(map[v]);
        std::sort(e.pins.begin(), e.pins.end());
        e.pins.erase(std::unique(e.pins.begin(), e.pins.end()), e.pins.end());
        if (e.pins.size() > 1) next.edges.push_back(std::move(e));
    }
    // Identical contracted nets have identical cut state; aggregate their cost.
    std::sort(next.edges.begin(), next.edges.end(), [](const Edge& a, const Edge& b) { return a.pins < b.pins; });
    std::size_t write = 0;
    for (std::size_t i = 0; i < next.edges.size(); ++i) {
        if (write && next.edges[write - 1].pins == next.edges[i].pins) {
            next.edges[write - 1].weight += next.edges[i].weight;
        } else {
            if (write != i) next.edges[write] = std::move(next.edges[i]);
            ++write;
        }
    }
    next.edges.resize(write);
    next.index();
    return next;
}

std::vector<int> growing_partition(const Graph& g, int seed, int low, int high, const Budget& budget) {
    std::vector<int> side(g.n, 0), best;
    std::vector<int> counts(g.edges.size(), 0);
    int weight = 0;
    Gain current_cut = 0, best_cut = std::numeric_limits<Gain>::max();
    for (int step = 0; step < g.n; ++step) {
        if (budget.expired()) break;
        int chosen = -1;
        Gain chosen_gain = 0;
        for (int v = 0; v < g.n; ++v) {
            if (side[v] || weight + g.weight[v] > high) continue;
            Gain gain = 0;
            for (int e : g.incident[v]) {
                const int remaining = static_cast<int>(g.edges[e].pins.size()) - counts[e];
                gain += static_cast<Gain>(g.edges[e].weight) * ((remaining == 1) - (counts[e] == 0));
            }
            if (step == 0 && v == seed) { chosen = v; chosen_gain = gain; break; }
            if (chosen < 0 || gain * g.weight[chosen] > chosen_gain * g.weight[v] ||
                (gain * g.weight[chosen] == chosen_gain * g.weight[v] && v < chosen)) {
                chosen = v; chosen_gain = gain;
            }
        }
        if (chosen < 0) break;
        side[chosen] = 1; weight += g.weight[chosen]; current_cut -= chosen_gain;
        for (int e : g.incident[chosen]) ++counts[e];
        if (weight >= low && current_cut < best_cut) { best_cut = current_cut; best = side; }
    }
    if (!best.empty()) return best;
    // This only matters for strict bounds on tiny contracted graphs. A simple
    // subset-sum reconstruction finds a feasible coarse balance when one exists.
    if (budget.expired()) return {};
    std::vector<int> parent(high + 1, -1), previous(high + 1, -1);
    parent[0] = -2;
    for (int v = 0; v < g.n; ++v) {
        for (int s = high; s >= g.weight[v]; --s) {
            if (parent[s] == -1 && parent[s - g.weight[v]] != -1) {
                parent[s] = v; previous[s] = s - g.weight[v];
            }
        }
    }
    int target = -1;
    for (int s = low; s <= high; ++s) if (parent[s] != -1) {
        if (target < 0 || std::abs(2 * s - g.total) < std::abs(2 * target - g.total)) target = s;
    }
    if (target < 0) return {};
    side.assign(g.n, 0);
    while (target > 0) { side[parent[target]] = 1; target = previous[target]; }
    return side;
}

std::vector<int> exact_small(const Graph& g, int low, int high, const Budget& budget) {
    if (low == 0) return std::vector<int>(g.n, 0);
    std::vector<int> counts(g.edges.size(), 0), best, side(g.n, 0);
    Gain current_cut = 0, best_cut = std::numeric_limits<Gain>::max();
    int ones = 0;
    const std::uint64_t limit = std::uint64_t{1} << (g.n - 1);
    std::uint64_t last_gray = 0;
    for (std::uint64_t i = 1; i < limit; ++i) {
        const std::uint64_t gray = i ^ (i >> 1);
        std::uint64_t difference = gray ^ last_gray;
        int v = 0;
        while ((difference & 1) == 0) { ++v; difference >>= 1; }
        const int delta = side[v] ? -1 : 1;
        side[v] ^= 1; ones += delta;
        for (int e : g.incident[v]) {
            const bool before = counts[e] > 0 && counts[e] < static_cast<int>(g.edges[e].pins.size());
            counts[e] += delta;
            const bool after = counts[e] > 0 && counts[e] < static_cast<int>(g.edges[e].pins.size());
            current_cut += static_cast<Gain>(g.edges[e].weight) * (after - before);
        }
        last_gray = gray;
        if (ones >= low && ones <= high && current_cut < best_cut) {
            best_cut = current_cut; best = side;
            if (best_cut == 0) break;
        }
        if ((i & 16383) == 0 && budget.expired()) break;
    }
    return best;
}

std::vector<int> solve(const Graph& original, int low, int high, const Budget& budget) {
    if (original.n <= 22) return exact_small(original, low, high, budget);
    std::vector<int> fallback(original.n, 0);
    for (int v = 0; v < original.n / 2; ++v) fallback[v] = 1;
    std::vector<Graph> levels;
    std::vector<std::vector<int>> maps;
    levels.push_back(original);
    for (int round = 0; levels.back().n > 220 && !budget.expired(); ++round) {
        std::vector<int> map;
        Graph next = coarsen(levels.back(), map, round, budget);
        if (next.n == 0 || next.n * 10LL >= levels.back().n * 9LL) break;
        levels.push_back(std::move(next)); maps.push_back(std::move(map));
    }
    const Graph& coarse = levels.back();
    std::vector<int> order(coarse.n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return coarse.degree[a] != coarse.degree[b] ? coarse.degree[a] > coarse.degree[b] : a < b;
    });
    std::vector<int> best;
    Gain best_cut = std::numeric_limits<Gain>::max();
    const int trials = coarse.n <= 500 ? 24 : 4;
    for (int trial = 0; trial < trials && !budget.expired(); ++trial) {
        const int seed = order[(static_cast<long long>(trial) * coarse.n / trials) % coarse.n];
        std::vector<int> side;
        if (coarse.n <= 500) {
            side = growing_partition(coarse, seed, low, high, budget);
        } else {
            // A graph containing only giant nets or isolated vertices may not
            // contract. Avoid quadratic region growing on that large level.
            side.assign(coarse.n, 0);
            int weight = 0;
            for (int i = 0; i < coarse.n && weight < coarse.total / 2; ++i) {
                const int v = trial % 2 ? order[(i + seed) % coarse.n] : (i + seed) % coarse.n;
                if (weight + coarse.weight[v] <= high) {
                    side[v] = 1; weight += coarse.weight[v];
                }
            }
            if (weight < low) side.clear();
        }
        if (side.empty()) continue;
        refine(coarse, side, low, high, 10, budget);
        const Gain value = cut(coarse, side);
        if (value < best_cut) { best_cut = value; best = std::move(side); }
    }
    if (best.empty()) {
        // Retain a legal result even if coarsening or the budget prevented a
        // coarse solution. Original vertices all have unit weight.
        best = std::move(fallback);
        refine(original, best, low, high, 8, budget);
        return best;
    }
    for (int level = static_cast<int>(levels.size()) - 2; level >= 0; --level) {
        std::vector<int> fine(levels[level].n);
        for (int v = 0; v < levels[level].n; ++v) fine[v] = best[maps[level][v]];
        best = std::move(fine);
        refine(levels[level], best, low, high, level == 0 ? 12 : 5, budget);
    }
    return best;
}
} // namespace partition

int main(int argc, char** argv) {
    try {
        if (argc < 2 || argc > 3 || (argc == 3 && std::string(argv[2]) != "--strict-balance")) {
            std::cerr << "usage: Lab1 INPUT [--strict-balance]\n";
            return 2;
        }
        partition::Budget budget;
        const auto graph = partition::read_graph(argv[1]);
        // The supplied verifier floors 45% to obtain its inclusive lower bound.
        // An optional strict interpretation is useful when a real-valued bound
        // is required instead (and correctly rejects infeasible tiny graphs).
        const bool strict = argc == 3;
        const int low = static_cast<int>((45LL * graph.n + (strict ? 99 : 0)) / 100);
        const int high = graph.n - low;
        if (low > high) throw std::runtime_error("no integer partition satisfies the requested balance interval");
        const auto side = partition::solve(graph, low, high, budget);
        if (side.size() != static_cast<std::size_t>(graph.n))
            throw std::runtime_error("no balanced partition found");
        const int ones = partition::side_weight(graph, side);
        if (ones < low || ones > high) throw std::runtime_error("internal balance check failed");
        std::ofstream out("output.txt", std::ios::trunc);
        if (!out) throw std::runtime_error("cannot create output.txt in the current directory");
        for (int value : side) out << value << '\n';
        out.close();
        if (!out) throw std::runtime_error("failed while writing output.txt");
        std::cerr << "cut_nets=" << partition::cut(graph, side)
                  << " group0=" << graph.n - ones << " group1=" << ones << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
