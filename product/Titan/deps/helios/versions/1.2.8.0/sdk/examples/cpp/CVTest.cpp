/**
 * CV C++ Test Script
 * 
 * This DLL demonstrates:
 * - Controller input visualization (all buttons, sticks, triggers)
 * - CV data packing and output
 * - Frame processing with visual feedback
 */

#include <helios/HeliosCVSDK.h>
#include <algorithm>
#include <cstddef>
#include <vector>
#include <random>
#include <string>
#include <cstdint>


class CVWorker {
private:
    std::vector<uint8_t> cvdata;
    std::string noun;
    std::vector<std::string> nouns;
    int frameCount;
    std::mt19937 rng;
    std::uniform_int_distribution<int> nounDist;
    
public:
    CVWorker(std::uint32_t, std::uint32_t)
        : frameCount(0), rng(std::random_device{}()) {
        
        
        // Initialize CV data buffer
        cvdata.resize(48, 0x00);
        
        // Initialize noun list (same as Python version)
        nouns = {
            "user", "player", "member", "patron", "client", "consumer", "operator", 
            "utilizer", "handler", "contender", "competitor", "contestant", "performer", 
            "actor", "rival", "protagonist", "antagonist", "challenger", "adversary", 
            "opponent", "friend", "teammate", "peer", "counterpart", "enemy"
        };
        
        noun = "user";
        nounDist = std::uniform_int_distribution<int>(
            0,
            static_cast<int>(nouns.size() - 1));
        
    }
    
    ~CVWorker() { }
    
    void process(Helios::Frame& frame) {
        // Reset CV data for this frame
        cvdata.assign(48, 0x00);
        
        // Work directly on the input frame - ZERO COPY!
        // The frame parameter is already pointing to shared memory
        if (frame.image.empty()) {
            return;
        }
        
        // Draw all controller elements through the overlay API.
        drawMenuButtons();
        drawFaceButtons();
        drawDpad();
        drawTriggers();
        drawBumpers();
        drawRightStick();
        drawLeftStick();
        
        // Draw text elements
        Helios::Overlay::text("Hello, " + noun, cv::Point(790, 200),
                             cv::Scalar(255, 255, 255), 2, Helios::Overlay::Target::Both);
        
        if (frameCount == 60) {
            noun = nouns[nounDist(rng)];
            frameCount = 0;
        }
        
        Helios::Overlay::text(std::to_string(frameCount), cv::Point(10, 20),
                             cv::Scalar(255, 255, 255), 2, Helios::Overlay::Target::Both);
        
        frameCount++;
        
        // Set frame count in CV data
        cvdata[0] = static_cast<uint8_t>(frameCount & 0xFF);
        
        Helios::Controls::sendCvData(cvdata.data(), cvdata.size());
    }
    
private:
    void drawMenuButtons() {
        // Menu buttons: BUTTON_2 and BUTTON_3
        struct ButtonInfo {
            int button;
            cv::Point pos;
        };
        
        std::vector<ButtonInfo> buttons = {
            {Helios::Controls::Controller::BUTTON_2, cv::Point(250, 220)},
            {Helios::Controls::Controller::BUTTON_3, cv::Point(320, 220)}
        };
        
        for (const auto& btn : buttons) {
            float value = Helios::Controls::getActual(btn.button);
            int color = static_cast<int>(value * 2.55f);
            writeButton(btn.button, value);
            Helios::Overlay::circle(btn.pos, 12, cv::Scalar(color, color, color), 1,
                                    Helios::Overlay::Target::Both, true);
            Helios::Overlay::circle(btn.pos, 12, cv::Scalar(255, 255, 255), 3,
                                    Helios::Overlay::Target::Both);
        }
    }
    
