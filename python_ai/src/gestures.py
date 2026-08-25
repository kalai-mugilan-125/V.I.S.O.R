import numpy as np

class GestureAnalyzer:
    def __init__(self):
        self.is_pinching = False

    def calculate_pinch_zoom(self, landmarks):
        """
        Calculates the Euclidean distance between thumb tip (4) and index tip (8).
        """
        if not landmarks or len(landmarks) < 21:
            return 0.0

        thumb_tip = np.array(landmarks[4])
        index_tip = np.array(landmarks[8])
        
        # Calculate 3D Euclidean distance
        distance = float(np.linalg.norm(index_tip - thumb_tip))
        return distance

    def calculate_orbit_rotation(self, landmarks):
        """
        Estimates palm variance (roll, pitch, yaw) based on wrist (0) and MCP joints.
        Returns a generic Vector3 [x, y, z] representation.
        """
        if not landmarks or len(landmarks) < 21:
            return [0.0, 0.0, 0.0]

        wrist = np.array(landmarks[0])
        index_mcp = np.array(landmarks[5])
        pinky_mcp = np.array(landmarks[17])

        # Basic vector math to determine palm orientation plane
        vector_y = index_mcp - wrist
        vector_x = pinky_mcp - index_mcp
        
        # Cross product gives the normal vector of the palm (Z-axis / depth orientation)
        normal_vector = np.cross(vector_x, vector_y)
        
        # Normalize the vectors to get stable rotation values
        pitch = float(np.arctan2(vector_y[1], vector_y[2])) if vector_y[2] != 0 else 0.0
        yaw = float(np.arctan2(normal_vector[0], normal_vector[2])) if normal_vector[2] != 0 else 0.0
        roll = float(np.arctan2(vector_x[1], vector_x[0])) if vector_x[0] != 0 else 0.0

        return [pitch, yaw, roll]

    def map_gesture(self, landmarks, pinch_dist):
        """
        Maps raw landmarks and calculated metrics to a discrete gesture command
        with sticky hysteresis on pinch detection.
        """
        if not landmarks or len(landmarks) < 21:
            self.is_pinching = False
            return "NONE"

        # Hysteresis for pinch:
        # Enter pinch at < 0.09 (generous entry), stay in pinch until > 0.20
        # This wide band prevents finger micro-movements during drag from dropping the lock
        if not self.is_pinching:
            if pinch_dist < 0.03:
                self.is_pinching = True
        else:
            if pinch_dist > 0.10:
                self.is_pinching = False

        if self.is_pinching:
            return "PINCH"

        # Count how many of the 4 main fingers are up (excluding thumb for simplicity)
        tips = [8, 12, 16, 20] # Index, Middle, Ring, Pinky tips
        pips = [6, 10, 14, 18] # Corresponding PIP joints
        
        fingers_up = 0
        for tip, pip in zip(tips, pips):
            # In image coordinates, 0 is top. So tip.y < pip.y means finger is pointing up
            if landmarks[tip][1] < landmarks[pip][1]:
                fingers_up += 1

        # Map the finger count to specific states
        if fingers_up == 0:
            return "FIST"
        elif fingers_up == 1:
            return "POINTING"
        elif fingers_up == 2:
            return "PEACE_SIGN"
        elif fingers_up >= 3:
            return "OPEN_PALM"

        return "IDLE"