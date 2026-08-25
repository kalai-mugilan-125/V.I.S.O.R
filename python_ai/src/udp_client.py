import socket
import json

class UDPTransmitter:
    def __init__(self, target_ip="127.0.0.1", target_port=5005):
        self.target_ip = target_ip
        self.target_port = target_port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Optional: Set buffer size for high-speed transmission
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)

    def send_packet(self, data_dict):
        """Serializes the dictionary to JSON and sends it via UDP."""
        try:
            # Compact JSON serialization
            message = json.dumps(data_dict, separators=(',', ':')).encode('utf-8')
            self.sock.sendto(message, (self.target_ip, self.target_port))
        except Exception as e:
            print(f"[UDP Error] Failed to send packet: {e}")