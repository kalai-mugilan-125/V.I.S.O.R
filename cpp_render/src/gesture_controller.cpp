#include "gesture_controller.h"
#include "camera.h"
#include "udp_server.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>

// ---------------------------------------------------------------------------
//  MovingAverage3D Implementation
// ---------------------------------------------------------------------------

MovingAverage3D::MovingAverage3D(int cap)
    : capacity(cap), sum(0.0f), index(0) {}

glm::vec3 MovingAverage3D::add(const glm::vec3& val) {
    if ((int)window.size() < capacity) {
        window.push_back(val);
        sum += val;
        return sum / static_cast<float>(window.size());
    } else {
        sum -= window[index];
        window[index] = val;
        sum += val;
        index = (index + 1) % capacity;
        return sum / static_cast<float>(capacity);
    }
}

void MovingAverage3D::reset() {
    window.clear();
    sum = glm::vec3(0.0f);
    index = 0;
}

// ---------------------------------------------------------------------------
//  GestureController Implementation
// ---------------------------------------------------------------------------

GestureController::GestureController()
    : m_filterHand1(15),
      m_filterHand2(15),
      m_filterPeace(10),
      m_filterPinch(10),
      // Hysteresis
      m_lockedGesture("NONE"),
      m_pendingGesture("NONE"),
      m_pendingCount(0),
      // Tracking
      m_prevPrimaryGesture("IDLE"),
      m_prevPrimaryPos(0.0f),
      m_hasPrevPrimaryPos(false),
      m_prevPeacePos(0.0f),
      m_hasPrevPeacePos(false),
      // Smooth Pinch & Drag Zoom
      m_prevPinchPos(0.0f),
      m_hasPrevPinchPos(false),
      m_prevPinchDistance(0.0f),
      m_hasPrevPinchDist(false),
      // Dual-Hand Zoom
      m_prevDualHandDist(0.0f),
      m_hasPrevDualHandDist(false),
      // Cooldowns
      m_panCooldownUntil(0.0),
      // Telemetry
      m_currentAction("Idle"),
      m_primaryGesture("NONE"),
      m_secondaryGesture("NONE"),
      m_detectedHandCount(0) {
}

void GestureController::updateLockedGesture(const std::string& rawGesture) {
    if (rawGesture == m_lockedGesture) {
        m_pendingGesture = rawGesture;
        m_pendingCount = 0;
        return;
    }

    if (rawGesture == m_pendingGesture) {
        m_pendingCount++;
        if (m_pendingCount >= kHysteresisFrames) {
            m_lockedGesture = rawGesture;
            m_pendingCount = 0;

            if (m_lockedGesture != "PEACE_SIGN") {
                m_hasPrevPeacePos = false;
                m_filterPeace.reset();
            }
            if (m_lockedGesture != "OPEN_PALM") {
                m_hasPrevPrimaryPos = false;
            }
            if (m_lockedGesture != "PINCH") {
                m_hasPrevPinchPos = false;
                m_hasPrevPinchDist = false;
                m_filterPinch.reset();
            }
        }
    } else {
        m_pendingGesture = rawGesture;
        m_pendingCount = 1;
    }
}

