#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

class Camera;
struct DataPacket;

/**
 * @struct MovingAverage3D
 * @brief Sliding window filter that smooths 3D coordinates to eliminate webcam landmark jitter.
 */
struct MovingAverage3D {
    int capacity;
    std::vector<glm::vec3> window;
    glm::vec3 sum;
    int index;

    MovingAverage3D(int cap = 5);
    glm::vec3 add(const glm::vec3& val);
    void reset();
};

/**
 * @class GestureController
 * @brief Processes UDP hand tracking telemetry and maps discrete and two-hand composite
 *        gestures to camera navigation actions (Orbit, Pan, Zoom, Reset).
 */
class GestureController {
public:
    GestureController();
    ~GestureController() = default;

    /**
     * @brief Processes the latest telemetry packet and commands the camera.
     * @param packet Data packet received via UDP from Python AI backend.
     * @param camera 3D camera to navigate.
     */
    void processGestures(const DataPacket& packet, Camera& camera);

    /** Returns the human-readable name of the current active action for UI telemetry. */
    const std::string& getCurrentAction() const { return m_currentAction; }

    /** Returns the number of hands currently detected. */
    int getDetectedHandCount() const { return m_detectedHandCount; }

    /** Returns the primary hand gesture. */
    const std::string& getPrimaryGesture() const { return m_primaryGesture; }

    /** Returns the secondary hand gesture if 2 hands present. */
    const std::string& getSecondaryGesture() const { return m_secondaryGesture; }

private:
    /** @brief Hysteresis update: promotes rawGesture to m_lockedGesture only after
     *         kHysteresisFrames consecutive frames. Prevents brief misclassifications
     *         from interrupting an ongoing gesture action. */
    void updateLockedGesture(const std::string& rawGesture);

    MovingAverage3D m_filterHand1;
    MovingAverage3D m_filterHand2;
    MovingAverage3D m_filterPeace;
    MovingAverage3D m_filterPinch;

    // ---- Hysteresis gesture lock ----
    std::string m_lockedGesture;   // The currently active, committed gesture
    std::string m_pendingGesture;  // Gesture being evaluated for promotion
    int         m_pendingCount;    // How many consecutive frames it has been seen
    static constexpr int kHysteresisFrames = 2; // Frames needed to confirm a gesture switch

    // Previous frame tracking state
    std::string m_prevPrimaryGesture;
    glm::vec3   m_prevPrimaryPos;
    bool        m_hasPrevPrimaryPos;

    glm::vec3   m_prevPeacePos;
    bool        m_hasPrevPeacePos;

    // ---- Smooth Pinch & Drag Zoom ----
    glm::vec3   m_prevPinchPos;
    bool        m_hasPrevPinchPos;
    float       m_prevPinchDistance;
    bool        m_hasPrevPinchDist;

    // ---- Dual-Hand Expansion Zoom ----
    float       m_prevDualHandDist;
    bool        m_hasPrevDualHandDist;

    // Cooldown timers (in seconds)
    double m_panCooldownUntil;

    // Telemetry display state
    std::string m_currentAction;
    std::string m_primaryGesture;
    std::string m_secondaryGesture;
    int         m_detectedHandCount;
};
