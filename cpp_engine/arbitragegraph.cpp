/**
 * @file arbitragegraph.cpp
 * @brief Implements the ArbitrageGraph class for detecting cryptocurrency arbitrage opportunities.
 *
 * @details
 * This file contains the implementation of the ArbitrageGraph class, which is the core of the
 * arbitrage detection engine. The fundamental idea is to represent the cryptocurrency market
 * as a directed graph where:
 * - Each currency (e.g., BTC, ETH, USD) is a **vertex** (or node).
 * - Each trading pair (e.g., BTC-USD) represents two directed **edges** between the
 * corresponding currency vertices (e.g., BTC -> USD and USD -> BTC).
 *
 * The goal of triangular arbitrage is to find a sequence of trades (a cycle in the graph)
 * that results in a profit. For example, starting with USD, buying BTC, selling the BTC
 * for ETH, and then selling the ETH back to USD.
 *
 * This problem can be modeled as finding a "negative weight cycle" in the graph.
 * If the edge weights were simply the exchange rates, we would look for a cycle where the
 * product of rates is greater than 1. However, by transforming the edge weights using the
 * negative logarithm of the exchange rates (-log(rate)), the problem becomes finding a
 * cycle where the sum of weights is negative. This is a classic graph theory problem that
 * can be solved efficiently.
 *
 * This implementation uses the Shortest Path Faster Algorithm (SPFA), which is an
 * optimization of the Bellman-Ford algorithm. SPFA is particularly well-suited for this
 * use case because it efficiently handles sparse graphs (graphs with relatively few edges
 * compared to vertices) and can quickly re-evaluate the graph as new price ticks arrive,
 * without recomputing everything from scratch.
 */

#include "arbitragegraph.h"
#include <set>
#include <cmath>
#include <iostream>
#include <limits>

/**
 * @brief Creates a unique 64-bit key for an edge.
 * 
 * @param source_id The integer ID of the source currency vertex.
 * @param dest_id The integer ID of the destination currency vertex.
 * @return A unique 64-bit integer key representing the directed edge.
 */
uint64_t ArbitrageGraph::create_edge_key(int source_id, int dest_id) const {
  return (static_cast<uint64_t>(source_id) << 32) | dest_id;
}

/**
 * @brief Constructs the ArbitrageGraph.
 *
 * This constructor initializes the graph structure. It identifies all unique currencies
 * from the list of trading pairs, assigns each a unique integer ID, and sets up the
 * data structures needed for the SPFA algorithm.
 *  
 * @param symbols A vector of strings, where each string is a trading pair (e.g., "BTC-USD").
 */
ArbitrageGraph::ArbitrageGraph(const std::vector<std::string>& symbols) {

  /* Fill set of currency names */
  std::set<std::string> unique_currencies;
  for (const auto& symbol : symbols) {
    size_t delimiter_pos = symbol.find('-');
    if (delimiter_pos != std::string::npos) {
      unique_currencies.insert(symbol.substr(0, delimiter_pos));
      unique_currencies.insert(symbol.substr(delimiter_pos+1));
    }
  }

  /* 2 maps for stirng->int conversion of currency names */
  this->num_vertices = unique_currencies.size();
  int current_id = 0;
  for (const auto& currency_name : unique_currencies) {
    this->currency_to_id[currency_name] = current_id;
    this->id_to_currency.push_back(currency_name);
    current_id++;
  }

  /* Data structure initialization for SPFA */
  this->adjacency_list.resize(num_vertices);
  this->distance.resize(num_vertices, std::numeric_limits<double>::infinity());
  this->predecessor.resize(num_vertices, -1);
  this->update_counts.resize(num_vertices, 0);
  
  this->distance[0] = 0.0;
  
}

/**
 * @brief Updates the graph with a new price tick.
 * 
 * This function is called every time a new trade occurs in the market. It updates the
 * weights of the two corresponding edges in the graph (e.g., BTC -> USD and USD -> BTC)
 * and marks the affected vertices as "dirty", so that the `find_arbitrage_cycle`
 * function knows to re-evaluate them.
 * 
 * @param symbol The trading pair that has a new price (e.g., "BTC-USD").
 * @param price The new price for the trading pair.
 */
