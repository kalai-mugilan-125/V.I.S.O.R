import socket
import json
import numpy as np
import sys
import time

try:
    import pygame
    from pygame.locals import *
    from OpenGL.GL import *
    from OpenGL.GLU import *
except ImportError:
    print("Pygame and PyOpenGL are required to run this script.")
    print("Please install them using: pip install pygame PyOpenGL")
    sys.exit(1)

# UDP server configuration matching main.py UDPTransmitter
UDP_IP = "127.0.0.1"
UDP_PORT = 5005

# Cube definitions
vertices = (
    (1, -1, -1),
    (1, 1, -1),
    (-1, 1, -1),
    (-1, -1, -1),
    (1, -1, 1),
    (1, 1, 1),
    (-1, -1, 1),
    (-1, 1, 1)
)

edges = (
    (0,1), (0,3), (0,4),
    (2,1), (2,3), (2,7),
    (6,3), (6,4), (6,7),
    (5,1), (5,4), (5,7)
)

colors = (
    (1, 0, 0), (0, 1, 0), (0, 0, 1), (1, 1, 0),
    (1, 0, 1), (0, 1, 1), (1, 1, 1), (0.5, 0.5, 0.5)
)

surfaces = (
    (0,1,2,3),
    (3,2,7,6),
    (6,7,5,4),
    (4,5,1,0),
    (1,5,7,2),
    (4,0,3,6)
)

def draw_cube():
    glBegin(GL_QUADS)
    for i, surface in enumerate(surfaces):
        glColor3fv(colors[i])
        for vertex in surface:
            glVertex3fv(vertices[vertex])
    glEnd()

    glBegin(GL_LINES)
    glColor3fv((0, 0, 0))
    for edge in edges:
        for vertex in edge:
            glVertex3fv(vertices[vertex])
    glEnd()

def draw_text(x, y, text, font):
    text_surface = font.render(text, True, (255, 255, 255, 255), (0, 0, 0, 0))
    text_data = pygame.image.tostring(text_surface, "RGBA", True)
    width, height = text_surface.get_size()
    
    glMatrixMode(GL_PROJECTION)
    glPushMatrix()
    glLoadIdentity()
    gluOrtho2D(0, 800, 0, 600)
    
    glMatrixMode(GL_MODELVIEW)
    glPushMatrix()
    glLoadIdentity()
    
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    
    glRasterPos2i(x, y)
    glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, text_data)
    
    glDisable(GL_BLEND)
    
    glPopMatrix()
    glMatrixMode(GL_PROJECTION)
    glPopMatrix()
    glMatrixMode(GL_MODELVIEW)

