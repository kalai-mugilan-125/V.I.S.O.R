import cv2
import mediapipe as mp

class MediaPipeTracker:
    def __init__(self, static_image_mode=False, max_num_hands=2, min_detection_confidence=0.7):
        self.mp_hands = mp.solutions.hands
        self.hands = self.mp_hands.Hands(
            static_image_mode=static_image_mode,
            max_num_hands=max_num_hands,
            min_detection_confidence=min_detection_confidence,
            min_tracking_confidence=0.5
        )
        self.mp_draw = mp.solutions.drawing_utils

    def extract_landmarks(self, frame):
        """Processes an RGB frame and returns a list of 21-point landmark matrices for each detected hand."""
        # MediaPipe requires RGB format
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = self.hands.process(rgb_frame)

        all_hands_landmarks = []
        if results.multi_hand_landmarks:
            for hand_landmarks in results.multi_hand_landmarks:
                landmark_matrix = []
                for lm in hand_landmarks.landmark:
                    # Append raw x, y, z normalized coordinates
                    landmark_matrix.append([lm.x, lm.y, lm.z])
                all_hands_landmarks.append(landmark_matrix)
                
        return all_hands_landmarks, results
    
    def draw_debug(self, frame, results):
        """Draws the wireframe on the frame for debugging."""
        if results.multi_hand_landmarks:
            for hand_landmarks in results.multi_hand_landmarks:
                self.mp_draw.draw_landmarks(frame, hand_landmarks, self.mp_hands.HAND_CONNECTIONS)
        return frame