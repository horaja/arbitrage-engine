#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "market_event.h"
#include "symbol_registry.h"

/**
 * @class ArbitrageGraph
 * @brief Represents the cryptocurrency market as a graph to find arbitrage opportunities.
 *
 * Currencies are vertices and trading pairs are weighted, directed edges. It uses
 * the Shortest Path Faster Algorithm (SPFA), an optimization of Bellman-Ford, to
 * detect negative-weight cycles, which correspond to risk-free arbitrage.
 *
 * The graph is built once from a SymbolRegistry: because every currency and
 * symbol is known up front, the adjacency is stored in compressed-sparse-row
 * (CSR) form and each symbol's two directed edges have fixed slots. Per-quote
 * updates write edge weights directly by integer id with no string parsing,
 * hashing, or container growth on the hot path.
 */
class ArbitrageGraph {
public:
  /**
   * @brief Constructs the graph from the interned symbol/currency registry.
   */
  explicit ArbitrageGraph(const SymbolRegistry& registry);

  /**
   * @brief Updates both directional edges for a symbol from an executable quote.
   *
   * Forward edge BASE->QUOTE uses the bid (selling base for quote).
   * Reverse edge QUOTE->BASE uses 1/ask (buying base with quote).
   */
  void update_quote(std::uint32_t symbol_id, const TopOfBookQuote& quote);

  /**
   * @brief Detects and returns an arbitrage cycle if one exists.
   * @return The cycle as currency names, or nullopt if none is found.
   */
  std::optional<std::vector<std::string>> find_arbitrage_cycle();

  /**
   * @brief Gets the negative-log weight of a specific edge, or infinity if absent.
   */
  double get_edge_weight(const std::string& source_currency, const std::string& dest_currency) const;

private:
  struct Edge {
    int destination_id;
    double weight;
  };

  // Fixed edge positions for a symbol's two directed edges, plus the endpoint
  // vertex ids so dirty tracking needs no lookup.
  struct EdgeSlots {
    std::uint32_t forward_index;
    std::uint32_t reverse_index;
    int base_id;
    int quote_id;
  };

  // --- Graph structure (CSR adjacency, built once) ---
  std::vector<Edge> edges_;                  ///< Flat edge array.
  std::vector<int> row_start_;               ///< Per-vertex offsets, size num_vertices+1.
  std::vector<EdgeSlots> symbol_edge_slots_; ///< Indexed by symbol_id.
  std::vector<std::string> id_to_currency_;  ///< Vertex id -> currency name.

  int num_vertices = 0;

  // --- SPFA working state (reused across detections) ---
  std::vector<double> distance;
  std::vector<int> predecessor;
  std::vector<int> update_counts;
  std::vector<bool> in_queue;
  std::vector<int> dirty_vertices_;  ///< Vertices touched since last detection.
  std::vector<int> spfa_queue_;      ///< Reused FIFO buffer for SPFA.

  std::optional<std::vector<std::string>> reconstruct_cycle(int start_node) const;
};