class Visor3DTestEnv:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((UDP_IP, UDP_PORT))
        self.sock.setblocking(False)
        
        # 3D Environment actual rendering variables
        self.zoom = -5.0
        self.pan_x = 0.0
        self.pan_y = 0.0
        self.rot_x = 0.0
        self.rot_y = 0.0
        
        # Target variables for smooth animation (lerp)
        self.target_zoom = -5.0
        self.target_pan_x = 0.0
        self.target_pan_y = 0.0
        self.target_rot_x = 0.0
        self.target_rot_y = 0.0
        
        # State tracking for gestures
        self.prev_gesture = "IDLE"
        self.prev_hand_pos = None
        self.prev_peace_pos = None
        
        self.last_zoom_time = 0.0
        self.last_active_gesture_type = ""
        self.cooldown_until = 0.0
        
        self.current_action = ""
        self.display_text = ""
        
        self.running = True

    def process_packet(self, packet):
        """
        Maps the gestures from GESTURE_MAPPINGS.md to 3D cube actions.
        """
        if not packet.get("active", False):
            self.prev_gesture = "IDLE"
            self.prev_hand_pos = None
            return

        hands = packet.get("hands", [])
        if not hands:
            # Fallback for old packet format
            hands = [{
                "gesture": packet.get("gesture", "IDLE"),
                "landmarks": packet.get("landmarks", [])
            }]
            
        # For movement tracking we use the first hand's position as primary
        primary_hand = hands[0]
        gesture = primary_hand.get("gesture", "IDLE")
        landmarks = primary_hand.get("landmarks", [])
        
        # Check if we have two hands for composite gestures
        has_two_hands = len(hands) >= 2
        second_gesture = hands[1].get("gesture", "IDLE") if has_two_hands else "NONE"
        
        self.current_action = ""

        # Compute central position of the primary hand based on landmarks for movement tracking
        if landmarks:
            hand_pos = np.mean(landmarks, axis=0)
        else:
            hand_pos = None
            
        # Find PEACE_SIGN hand position for rotation tracking
        peace_pos = None
        if has_two_hands:
            if gesture == "PEACE_SIGN":
                peace_pos = hand_pos
            elif second_gesture == "PEACE_SIGN":
                peace_landmarks = hands[1].get("landmarks", [])
                if peace_landmarks:
                    peace_pos = np.mean(peace_landmarks, axis=0)

        current_time = time.time()

        if current_time > self.cooldown_until:
            self.last_active_gesture_type = ""

        if has_two_hands:
            # ONLY allow rotation when two hands are present
            is_composite_rotation = (gesture == "OPEN_PALM" and second_gesture == "PEACE_SIGN") or \
                                    (gesture == "PEACE_SIGN" and second_gesture == "OPEN_PALM")
            if is_composite_rotation:
                if self.last_active_gesture_type not in ["ROTATE", ""] and current_time < self.cooldown_until:
                    pass # Cooldown active, ignore
                else:
                    self.last_active_gesture_type = "ROTATE"
                    self.cooldown_until = current_time + 2.0
                    if self.prev_peace_pos is not None and peace_pos is not None:
                        # Rotate ONLY based on the movement of the PEACE_SIGN hand
                        dx = peace_pos[0] - self.prev_peace_pos[0]
                        dy = peace_pos[1] - self.prev_peace_pos[1]
                        
                        # Anti-jump guard
                        if abs(dx) < 0.3 and abs(dy) < 0.3:
                            if abs(dx) > 0.01:
                                self.target_rot_y += dx * 200.0
                                self.current_action = "XYZ Rotation (Y-axis)"
                            if abs(dy) > 0.01:
                                self.target_rot_x += dy * 200.0
                                self.current_action = "XYZ Rotation (X-axis)"
        else:
            # Single hand logic
            is_zoom_attempt = (self.prev_gesture == "POINTING" and gesture == "PINCH") or \
                              (self.prev_gesture == "PINCH" and gesture == "POINTING")
                              
            if is_zoom_attempt:
                if self.last_active_gesture_type not in ["ZOOM", ""] and current_time < self.cooldown_until:
                    pass # Cooldown active
                else:
                    if current_time - self.last_zoom_time > 1.0:
                        self.last_active_gesture_type = "ZOOM"
                        self.cooldown_until = current_time + 2.0
                        if self.prev_gesture == "POINTING" and gesture == "PINCH":
                            self.target_zoom -= 1.0
                            self.current_action = "Zoom Out"
                            self.last_zoom_time = current_time
                        elif self.prev_gesture == "PINCH" and gesture == "POINTING":
                            self.target_zoom += 1.0
                            self.current_action = "Zoom In"
                            self.last_zoom_time = current_time

            elif gesture == "OPEN_PALM":
                if self.last_active_gesture_type not in ["PAN", ""] and current_time < self.cooldown_until:
                    pass # Cooldown active
                else:
                    self.last_active_gesture_type = "PAN"
                    self.cooldown_until = current_time + 2.0
                    if self.prev_hand_pos is not None and hand_pos is not None:
                        dx = hand_pos[0] - self.prev_hand_pos[0]
                        dy = hand_pos[1] - self.prev_hand_pos[1]
                        
                        # Anti-jump guard
                        if abs(dx) < 0.3 and abs(dy) < 0.3:
                            if abs(dx) > 0.01:
                                self.target_pan_x -= dx * 5.0
                                self.current_action = "Move Environment Left/Right"
                            if abs(dy) > 0.01:
                                self.target_pan_y -= dy * 5.0
                                self.current_action = "Move Environment Up/Down"

            elif gesture == "FIST":
                # FIST allows user to move hand around without affecting the cube.
                # It also resets the mode cooldown so they can immediately transition to another gesture.
                self.last_active_gesture_type = ""
                self.cooldown_until = 0.0
                self.current_action = "Idle (Fist)"

        if self.current_action:
            print(f"[Action] {self.current_action}")
            self.display_text = self.current_action

        self.prev_gesture = gesture
        self.prev_hand_pos = hand_pos
        self.prev_peace_pos = peace_pos

    def run(self):
        pygame.init()
        pygame.font.init()
        font = pygame.font.SysFont('arial', 32, bold=True)
        display = (800, 600)
        pygame.display.set_mode(display, DOUBLEBUF | OPENGL)
        pygame.display.set_caption("V.I.S.O.R - 3D Gesture Mapping Test")

        # Setup perspective
        gluPerspective(45, (display[0] / display[1]), 0.1, 50.0)

        clock = pygame.time.Clock()
        print("Listening for V.I.S.O.R AI UDP Packets on port 5005...")

        while self.running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        self.running = False

            # Process incoming UDP packets
            try:
                while True:
                    data, addr = self.sock.recvfrom(8192)
                    packet = json.loads(data.decode('utf-8'))
                    self.process_packet(packet)
            except BlockingIOError:
                pass # No more packets to read
            except json.JSONDecodeError:
                print("Failed to decode JSON packet.")
            except OSError as e:
                # Handle socket error if not just blocking IO
                if getattr(e, 'errno', None) != 10035: # 10035 is WSAEWOULDBLOCK on Windows
                    pass

            # Apply smooth animations (lerp)
            self.pan_x += (self.target_pan_x - self.pan_x) * 0.1
            self.pan_y += (self.target_pan_y - self.pan_y) * 0.1
            self.zoom += (self.target_zoom - self.zoom) * 0.1
            self.rot_x += (self.target_rot_x - self.rot_x) * 0.1
            self.rot_y += (self.target_rot_y - self.rot_y) * 0.1

            # Rendering
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
            glEnable(GL_DEPTH_TEST)

            # Apply transformations
            glPushMatrix()
            # Pan and Zoom
            glTranslatef(self.pan_x, self.pan_y, self.zoom)
            # Rotation
            glRotatef(self.rot_x, 1, 0, 0)
            glRotatef(self.rot_y, 0, 1, 0)
            
            draw_cube()
            
            glPopMatrix()

            if self.display_text:
                draw_text(20, 550, f"Action: {self.display_text}", font)

            pygame.display.flip()
            clock.tick(60)

        pygame.quit()

if __name__ == "__main__":
    app = Visor3DTestEnv()
    app.run()
