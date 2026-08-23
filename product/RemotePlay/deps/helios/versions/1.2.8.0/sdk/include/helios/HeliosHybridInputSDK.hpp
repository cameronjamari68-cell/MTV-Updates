#pragma once

#ifndef _WIN32
#error "HeliosHybridInputSDK.hpp requires Windows shared memory APIs."
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "HeliosInputABI.h"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

namespace Helios::CVPython::HybridInput {

inline constexpr std::size_t kSharedMemoryHeaderSize = 128;

inline constexpr const char* kEnvProcessId = "HELIOS_PROCESS_ID";
inline constexpr const char* kEnvControllerInput = "HELIOS_CONTROLLER_INPUT";
inline constexpr const char* kEnvControllerReport = "HELIOS_CONTROLLER_REPORT";
inline constexpr const char* kEnvKeyboardInput = "HELIOS_KEYBOARD_INPUT";
inline constexpr const char* kEnvKeyboardReport = "HELIOS_KEYBOARD_REPORT";
inline constexpr const char* kEnvMouseInput = "HELIOS_MOUSE_INPUT";
inline constexpr const char* kEnvMouseReport = "HELIOS_MOUSE_REPORT";

namespace Controller {
inline constexpr int BUTTON_1 = 0;
inline constexpr int BUTTON_2 = 1;
inline constexpr int BUTTON_3 = 2;
inline constexpr int BUTTON_4 = 3;
inline constexpr int BUTTON_5 = 4;
inline constexpr int BUTTON_6 = 5;
inline constexpr int BUTTON_7 = 6;
inline constexpr int BUTTON_8 = 7;
inline constexpr int BUTTON_9 = 8;
inline constexpr int BUTTON_10 = 9;
inline constexpr int BUTTON_11 = 10;
inline constexpr int BUTTON_12 = 11;
inline constexpr int BUTTON_13 = 12;
inline constexpr int BUTTON_14 = 13;
inline constexpr int BUTTON_15 = 14;
inline constexpr int BUTTON_16 = 15;
inline constexpr int BUTTON_17 = 16;
inline constexpr int BUTTON_18 = 17;
inline constexpr int BUTTON_19 = 18;
inline constexpr int BUTTON_20 = 19;
inline constexpr int BUTTON_21 = 20;
inline constexpr int STICK_1_X = 21;
inline constexpr int STICK_1_Y = 22;
inline constexpr int STICK_2_X = 23;
inline constexpr int STICK_2_Y = 24;
inline constexpr int POINT_1_X = 25;
inline constexpr int POINT_1_Y = 26;
inline constexpr int POINT_2_X = 27;
inline constexpr int POINT_2_Y = 28;
inline constexpr int ACCEL_1_X = 29;
inline constexpr int ACCEL_1_Y = 30;
inline constexpr int ACCEL_1_Z = 31;
inline constexpr int ACCEL_2_X = 32;
inline constexpr int ACCEL_2_Y = 33;
inline constexpr int ACCEL_2_Z = 34;
inline constexpr int GYRO_1_X = 35;
inline constexpr int GYRO_1_Y = 36;
inline constexpr int GYRO_1_Z = 37;
inline constexpr int PADDLE_1 = 38;
inline constexpr int PADDLE_2 = 39;
inline constexpr int PADDLE_3 = 40;
inline constexpr int PADDLE_4 = 41;
} // namespace Controller

namespace KeyboardModifier {
inline constexpr std::uint32_t SHIFT_LEFT = 1u << 0;
inline constexpr std::uint32_t SHIFT_RIGHT = 1u << 1;
inline constexpr std::uint32_t CTRL_LEFT = 1u << 2;
inline constexpr std::uint32_t CTRL_RIGHT = 1u << 3;
inline constexpr std::uint32_t ALT_LEFT = 1u << 4;
inline constexpr std::uint32_t ALT_RIGHT = 1u << 5;
inline constexpr std::uint32_t WIN_LEFT = 1u << 6;
inline constexpr std::uint32_t WIN_RIGHT = 1u << 7;
inline constexpr std::uint32_t CAPS_LOCK = 1u << 8;
inline constexpr std::uint32_t NUM_LOCK = 1u << 9;
inline constexpr std::uint32_t SCROLL_LOCK = 1u << 10;
} // namespace KeyboardModifier

namespace MouseButton {
inline constexpr std::uint32_t LEFT = 1u << 0;
inline constexpr std::uint32_t RIGHT = 1u << 1;
inline constexpr std::uint32_t MIDDLE = 1u << 2;
inline constexpr std::uint32_t X1 = 1u << 3;
inline constexpr std::uint32_t X2 = 1u << 4;
} // namespace MouseButton

struct SharedMemoryHeader {
    std::uint64_t writeSequence;
    std::uint64_t timestampNs;
    std::uint32_t dataReady;
    std::uint32_t dataSize;
    std::uint32_t maxDataSize;
    char writerInfo[32];
    char reserved[68];
};

using SharedMemoryControllerState = HeliosControllerState;
using ControllerData = HeliosControllerData;
using KeyboardData = HeliosKeyboardData;
using MouseCoordMode = HeliosMouseCoordMode;
using MouseMotionSample = HeliosMouseMotionSample;
using MouseData = HeliosMouseData;

static_assert(sizeof(SharedMemoryHeader) == kSharedMemoryHeaderSize, "SharedMemoryHeader must be 128 bytes.");

inline std::wstring wideFromUtf8(std::string_view value) {
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

inline std::string readEnvironment(const char* name) {
    if (!name || !*name) {
        return {};
    }

    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || !value) {
        return {};
    }
    std::string result(value);
    std::free(value);
    return result;
}

class SharedMemoryView {
public:
    SharedMemoryView() = default;
    ~SharedMemoryView() {
        close();
    }

