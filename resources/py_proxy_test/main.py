from http.server import BaseHTTPRequestHandler, HTTPServer

class MyHandler(BaseHTTPRequestHandler):
    def _log_request(self):
        print("\n====== Incoming Request ======")
        print(f"{self.command} {self.path} {self.request_version}")
        print("---- Headers ----")
        for name, value in self.headers.items():
            print(f"{name}: {value}")

        content_length = self.headers.get('Content-Length')
        if content_length:
            length = int(content_length)
            body = self.rfile.read(length).decode('utf-8', errors='replace')
            print("---- Body ----")
            print(body)
        else:
            print("No Body")
        print("=============================\n")

    def do_GET(self):
        self._log_request()
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b"GET received")

    def do_POST(self):
        self._log_request()
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b"POST received")

def run(server_class=HTTPServer, handler_class=MyHandler, port=8081):
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    print(f"Starting HTTP server on port {port}...")
    httpd.serve_forever()

if __name__ == "__main__":
    run()