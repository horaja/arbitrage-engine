#include "arbitragegraph.h"
#include <set>
#include <vector>

ArbitrageGraph::create_edge_key(source_id, dest_id) {
  return ((uint64) source_id) << 32 | dest_id;
}

ArbitrageGraph::ArbitrageGraph(const std::vector<std::string>& symbols) {
  /**
   * goal: build static skeleton of graph from a list of symbols
   * input: list of all currency pairs
   * for each currency name, assign unique integer ID & store mappings
   * For each trading pair, create two "empty" directed edge structs
   * add to adjacency list structure
   *  - while adding each edge, create unique key (source << 31 | dest) and add to edge index map
   * init all ds needed for SPFA
  */
  std::set<string> unique_symbols(symbols);
  this->num_vertices = unique_symbols.size();

  int current_id = 0;
  for (const auto& symbol_name : unique_symbols) {
    this->currency_to_id[symbol_name] = current_id;
    this->id_to_currency.push_back(symbol_name);
    current_id++;
  }

  this->distance.resize(num_vertices, std::numeric_limits<double>::infinity());
  this->predecessor.resize(num_vertices, -1);
  this->update_counts.resize(num_vertices, 0);
  this->distance[0] = 0.0;

  for (int i = 0; i < num_vertices - 1; i++) {
    for (int j = 0; j < num_vertices - 1; j++) {
      if (i == j) continue;

      ArbitrageGraph::Edge new_edge = {destination_id}
    }
  }

  
}

void ArbitrageGraph::update_price(const std::string& symbol, double price) {
  /**
   * goal: efficiently update graph w/ new price tick
   * receive symbol string and new price
   * parse to get two currency names
   * use currency-to-id map to get ids
   * calculate new weights
   * use edge index map and update weights
  */

}