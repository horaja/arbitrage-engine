/**
 * @file arbitragegraph.cpp
 * @brief Implements ArbitrageGraph negative-cycle detection over executable quotes.
 *
 * @details
 * The market is a directed graph: each currency is a vertex and each trading pair
 * contributes two directed edges (BASE -> QUOTE and QUOTE -> BASE). Triangular
 * arbitrage is a cycle whose exchange-rate product exceeds 1; taking edge weights
 * as -log(rate) turns this into a negative-weight cycle, found with SPFA (a
 * queue-based optimization of Bellman-Ford) that re-relaxes only the vertices
 * touched by recent quote updates.
 *
 * The graph is sized once from the SymbolRegistry. Adjacency is stored in CSR
 * form (a flat edge array plus per-vertex offsets) and each symbol owns fixed
 * forward/reverse edge slots, so a quote update is two array writes by id.
 */

#include "arbitragegraph.h"

#include <algorithm>
#include <cmath>
#include <limits>

double ArbitrageGraph::get_edge_weight(const std::string& source_currency,
                                       const std::string& dest_currency) const {
  const auto source_iter = std::find(id_to_currency_.begin(), id_to_currency_.end(), source_currency);
  const auto dest_iter = std::find(id_to_currency_.begin(), id_to_currency_.end(), dest_currency);
  if (source_iter == id_to_currency_.end() || dest_iter == id_to_currency_.end()) {
    return std::numeric_limits<double>::infinity();
  }

  const int source_id = static_cast<int>(source_iter - id_to_currency_.begin());
  const int dest_id = static_cast<int>(dest_iter - id_to_currency_.begin());
  for (int i = row_start_[source_id]; i < row_start_[source_id + 1]; ++i) {
    if (edges_[i].destination_id == dest_id) {
      return edges_[i].weight;
    }
  }
  return std::numeric_limits<double>::infinity();
}

ArbitrageGraph::ArbitrageGraph(const SymbolRegistry& registry) {
  num_vertices = static_cast<int>(registry.currency_count());
  const std::size_t symbol_count = registry.symbol_count();

  id_to_currency_.reserve(num_vertices);
  for (int i = 0; i < num_vertices; ++i) {
    id_to_currency_.push_back(registry.currency_name(static_cast<std::uint32_t>(i)));
  }

  // CSR build: count out-degree per vertex (one outgoing edge per symbol per
  // endpoint), prefix-sum into row offsets, then place each symbol's two edges.
  row_start_.assign(num_vertices + 1, 0);
  for (std::uint32_t s = 0; s < symbol_count; ++s) {
    const SymbolDescriptor& descriptor = registry.descriptor(s);
    ++row_start_[descriptor.base_currency_id + 1];
    ++row_start_[descriptor.quote_currency_id + 1];
  }
  for (int v = 0; v < num_vertices; ++v) {
    row_start_[v + 1] += row_start_[v];
  }

  // Edges start at +infinity so an un-quoted pair never relaxes (it behaves as
  // absent until a real quote arrives, matching incremental edge insertion).
  edges_.assign(row_start_[num_vertices], Edge{0, std::numeric_limits<double>::infinity()});
  symbol_edge_slots_.resize(symbol_count);
  std::vector<int> cursor(row_start_.begin(), row_start_.end() - 1);
  for (std::uint32_t s = 0; s < symbol_count; ++s) {
    const SymbolDescriptor& descriptor = registry.descriptor(s);
    const int base_id = static_cast<int>(descriptor.base_currency_id);
    const int quote_id = static_cast<int>(descriptor.quote_currency_id);

    const int forward_index = cursor[base_id]++;
    edges_[forward_index].destination_id = quote_id;
    const int reverse_index = cursor[quote_id]++;
    edges_[reverse_index].destination_id = base_id;

    symbol_edge_slots_[s] = EdgeSlots{
        static_cast<std::uint32_t>(forward_index),
        static_cast<std::uint32_t>(reverse_index),
        base_id,
        quote_id,
    };
  }

  distance.assign(num_vertices, 0.0);
  predecessor.assign(num_vertices, -1);
  update_counts.assign(num_vertices, 0);
  in_queue.assign(num_vertices, false);
  dirty_vertices_.reserve(2 * symbol_count);
  spfa_queue_.reserve(num_vertices > 0 ? num_vertices : 1);
}

void ArbitrageGraph::update_quote(std::uint32_t symbol_id, const TopOfBookQuote& quote) {
  const EdgeSlots& slots = symbol_edge_slots_[symbol_id];

  edges_[slots.forward_index].weight = -std::log(quote.bid_price);
  edges_[slots.reverse_index].weight = std::log(quote.ask_price);

  dirty_vertices_.push_back(slots.base_id);
  dirty_vertices_.push_back(slots.quote_id);
}

std::optional<std::vector<std::string>> ArbitrageGraph::find_arbitrage_cycle() {
  std::fill(distance.begin(), distance.end(), 0.0);
  std::fill(predecessor.begin(), predecessor.end(), -1);
  std::fill(update_counts.begin(), update_counts.end(), 0);
  std::fill(in_queue.begin(), in_queue.end(), false);

  spfa_queue_.clear();
  for (const int vertex : dirty_vertices_) {
    if (!in_queue[vertex]) {
      in_queue[vertex] = true;
      spfa_queue_.push_back(vertex);
    }
  }
  dirty_vertices_.clear();

  std::size_t head = 0;
  while (head < spfa_queue_.size()) {
    const int u = spfa_queue_[head++];
    in_queue[u] = false;

    for (int i = row_start_[u]; i < row_start_[u + 1]; ++i) {
      const int v = edges_[i].destination_id;
      const double weight = edges_[i].weight;

      if (distance[u] + weight < distance[v]) {
        distance[v] = distance[u] + weight;
        predecessor[v] = u;

        if (!in_queue[v]) {
          spfa_queue_.push_back(v);
          in_queue[v] = true;
        }

        if (++update_counts[v] >= num_vertices) {
          auto cycle = reconstruct_cycle(v);
          if (cycle) {
            return cycle;
          }
        }
      }
    }
  }

  return std::nullopt;
}

std::optional<std::vector<std::string>> ArbitrageGraph::reconstruct_cycle(int start_node) const {
  // Walk back num_vertices predecessors to land on a node guaranteed inside the
  // negative cycle, then trace the cycle once.
  int current = start_node;
  for (int i = 0; i < num_vertices; ++i) {
    if (predecessor[current] == -1) {
      return std::nullopt;
    }
    current = predecessor[current];
  }

  const int cycle_start = current;
  std::vector<int> path;
  do {
    path.push_back(current);
    if (predecessor[current] == -1) {
      return std::nullopt;
    }
    current = predecessor[current];
  } while (current != cycle_start);
  path.push_back(cycle_start);
  std::reverse(path.begin(), path.end());

  std::vector<std::string> cycle;
  cycle.reserve(path.size());
  for (const int node_id : path) {
    cycle.push_back(id_to_currency_[node_id]);
  }
  return cycle;
}
