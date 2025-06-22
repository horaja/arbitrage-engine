'''
Tasks:
1. Define Config Settings:
  - Output filename
  - List of market PRODUCT_IDS to track
2. Prepare the output file:
  a. Check if the output CSV file already exists.
  b. If it does not exist, create it and write the column headers.
    - timestamp, symbol, price, quantity
3. Main Data Logging Function:
  a. Construct the specific connection address (URI) for the Binance WebSocket stream based on the list of PRODUCT_IDS.
    - base endpoint is: wss://stream.binance.com:9443 or wss://stream.binance.com:443.
  b. Establish persistent connection to the WebSocket server
  c. Upon successful connection, start an infinite loop to listen for messages.
  d. For each message received:
    i.   Attempt to parse the message as structured JSON data.
    ii.  Extract the essential trade details: timestamp, symbol, price, and quantity.
    iii. Append this new trade data as a new line in the output CSV file.
    iv.  Print the formatted trade data to the console for live monitoring.
    v.   If any error occurs during this process, print the error and pause briefly before continuing the loop.
4. When the script is run directly:
  a. Start the main data logging function.
  b. Keep the program running until the user manually stops it (e.g., with Ctrl+C).
  c. When stopped, print a confirmation message.
'''

import asyncio
import websockets
import json
import pandas as pd
from datetime import datetime

OUTPUT_FILE = "trade_data_coinbase.csv"
PRODUCT_IDS = ["BTC-USD", "ETH-USD", "ETH-BTC"]

try:
  df = pd.read_csv(OUTPUT_FILE)
except FileNotFoundError:
  df = pd.DataFrame(columns=["timestamp", "symbol", "price", "quantity"])
  df.to_csv(OUTPUT_FILE, index=False)

async def data_logger():
  uri = f"wss://ws-feed.exchange.coinbase.com"

  subscribe_message = {
    "type": "subscribe",
    "product_ids": PRODUCT_IDS,
    "channels": ["matches"]
  }
  
  async with websockets.connect(uri) as websocket:
    await websocket.send(json.dumps(subscribe_message))
    print(f"Connected to Coinbase WebSocket and subscribed to 'matches' for: {PRODUCT_IDS}")
    
    while True:
      try:
        message = await websocket.recv()
        data = json.loads(message)

        if data.get("type") == "match":
          trade_time = datetime.fromisoformat(data['time'].replace('Z', '+00:00'))
          symbol = data['product_id']
          price = float(data['price'])
          quantity = float(data['size'])

          with open(OUTPUT_FILE, 'a') as f:
            f.write(f"{trade_time}, {symbol}, {price}, {quantity}\n")

          # for debugging purposes:
          print(f"{trade_time} | {symbol:<10} | Price: {price:<15.4f} | Quantity: {quantity:<15.8f}")

      except Exception as e:
        print(f"An error occured: {e}")
        # enhance error handling
        await asyncio.sleep(5)

if __name__ == "__main__":
  try:
    asyncio.run(data_logger())
  except KeyboardInterrupt:
    print("Data logging stopped.")

