#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstdint>

/**
 * @class ArbitrageGraph
 * @brief Represents the cryptocurrency market as a graph to find arbitrage opportunities.
 *
 * This class models currencies as vertices and trading pairs as weighted, directed edges.
 * It uses the Shortest Path Faster Algorithm (SPFA), an optimization of Bellman-Ford,
 * to detect negative weight cycles, which correspond to risk-free arbitrage
 * opportunities in the market.
 */
class ArbitrageGraph {
public:
  /**
   * @brief Constructs the graph with an initial set of trading symbols.
   * @param symbols A vector of strings representing trading pairs (e.g., "BTC-USD").
   */
  ArbitrageGraph(const std::vector<std::string>& symbols);

  /**
   * @brief Updates an edge's weight based on a new price tick.
   * @param symbol The trading pair with a new price.
   * @param price The new market price.
   */
  void update_price(const std::string& symbol, double price);

  /**
   * @brief Detects and returns an arbitrage cycle if one exists.
   * @return An optional containing the cycle as a vector of currency strings,
   * or nullopt if no opportunity is found.
   */
  std::optional<std::vector<std::string>> find_arbitrage_cycle();

private:
  /**
   * @struct Edge
   * @brief Represents a directed edge in the graph.
   */
  struct Edge {
      int destination_id;
      double weight;
  };

  // --- Graph Structure ---
  
  /// @brief Adjacency list representation of the graph.
  std::vector<std::vector<Edge>> adjacency_list;
  
  /// @brief Maps currency string names to their unique integer IDs.
  std::unordered_map<std::string, int> currency_to_id;
  
  /// @brief Maps unique integer IDs back to their currency string names.
  std::vector<std::string> id_to_currency;
  
  /// @brief Provides O(1) lookup for edge weights to avoid linear scans.
  std::unordered_map<uint64_t, size_t> edge_index_map;

  // --- SPFA Algorithm Data ---
  
  /// @brief The total number of unique currencies (vertices) in the graph.
  int num_vertices = 0;
  
  /// @brief Stores the shortest distance from the source to each vertex.
  std::vector<double> distance;
  
  /// @brief Stores the predecessor of each vertex in the shortest path tree.
  std::vector<int> predecessor;
  
  /// @brief Counts updates to each vertex's distance to detect negative cycles.
  std::vector<int> update_counts;
  
  /// @brief Queue of vertices whose distances have been updated, for SPFA optimization.
  std::deque<int> dirty_vertices;

  // --- Private Helper Functions ---

  /**
   * @brief Creates a unique 64-bit key for a directed edge.
   * @param source_id The integer ID of the source vertex.
   * @param destination_id The integer ID of the destination vertex.
   * @return A unique 64-bit integer key.
   */
  uint64_t create_edge_key(int source_id, int destination_id) const;

  /**
   * @brief Reconstructs the arbitrage cycle path from the predecessor list.
   * @param start_node A node within the detected negative cycle.
   * @return A vector of currency strings representing the arbitrage path.
   */
  std::vector<std::string> reconstruct_cycle(int start_node) const;
};