    SharedMemoryView(const SharedMemoryView&) = delete;
    SharedMemoryView& operator=(const SharedMemoryView&) = delete;

    SharedMemoryView(SharedMemoryView&& other) noexcept {
        *this = std::move(other);
    }

    SharedMemoryView& operator=(SharedMemoryView&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        close();
        handle_ = std::exchange(other.handle_, nullptr);
        base_ = std::exchange(other.base_, nullptr);
        blockName_ = std::move(other.blockName_);
        return *this;
    }

    bool openReadOnly(const std::string& blockName, std::string* error = nullptr) noexcept {
        close();

        if (blockName.empty()) {
            if (error) {
                *error = "Shared memory block name is empty.";
            }
            return false;
        }

        const std::wstring wideName = wideFromUtf8(blockName);
        handle_ = OpenFileMappingW(FILE_MAP_READ, FALSE, wideName.c_str());
        if (!handle_) {
            if (error) {
                *error = "Failed to open shared memory mapping.";
            }
            return false;
        }

        base_ = static_cast<const std::uint8_t*>(MapViewOfFile(handle_, FILE_MAP_READ, 0, 0, 0));
        if (!base_) {
            if (error) {
                *error = "Failed to map shared memory view.";
            }
            CloseHandle(handle_);
            handle_ = nullptr;
            return false;
        }

        blockName_ = blockName;
        return true;
    }

    void close() noexcept {
        if (base_) {
            UnmapViewOfFile(base_);
            base_ = nullptr;
        }
        if (handle_) {
            CloseHandle(handle_);
            handle_ = nullptr;
        }
        blockName_.clear();
    }

    bool isOpen() const noexcept {
        return base_ != nullptr;
    }

    explicit operator bool() const noexcept {
        return isOpen();
    }

    const std::string& blockName() const noexcept {
        return blockName_;
    }

    const SharedMemoryHeader* header() const noexcept {
        return isOpen() ? reinterpret_cast<const SharedMemoryHeader*>(base_) : nullptr;
    }

    const std::uint8_t* dataBytes() const noexcept {
        return isOpen() ? (base_ + kSharedMemoryHeaderSize) : nullptr;
    }

    bool isReady() const noexcept {
        const SharedMemoryHeader* const view = header();
        return view && view->dataReady != 0;
    }

    std::uint64_t writeSequence() const noexcept {
        const SharedMemoryHeader* const view = header();
        if (!view) {
            return 0;
        }

        std::uint64_t value = 0;
        std::memcpy(&value, &view->writeSequence, sizeof(value));
        return value;
    }

    std::uint32_t dataSize() const noexcept {
        const SharedMemoryHeader* const view = header();
        if (!view) {
            return 0;
        }

        std::uint32_t value = 0;
        std::memcpy(&value, &view->dataSize, sizeof(value));
        return value;
    }

    std::wstring updatedEventName() const {
        if (blockName_.empty()) {
            return {};
        }
        return wideFromUtf8("Global\\" + blockName_ + "_Updated");
    }

private:
    HANDLE handle_{nullptr};
    const std::uint8_t* base_{nullptr};
    std::string blockName_;
};

template <typename T>
class TypedBlock : public SharedMemoryView {
public:
    const T* mapped() const noexcept {
        if (!isReady() || dataSize() < sizeof(T)) {
            return nullptr;
        }
        return reinterpret_cast<const T*>(dataBytes());
    }