    void drawFaceButtons() {
        // Face buttons: BUTTON_14, 15, 16, 17 (Y/Triangle, B/Circle, A/Cross, X/Square)
        struct ButtonInfo {
            int button;
            cv::Point pos;
        };
        
        std::vector<ButtonInfo> buttons = {
            {Helios::Controls::Controller::BUTTON_14, cv::Point(430, 190)}, // Y/Triangle
            {Helios::Controls::Controller::BUTTON_15, cv::Point(460, 220)}, // B/Circle
            {Helios::Controls::Controller::BUTTON_16, cv::Point(430, 250)}, // A/Cross
            {Helios::Controls::Controller::BUTTON_17, cv::Point(400, 220)}  // X/Square
        };
        
        for (const auto& btn : buttons) {
            float value = Helios::Controls::getActual(btn.button);
            int color = static_cast<int>(value * 2.55f);
            writeButton(btn.button, value);
            Helios::Overlay::circle(btn.pos, 15, cv::Scalar(color, color, color), 1,
                                    Helios::Overlay::Target::Both, true);
            Helios::Overlay::circle(btn.pos, 15, cv::Scalar(255, 255, 255), 3,
                                    Helios::Overlay::Target::Both);
        }
    }
    
    void drawDpad() {
        // D-pad: BUTTON_10, 11, 12, 13 (Up, Down, Left, Right)
        struct ButtonInfo {
            int button;
            cv::Point pos;
        };
        
        std::vector<ButtonInfo> dpad = {
            {Helios::Controls::Controller::BUTTON_10, cv::Point(190, 270)}, // Up
            {Helios::Controls::Controller::BUTTON_11, cv::Point(190, 330)}, // Down
            {Helios::Controls::Controller::BUTTON_12, cv::Point(160, 300)}, // Left
            {Helios::Controls::Controller::BUTTON_13, cv::Point(220, 300)}  // Right
        };
        
        for (const auto& btn : dpad) {
            float value = Helios::Controls::getActual(btn.button);
            int color = static_cast<int>(value * 2.55f);
            writeButton(btn.button, value);
            Helios::Overlay::rect(btn.pos, cv::Point(btn.pos.x + 27, btn.pos.y + 27),
                                 cv::Scalar(color, color, color), 1,
                                 Helios::Overlay::Target::Both, true);
            Helios::Overlay::rect(btn.pos, cv::Point(btn.pos.x + 27, btn.pos.y + 27),
                                 cv::Scalar(255, 255, 255), 3,
                                 Helios::Overlay::Target::Both);
        }
    }
    
    void drawTriggers() {
        // Triggers: BUTTON_8 (LT) and BUTTON_5 (RT)
        struct TriggerInfo {
            int button;
            cv::Point pos;
        };
        
        std::vector<TriggerInfo> triggers = {
            {Helios::Controls::Controller::BUTTON_8, cv::Point(90, 310)}, // LT
            {Helios::Controls::Controller::BUTTON_5, cv::Point(440, 310)}  // RT
        };
        
        for (const auto& trigger : triggers) {
            float value = Helios::Controls::getActual(trigger.button);
            int offset = static_cast<int>(value / 2);
            int color = static_cast<int>(value * 2.55f);
            writeButton(trigger.button, value);
            
            Helios::Overlay::rect(cv::Point(trigger.pos.x, trigger.pos.y + offset),
                                 cv::Point(trigger.pos.x + 30, trigger.pos.y + 50),
                                 cv::Scalar(color, color, color), 1,
                                 Helios::Overlay::Target::Both, true);
            Helios::Overlay::rect(cv::Point(trigger.pos.x, trigger.pos.y + offset),
                                 cv::Point(trigger.pos.x + 30, trigger.pos.y + 50),
                                 cv::Scalar(255, 255, 255), 3,
                                 Helios::Overlay::Target::Both);
        }
    }
    
    void drawBumpers() {
        // Bumpers: BUTTON_7 (LB) and BUTTON_4 (RB)
        struct ButtonInfo {
            int button;
            cv::Point pos;
        };
        
        std::vector<ButtonInfo> bumpers = {
            {Helios::Controls::Controller::BUTTON_7, cv::Point(95, 120)}, // LB
            {Helios::Controls::Controller::BUTTON_4, cv::Point(395, 120)}  // RB
        };
        
        for (const auto& btn : bumpers) {
            float value = Helios::Controls::getActual(btn.button);
            int color = static_cast<int>(value * 2.55f);
            writeButton(btn.button, value);
            Helios::Overlay::rect(btn.pos, cv::Point(btn.pos.x + 65, btn.pos.y + 20),
                                 cv::Scalar(color, color, color), 1,
                                 Helios::Overlay::Target::Both, true);
            Helios::Overlay::rect(btn.pos, cv::Point(btn.pos.x + 65, btn.pos.y + 20),
                                 cv::Scalar(255, 255, 255), 3,
                                 Helios::Overlay::Target::Both);
        }
    }
    
