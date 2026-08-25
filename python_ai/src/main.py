import cv2
import time
import sys
from tracker import MediaPipeTracker
from gestures import GestureAnalyzer
from udp_client import UDPTransmitter

class VisionController:
    def __init__(self):
        self.tracker = MediaPipeTracker(min_detection_confidence=0.7)
        self.analyzer = GestureAnalyzer()
        self.transmitter = UDPTransmitter(target_ip="127.0.0.1", target_port=5005)
        self.cap = cv2.VideoCapture(0, cv2.CAP_DSHOW) # Use DirectShow on Windows for fast capture
        
        # If DirectShow fails or camera 0 fails, fallback
        if not self.cap.isOpened():
            self.cap = cv2.VideoCapture(0)

        # Force optimal parameters
        self.cap.set(cv2.CAP_PROP_FPS, 60)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    def run(self):
        print("[V.I.S.O.R AI] Headless Vision Microservice Active (Port 5005)...", flush=True)
        
        while self.cap.isOpened():
            success, frame = self.cap.read()
            if not success:
                time.sleep(0.01)
                continue

            # 1. Extract raw landmarks
            all_hands_landmarks, _ = self.tracker.extract_landmarks(frame)

            # 2. Analyze spatial gestures if hands detected
            if all_hands_landmarks:
                hands_data = []
                for landmarks in all_hands_landmarks:
                    pinch_dist = self.analyzer.calculate_pinch_zoom(landmarks)
                    palm_rot = self.analyzer.calculate_orbit_rotation(landmarks)
                    mapped_gesture = self.analyzer.map_gesture(landmarks, pinch_dist)
                    
                    hands_data.append({
                        "landmarks": landmarks,
                        "pinch_distance": pinch_dist,
                        "palm_rotation": palm_rot,
                        "gesture": mapped_gesture
                    })

                # 3. Package and transmit data asynchronously via UDP
                packet = {
                    "active": True,
                    "landmarks": hands_data[0]["landmarks"],
                    "pinch_distance": hands_data[0]["pinch_distance"],
                    "palm_rotation": hands_data[0]["palm_rotation"],
                    "gesture": hands_data[0]["gesture"],
                    "hands": hands_data
                }
                self.transmitter.send_packet(packet)
            else:
                # Send inactive state so renderer knows no hands are present
                self.transmitter.send_packet({"active": False, "hands": []})

            # Small yield to prevent 100% CPU thread starvation
            time.sleep(0.001)

        self.cap.release()

if __name__ == "__main__":
    controller = VisionController()
    controller.run()