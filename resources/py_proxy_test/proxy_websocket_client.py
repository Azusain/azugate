import websocket
import time

def main():
    ws = websocket.create_connection("ws://127.0.0.1:8080")
    try:
        ws.send("This is WebSocket Client")
        while True:
            response = ws.recv()
            print(f"Get Message: {response}")
            time.sleep(1)  
    except KeyboardInterrupt:
        print("Exiting...")
    finally:
        ws.close()

if __name__ == "__main__":
    main()