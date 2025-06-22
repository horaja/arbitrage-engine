#include <iostream>

// 1. Test that Boost is linked correctly by including a header 
//    and using a macro it provides.
#include <boost/version.hpp>

// 2. Test that the compiler can find the websocketpp headers.
//    We don't need to use the library yet, just successfully include it.
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/client.hpp>

// Define a placeholder type for the test include
typedef websocketpp::client<websocketpp::config::asio> client;

int main() {
  // Print a startup message
  std::cout << "Arbitrage Engine starting up..." << std::endl;

  // --- Verification ---

  // Verify Boost Linking
  // The BOOST_LIB_VERSION macro is a string like "1_85" provided by Boost's headers
  std::cout << "Test 1: Boost library linking..." << std::endl;
  std::cout << "  - Boost version: " << BOOST_LIB_VERSION << std::endl;
  std::cout << "  - Test PASSED." << std::endl;

  // Verify websocketpp Header Inclusion
  std::cout << "Test 2: WebSocket++ header inclusion..." << std::endl;
  std::cout << "  - Headers included successfully." << std::endl;
  std::cout << "  - Test PASSED." << std::endl;

  std::cout << "\nCMake configuration appears to be working correctly!" << std::endl;

  return 0;
}