    bool snapshot(T* output) const noexcept {
        if (!output) {
            return false;
        }

        const SharedMemoryHeader* const view = header();
        if (!view || view->dataReady == 0 || dataSize() < sizeof(T)) {
            return false;
        }

        const std::uint64_t before = writeSequence();
        std::memcpy(output, dataBytes(), sizeof(T));
        const std::uint64_t after = writeSequence();
        return before != 0 && before == after;
    }
};

class ControllerBlock : public TypedBlock<ControllerData> {
public:
    float get(int identifier) const noexcept {
        const ControllerData* const data = mapped();
        if (!data || identifier < 0 || identifier > Controller::PADDLE_4) {
            return 0.0f;
        }

        if (identifier <= Controller::BUTTON_21) {
            return data->state.buttons[static_cast<std::size_t>(identifier)];
        }
        return data->state.axes[static_cast<std::size_t>(identifier - Controller::STICK_1_X)];
    }

    const float* buttons() const noexcept {
        const ControllerData* const data = mapped();
        return data ? data->state.buttons : nullptr;
    }

    const float* axes() const noexcept {
        const ControllerData* const data = mapped();
        return data ? data->state.axes : nullptr;
    }
};

class KeyboardBlock : public TypedBlock<KeyboardData> {
public:
    bool isKeyDown(std::size_t virtualKey) const noexcept {
        const KeyboardData* const data = mapped();
        return data && virtualKey < HELIOS_KEY_COUNT && data->keys[virtualKey] != 0;
    }

    std::uint32_t modifiers() const noexcept {
        const KeyboardData* const data = mapped();
        return data ? data->modifiers : 0;
    }
};

class MouseBlock : public TypedBlock<MouseData> {
public:
    bool isButtonDown(std::uint32_t buttonMask) const noexcept {
        const MouseData* const data = mapped();
        return data && (data->buttons & buttonMask) != 0;
    }
};

struct BlockNames {
    std::string controllerInput;
    std::string controllerReport;
    std::string keyboardInput;
    std::string keyboardReport;
    std::string mouseInput;
    std::string mouseReport;

    static BlockNames fromEnvironment() {
        return {
            readEnvironment(kEnvControllerInput),
            readEnvironment(kEnvControllerReport),
            readEnvironment(kEnvKeyboardInput),
            readEnvironment(kEnvKeyboardReport),
            readEnvironment(kEnvMouseInput),
            readEnvironment(kEnvMouseReport),
        };
    }
};

class InputReportSources {
public:
    ControllerBlock controllerInput;
    ControllerBlock controllerReport;
    KeyboardBlock keyboardInput;
    KeyboardBlock keyboardReport;
    MouseBlock mouseInput;
    MouseBlock mouseReport;

    bool openAllFromEnvironment(std::string* error = nullptr) noexcept {
        return openAll(BlockNames::fromEnvironment(), error);
    }

    bool openAll(const BlockNames& names, std::string* error = nullptr) noexcept {
        close();

        const std::array<const std::string*, 6> required{
            &names.controllerInput,
            &names.controllerReport,
            &names.keyboardInput,
            &names.keyboardReport,
            &names.mouseInput,
            &names.mouseReport,
        };

        bool missingRequiredName = false;
        for (const std::string* value : required) {
            if (!value || value->empty()) {
                missingRequiredName = true;
                break;
            }
        }

        if (missingRequiredName) {
            if (error) {
                *error = "Missing required Helios shared memory configuration.";
            }
            return false;
        }

        auto openBlock = [&](auto& block, const std::string& blockName) -> bool {
            if (block.openReadOnly(blockName)) {
                return true;
            }
            if (error) {
                *error = "Required Helios shared memory is unavailable.";
            }
            close();
            return false;
        };

        return openBlock(controllerInput, names.controllerInput)
            && openBlock(controllerReport, names.controllerReport)
            && openBlock(keyboardInput, names.keyboardInput)
            && openBlock(keyboardReport, names.keyboardReport)
            && openBlock(mouseInput, names.mouseInput)
            && openBlock(mouseReport, names.mouseReport);
    }

    void close() noexcept {
        controllerInput.close();
        controllerReport.close();
        keyboardInput.close();
        keyboardReport.close();
        mouseInput.close();
        mouseReport.close();
    }
};

} // namespace Helios::CVPython::HybridInput