void GestureController::processGestures(const DataPacket& packet, Camera& camera) {
    if (!packet.active || packet.hands.empty()) {
        m_currentAction     = "Idle (No Hands)";
        m_primaryGesture    = "NONE";
        m_secondaryGesture  = "NONE";
        m_detectedHandCount = 0;
        m_hasPrevPrimaryPos = false;
        m_hasPrevPeacePos   = false;
        m_hasPrevPinchPos   = false;
        m_hasPrevPinchDist  = false;
        m_hasPrevDualHandDist = false;
        m_prevPrimaryGesture = "IDLE";
        m_lockedGesture     = "NONE";
        m_pendingGesture    = "NONE";
        m_pendingCount      = 0;
        m_filterHand1.reset();
        m_filterHand2.reset();
        m_filterPeace.reset();
        m_filterPinch.reset();
        return;
    }

    m_detectedHandCount = (int)packet.hands.size();
    m_currentAction = "Idle";

    // --- Primary & Secondary hands ---
    const auto& h1 = packet.hands[0];
    m_primaryGesture = h1.gesture;
    glm::vec3 pos1 = m_filterHand1.add(h1.position);

    glm::vec3 pos2(0.0f);
    if (packet.hands.size() >= 2) {
        const auto& h2 = packet.hands[1];
        m_secondaryGesture = h2.gesture;
        pos2 = m_filterHand2.add(h2.position);
    } else {
        m_secondaryGesture = "NONE";
        m_hasPrevDualHandDist = false;
    }

    // === Hysteresis lock on primary gesture ===
    updateLockedGesture(h1.gesture);
    const std::string& gesture = m_lockedGesture;

    // =========================================================================
    //  1. DUAL-HAND DISTANCE ZOOM (When 2 hands are present and not orbiting)
    //  Spreading hands apart = Zoom In, bringing hands together = Zoom Out
    // =========================================================================
    if (packet.hands.size() >= 2 && gesture != "PEACE_SIGN" && m_secondaryGesture != "PEACE_SIGN") {
        float currentDualDist = glm::distance(pos1, pos2);

        if (m_hasPrevDualHandDist) {
            float deltaDist = currentDualDist - m_prevDualHandDist;

            if (std::abs(deltaDist) > 0.003f && std::abs(deltaDist) < 0.25f) {
                // Spreading hands (delta > 0) -> Zoom In (camera.zoom negative)
                // Pinching hands together (delta < 0) -> Zoom Out (camera.zoom positive)
                camera.zoom(-deltaDist * 40.0f);
                m_currentAction = (deltaDist > 0) ? "Dual-Hand: Zoom In" : "Dual-Hand: Zoom Out";
                m_prevDualHandDist = currentDualDist;
                return;
            }
        }
        m_prevDualHandDist = currentDualDist;
        m_hasPrevDualHandDist = true;
    } else {
        m_hasPrevDualHandDist = false;
    }

    // =========================================================================
    //  2. PEACE_SIGN → Orbital Rotation
    // =========================================================================
    if (gesture == "PEACE_SIGN") {
        glm::vec3 peacePos = m_filterPeace.add(h1.position);

        if (m_hasPrevPeacePos) {
            float dx = peacePos.x - m_prevPeacePos.x;
            float dy = peacePos.y - m_prevPeacePos.y;

            if (std::abs(dx) < 0.5f && std::abs(dy) < 0.5f) {
                if (std::abs(dx) > 0.0005f || std::abs(dy) > 0.0005f) {
                    camera.orbit(-dx * 100.0f, dy * 100.0f);
                    m_currentAction = "Peace Sign: Orbit";
                }
            }
        } else {
            m_currentAction = "Peace Sign: Orbit Engaged";
        }

        m_prevPeacePos    = peacePos;
        m_hasPrevPeacePos = true;
        m_hasPrevPrimaryPos = false;
        m_hasPrevPinchPos   = false;
        m_prevPrimaryGesture = "ORBITING";
        return;
    }

    // =========================================================================
    //  3. POINTING → Continuous Smooth Zoom
    //  Point your index finger, then move hand Up to zoom in, Down to zoom out.
    // =========================================================================
    if (gesture == "POINTING") {
        glm::vec3 pointPos = m_filterPinch.add(h1.position);

        if (m_hasPrevPinchPos) {
            float dy = pointPos.y - m_prevPinchPos.y;

            // In screen coordinates: Y is 0 at top, 1 at bottom.
            // Moving hand UP means dy is negative -> zoom IN (negative camera.zoom)
            // Moving hand DOWN means dy is positive -> zoom OUT (positive camera.zoom)
            if (std::abs(dy) < 0.4f) {
                if (std::abs(dy) > 0.001f) {
                    camera.zoom(dy * 30.0f);
                    m_currentAction = (dy < 0) ? "Pointing: Zoom In" : "Pointing: Zoom Out";
                } else {
                    m_currentAction = "Pointing: Locked";
                }
            }
        } else {
            m_currentAction = "Pointing: Zoom Engaged";
        }

        m_prevPinchPos      = pointPos;
        m_hasPrevPinchPos   = true;
        m_hasPrevPrimaryPos = false;
        m_hasPrevPeacePos   = false;
        m_prevPrimaryGesture = "POINTING";
        return;
    }

    // =========================================================================
    //  4. OPEN_PALM → Environment Panning
    // =========================================================================
    if (gesture == "OPEN_PALM") {
        double currentTime = glfwGetTime();
        if (currentTime > m_panCooldownUntil) {
            if (m_hasPrevPrimaryPos && m_prevPrimaryGesture == "OPEN_PALM") {
                float dx = pos1.x - m_prevPrimaryPos.x;
                float dy = pos1.y - m_prevPrimaryPos.y;

                if (std::abs(dx) < 0.3f && std::abs(dy) < 0.3f) {
                    if (std::abs(dx) > 0.001f || std::abs(dy) > 0.001f) {
                        camera.pan(dx * 30.0f, dy * 30.0f);
                        m_currentAction = "Open Palm: Pan";
                    }
                }
            }
        }
        m_prevPrimaryPos    = pos1;
        m_hasPrevPrimaryPos = true;
        m_hasPrevPinchPos   = false;
        m_hasPrevPeacePos   = false;
        m_prevPrimaryGesture = "OPEN_PALM";
        return;
    }

    // =========================================================================
    //  5. FIST → Neutral / Cooldown Reset
    // =========================================================================
    if (gesture == "FIST") {
        m_panCooldownUntil  = 0.0;
        m_hasPrevPrimaryPos = false;
        m_hasPrevPinchPos   = false;
        m_hasPrevPeacePos   = false;
        m_currentAction     = "Neutral (Fist)";
        m_prevPrimaryGesture = "FIST";
        return;
    }

    // Default: Reset movement anchors for unhandled gestures
    m_hasPrevPrimaryPos = false;
    m_hasPrevPinchPos   = false;
    m_prevPrimaryGesture = gesture;
}
