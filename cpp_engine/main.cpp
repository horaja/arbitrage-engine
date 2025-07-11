#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>

#include "blockingconcurrentqueue.h"
#include "arbitragegraph.h"

struct PriceUpdate {
  std::string symbol;
  double price;
  /** TODO: Add timestamp, etc. */
};

void io_thread_fn(moodycamel::BlockingConcurrentQueue<PriceUpdate> queue) {
  std::cout << "IO Thread: Starting Up..." << std::endl;

  std::ifstream inputFile("trade_data_coinbase.csv");
  if (!inputFile.is_open()) {
    std::cerr << "Error: Could not open file." << std::endl;
    return;
  }

  std::string line;
  char delimiter = ',';
  std::getline(inputFile, line);

  while (std::getline(inputFile, line)) {
    std::stringstream ss(line);

    std::string timestamp_str, symbol_str, price_str, quantity_str;

    std::getline(ss, timestamp_str, delimiter);
    std::getline(ss, symbol_str, delimiter);
    std::getline(ss, price_str, delimiter);
    std::getline(ss, quantity_str, delimiter);

    PriceUpdate new_update;
    new_update.symbol = symbol_str;
    new_update.price = std::stod(price_str);

    queue.enqueue(new_update);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  std::cout << "IO Thread: Finished reading file. Sending poison pill." << std::endl;

  PriceUpdate poison_pill;
  poison_pill.symbol = "STOP";
  queue.enqueue(poison_pill);
}

void logic_thread_fn(moodycamel::BlockingConcurrentQueue<PriceUpdate> queue) {
  std::cout << "Logic Thread: Starting Up and Waiting for Data..." << std::endl;

  while(true) {
    PriceUpdate received_update;

    queue.wait_dequeue(received_update);

    if (received_update.symbol == "STOP") {
      std::cout << "Logic Thread: Poison pill received. Shutting down." << std::endl;
      break;
    }

    std::cout << "Logic Thread: Dequeued update for " << received_update.symbol << " at price " << received_update.price << std::endl;
    
    /** TODO: add core logic */
  }
}

int main() {
  std::cout << "Creating and Launching Threads..." << std::endl;

  moodycamel::BlockingConcurrentQueue<PriceUpdate> shared_queue;

  std::thread io_thread(io_thread_fn, shared_queue);
  std::thread logic_thread(logic_thread_fn, shared_queue);

  std::cout << "Main: Threads launched." << std::endl;

  io_thread.join();
  logic_thread.join();

  return 0;
}
