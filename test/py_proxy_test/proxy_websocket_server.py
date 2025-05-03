import time
from websocket_server import WebsocketServer

def on_message(client, server, message):
    print(f"Received message: {message}")
    while True:
      server.send_message(client, f"This is WebSocket server")
      time.sleep(1)

def main():
    server = WebsocketServer(host="0.0.0.0", port=8081)
    server.set_fn_message_received(on_message)
    print("WebSocket Server started at ws://0.0.0.0:8081")
    server.run_forever()

if __name__ == "__main__":
    main()