    void drawLeftStick() {
        cv::Point anchor(130, 220);
        Helios::Overlay::circle(anchor, 45, cv::Scalar(255, 255, 255), 2, Helios::Overlay::Target::Both);
        
        float xVal = Helios::Controls::getActual(Helios::Controls::Controller::STICK_2_X);
        float yVal = Helios::Controls::getActual(Helios::Controls::Controller::STICK_2_Y);
        float click = Helios::Controls::getActual(Helios::Controls::Controller::BUTTON_9);
        
        int clickColor = static_cast<int>(click * 2.55f);
        writeButton(Helios::Controls::Controller::BUTTON_9, click);
        
        cv::Point stickPos(anchor.x + static_cast<int>(xVal * 0.20f), 
                          anchor.y + static_cast<int>(yVal * 0.20f));
        Helios::Overlay::circle(stickPos, 37, cv::Scalar(clickColor, clickColor, clickColor), 1,
                                Helios::Overlay::Target::Both, true);
        
        int xyColor = static_cast<int>((std::abs(xVal) + std::abs(yVal)) * 2.25f) + 30;
        
        writeStick(2, xVal);
        writeStick(3, yVal);
        
        Helios::Overlay::circle(stickPos, 35, cv::Scalar(xyColor, xyColor, xyColor), 7, Helios::Overlay::Target::Both);
    }
    
    void drawRightStick() {
        cv::Point anchor(360, 305);
        Helios::Overlay::circle(anchor, 45, cv::Scalar(255, 255, 255), 2, Helios::Overlay::Target::Both);
        
        float xVal = Helios::Controls::getActual(Helios::Controls::Controller::STICK_1_X);
        float yVal = Helios::Controls::getActual(Helios::Controls::Controller::STICK_1_Y);
        float click = Helios::Controls::getActual(Helios::Controls::Controller::BUTTON_6);
        
        int clickColor = static_cast<int>(click * 2.55f);
        writeButton(Helios::Controls::Controller::BUTTON_6, click);
        
        cv::Point stickPos(anchor.x + static_cast<int>(xVal * 0.20f), 
                          anchor.y + static_cast<int>(yVal * 0.20f));
        Helios::Overlay::circle(stickPos, 37, cv::Scalar(clickColor, clickColor, clickColor), 1,
                                Helios::Overlay::Target::Both, true);
        
        int xyColor = static_cast<int>((std::abs(xVal) + std::abs(yVal)) * 2.25f) + 30;
        
        writeStick(0, xVal);
        writeStick(1, yVal);
        
        Helios::Overlay::circle(stickPos, 35, cv::Scalar(xyColor, xyColor, xyColor), 7, Helios::Overlay::Target::Both);
    }

    void writeButton(int button, float value) {
        const int encoded = std::clamp(static_cast<int>(value), 0, 100) | 1;
        cvdata[11 + static_cast<std::size_t>(button)] = static_cast<uint8_t>(encoded);
    }

    void writeStick(std::size_t index, float value) {
        const float clamped = std::clamp(value, -100.0f, 100.0f);
        cvdata[28 + index] = static_cast<uint8_t>((static_cast<int>(clamped) + 100) | 1);

        const uint32_t raw = static_cast<uint32_t>(
            static_cast<int32_t>(clamped * 65536.0f));
        const std::size_t offset = 32 + index * sizeof(raw);
        cvdata[offset] = static_cast<uint8_t>(raw >> 24);
        cvdata[offset + 1] = static_cast<uint8_t>(raw >> 16);
        cvdata[offset + 2] = static_cast<uint8_t>(raw >> 8);
        cvdata[offset + 3] = static_cast<uint8_t>(raw);
    }
};

HELIOS_CV_SCRIPT(CVWorker)
