#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstdint>

class ArbitrageGraph {
  public:
    ArbitrageGraph(const std::vector<std::string>& symbols);

    void update_price(const std::string& symbol, double price);

    std::optional<std::vector<std::string>> find_arbitrage_cycle();

  private:
    struct Edge {
      int destination_id;
      double weight;
    };

    std::unordered_map<int, std::vector<Edge>> adjacency_list;

    std::unordered_map<std::string, int> currency_to_id;

    std::vector<std::string> id_to_currency;

    std::unordered_map<uint64_t, size_t> edge_index_map;

    int num_vertices = 0;
    std::vector<double> distance;
    std::vector<int> predecessor;
    std::vector<int> update_counts;
    std::vector<int> dirty_vertices;

    uint64_t create_edge_key(int source_id, int destination_id) const;

    std::vector<std::string> reconstruct_cycle(int start_node) const;
};