void ArbitrageGraph::update_price(const std::string& symbol, double price) {

  /* Get Ids of input symbols */
  size_t delimiter_pos = symbol.find('-');
  if (delimiter_pos == std::string::npos) {
    throw std::runtime_error("Invalid symbol format. Expected 'BASE-QUOTE', but received: '" + symbol + "'");
  }

  std::string base_currency = symbol.substr(0, delimiter_pos);
  std::string quote_currency = symbol.substr(delimiter_pos+1);

  auto const base_iter = currency_to_id.find(base_currency);
  auto const quote_iter = currency_to_id.find(quote_currency);

  if (base_iter == currency_to_id.end() || quote_iter == currency_to_id.end()) {
    std::cerr << "Error: One or both currencies in the pair '" << base_currency << "-" << quote_currency << "' are not tracked." << std::endl;
    return;
  }

  int base_id = base_iter->second;
  int quote_id = quote_iter->second;


  /** 
   * TODO: KEY OPTIMIZATION REQUIRED
   * 
   * Instead of injesting "last traded price", ingest L1 order book data (best bid and best ask)
   * Reverse weight is its own argument, not calculated off of inputted price.
   */
  double weight = -log(price);
  double reverse_weight = -log(1.0 / price);

  /* Update Forward Edge */
  uint64_t forward_key = create_edge_key(base_id, quote_id);
  if (edge_index_map.find(forward_key) == edge_index_map.end()) {
    adjacency_list[base_id].push_back({quote_id, weight});
    edge_index_map[forward_key] = adjacency_list[base_id].size() - 1;
  } else {
    adjacency_list[base_id][edge_index_map[forward_key]].weight = weight;
  }

  /* Update Reverse Edge */
  uint64_t reverse_key = create_edge_key(quote_id, base_id);
  if (edge_index_map.find(reverse_key) == edge_index_map.end()) {
    adjacency_list[quote_id].push_back({base_id, reverse_weight});
    edge_index_map[reverse_key] = adjacency_list[quote_id].size() - 1;
  } else {
    adjacency_list[quote_id][edge_index_map[reverse_key]].weight = reverse_weight;
  }

  /* Key SPFA Optimization */
  dirty_vertices.push_back(base_id);
  dirty_vertices.push_back(quote_id);

}

/**
 * @brief Finds a negative weight cycle in the graph, which represents an arbitrage opportunity.
 * 
 * This function implements the Shortest Path Faster Algorithm (SPFA). It iteratively
 * "relaxes" the edges of the graph, updating the shortest known distance from the source
 * to each vertex. If it detects that a vertex has been updated more times than there are
 * vertices in the graph, it signifies the presence of a negative weight cycle.
 * 
 * @return An `std::optional` containing a vector of currency names in the cycle if one is found,
 * or `std::nullopt` if no opportunity exists.
 */
std::optional<std::vector<std::string>> ArbitrageGraph::find_arbitrage_cycle() {
  
  while (!dirty_vertices.empty()) {
  
    int u = dirty_vertices.front();
    dirty_vertices.pop_front();

    for (const auto& edge : adjacency_list[u]) {

      int v = edge.destination_id;
      double weight = edge.weight;

      if (distance[u] != std::numeric_limits<double>::infinity() && distance[u] + weight < distance[v]) {
        distance[v] = distance[u] + weight;
        predecessor[v] = u;
        dirty_vertices.push_back(v);

        update_counts[v]++;
        if (update_counts[v] >= num_vertices) {
          return reconstruct_cycle(v);
        }
      }
    }
  }

  return std::nullopt;

}

/**
 * @brief Reconstructs the arbitrage cycle from the predecessor list.
 * 
 * Once a negative cycle is detected by `find_arbitrage_cycle`, this function is called
 * to trace back through the `predecessor` array to identify the exact path of the cycle.
 * 
 * @param start_node A node that is part of the detected negative cycle.
 * @return A vector of strings representing the currencies in the arbitrage cycle.
 */
std::vector<std::string> ArbitrageGraph::reconstruct_cycle(int start_node) const {
  std::vector<std::string> cycle;
  std::vector<int> path;

  int current = start_node;
  for (int i = 0; i < num_vertices; i++) {
    current = predecessor.at(current);
  }

  int cycle_start = current;
  do {
    path.insert(path.begin(), current);
    current = predecessor.at(current);
  } while (current != cycle_start);
  path.insert(path.begin(), cycle_start);

  for (int node_id : path) {
    cycle.push_back(id_to_currency.at(node_id));
  }

  return cycle;
}