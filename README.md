# Latency-Aware, Risk-Aware Triangular Arbitrage Engine

This project is a high-performance triangular arbitrage detection engine built in C++ for cryptocurrency markets. It identifies risk-free profit opportunities and uses a sophisticated Gaussian Process model, trained in Python, to predict execution latency. This ensures the system only attempts trades with a high probability of success, making it a robust and practical automated trading system.

## Key Features

* **High-Performance Core:** The arbitrage detection logic is written in C++ for maximum performance.
* **Multi-threaded Architecture:** Decouples data ingestion from the core trading logic using concurrent queues.
* **Graph-Based Detection:** Models the market as a graph and uses the Bellman-Ford algorithm to find arbitrage opportunities (negative weight cycles).
* **Probabilistic Latency Modeling:** Employs a Gaussian Process (GP) regressor to provide a probabilistic forecast of round-trip execution latency, preventing trades with high timing risk.
* **Risk-Aware Logic:** The core C++ engine is parameterized by the ML model's output, only executing trades that are predicted to be profitable *after* accounting for latency risk.

## Technology Stack

* **Core Engine:** C++
* **ML Model & Data Logging:** Python
* **Key Libraries:**
    * C++: Boost, websocketpp, Eigen
    * Python: pandas, websockets, scikit-learn, GPy
* **Build System:** CMake

## Project Status

* **Phase 1: Data Acquisition & Environment Setup:** Complete
* **Phase 2: Core C++ Arbitrage Engine:** In Progress
* **Phase 3: Data Logging for ML Model:** Not Started
* **Phase 4: Python ML Latency Model:** Not Started
* **Phase 5: Integration and Final Testing:** Not Started

## Setup and Usage

### Prerequisites
* A C++ compiler (GCC, Clang)
* CMake
* Python 3.8+
* Boost Libraries

### Installation & Setup

1.  **Clone the repository:**
    ```bash
    git clone [Your Repository URL]
    cd [repository-name]
    ```
2.  **Setup Python Environment:**
    ```bash
    cd python_utils
    python3 -m venv venv
    source venv/bin/activate
    pip install -r requirements.txt
    ```
3.  **Build the C++ Engine:**
    ```bash
    cd ../cpp_engine
    mkdir build && cd build
    cmake ..
    make
    ```

### Running the Project

1.  **Start the Data Logger:**
    ```bash
    # From the python_utils directory
    python data_logger.py
    ```
2.  **Run the C++ Engine:**
    ```bash
    # From the cpp_engine/build directory
    ./arbitrage_engine
    ```