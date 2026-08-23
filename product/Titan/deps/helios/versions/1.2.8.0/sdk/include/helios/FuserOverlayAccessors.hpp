#pragma once

#include "FuserOverlayTypes.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Helios {

// Contention on overlay slots lasts nanoseconds (short memcpy windows), so spin
// with pause first and only fall back to the scheduler if it drags on.
inline void fuserOverlaySpinWait(uint32_t& spinCount) {
#ifdef _WIN32
    if (++spinCount < 64) {
        YieldProcessor();
        return;
    }
#endif
    std::this_thread::yield();
}

class FuserOverlayMapping {
public:
    FuserOverlayMapping() = default;
    FuserOverlayMapping(const FuserOverlayMapping&) = delete;
    FuserOverlayMapping& operator=(const FuserOverlayMapping&) = delete;

    ~FuserOverlayMapping() {
        disconnect();
    }

    bool connect(const std::string& memoryName) {
        disconnect();

#ifdef _WIN32
        const std::wstring wideName(memoryName.begin(), memoryName.end());
        m_mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, wideName.c_str());
        if (!m_mapping) {
            return false;
        }

        m_ring = static_cast<FuserOverlayRing*>(
            MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FuserOverlayRing)));
        if (!m_ring) {
            CloseHandle(m_mapping);
            m_mapping = nullptr;
            return false;
        }

        const std::string eventName(m_ring->overlayReadyEventName);
        const std::wstring wideEventName(eventName.begin(), eventName.end());
        m_event = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, wideEventName.c_str());
        if (!m_event) {
            UnmapViewOfFile(m_ring);
            m_ring = nullptr;
            CloseHandle(m_mapping);
            m_mapping = nullptr;
            return false;
        }

        return true;
#else
        (void)memoryName;
        return false;
#endif
    }

    void disconnect() {
#ifdef _WIN32
        if (m_event) {
            CloseHandle(m_event);
            m_event = nullptr;
        }
        if (m_ring) {
            UnmapViewOfFile(m_ring);
            m_ring = nullptr;
        }
        if (m_mapping) {
            CloseHandle(m_mapping);
            m_mapping = nullptr;
        }
#endif
    }

    bool isConnected() const {
        return m_ring != nullptr;
    }

    bool isEnabled() const {
        return m_ring && m_ring->enabled.load(std::memory_order_acquire) != 0;
    }

    bool isDrawingEnabled() const {
        return m_ring && m_ring->drawingEnabled.load(std::memory_order_acquire) != 0;
    }

    void setEnabled(bool enabled) {
        if (m_ring) {
            m_ring->enabled.store(enabled ? 1u : 0u, std::memory_order_release);
        }
    }

    void setDrawingEnabled(bool enabled) {
        if (m_ring) {
            m_ring->drawingEnabled.store(enabled ? 1u : 0u, std::memory_order_release);
        }
    }

    FuserOverlayRing* ring() const {
        return m_ring;
    }

#ifdef _WIN32
    HANDLE eventHandle() const {
        return m_event;
    }
#endif

protected:
    bool signalReady() {
#ifdef _WIN32
        return m_event && SetEvent(m_event);
#else
        return false;
#endif
    }

private:
#ifdef _WIN32
    HANDLE m_mapping{nullptr};
    HANDLE m_event{nullptr};
#endif
    FuserOverlayRing* m_ring{nullptr};
};

class FuserOverlayWriter final : public FuserOverlayMapping {
public:
    bool beginFrame(uint64_t sequence, uint32_t width, uint32_t height, uint64_t timestampNs) {
        FuserOverlayRing* const overlay = ring();
        if (!overlay || overlay->enabled.load(std::memory_order_acquire) == 0 || sequence == 0) {
            return false;
        }
        if (overlay->latestSequence.load(std::memory_order_acquire) > sequence) {
            return false;
        }

        FuserOverlaySlot& slot = overlay->frameSlots[sequence % overlay->slotCount];
        uint64_t observed = slot.sequence.load(std::memory_order_acquire);
        if (observed == sequence) {
            return true;
        }
        if (observed != UINT64_MAX && observed > sequence) {
            return false;
        }

        uint32_t spinCount = 0;
        for (;;) {
            observed = slot.sequence.load(std::memory_order_acquire);
            if (observed == sequence) {
                return true;
            }
            if (observed == UINT64_MAX) {
                fuserOverlaySpinWait(spinCount);
                continue;
            }
            if (observed > sequence) {
                return false;
            }

            uint64_t expected = observed;
            if (slot.sequence.compare_exchange_strong(
                    expected,
                    UINT64_MAX,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                slot.commandCount.store(0, std::memory_order_relaxed);
                slot.readyCommandCount.store(0, std::memory_order_relaxed);
                slot.primaryPublished.store(0, std::memory_order_relaxed);
                slot.frameWidth = width;
                slot.frameHeight = height;
                slot.timestampNs = timestampNs;
                slot.sequence.store(sequence, std::memory_order_release);
                return true;
            }
        }
    }

    bool addCommand(uint64_t sequence, const FuserOverlayCommand& command) {
        FuserOverlayRing* const overlay = ring();
        if (!overlay || overlay->enabled.load(std::memory_order_acquire) == 0 || sequence == 0) {
            return false;
        }

        FuserOverlaySlot& slot = overlay->frameSlots[sequence % overlay->slotCount];
        if (slot.sequence.load(std::memory_order_acquire) != sequence) {
            return false;
        }

        const uint32_t index = slot.commandCount.fetch_add(1, std::memory_order_acq_rel);
        if (index >= overlay->maxCommands) {
            return false;
        }

        slot.commands[index] = command;
        uint32_t spinCount = 0;
        while (slot.readyCommandCount.load(std::memory_order_acquire) != index) {
            fuserOverlaySpinWait(spinCount);
        }
        slot.readyCommandCount.store(index + 1, std::memory_order_release);
        return true;
    }

    bool addCommands(uint64_t sequence, const FuserOverlayCommand* commands, uint32_t commandCount) {
        FuserOverlayRing* const overlay = ring();
        if (!overlay || overlay->enabled.load(std::memory_order_acquire) == 0 || sequence == 0) {
            return false;
        }
        if (commandCount == 0) {
            return true;
        }

        FuserOverlaySlot& slot = overlay->frameSlots[sequence % overlay->slotCount];
        if (slot.sequence.load(std::memory_order_acquire) != sequence) {
            return false;
        }

        const uint32_t first = slot.commandCount.fetch_add(commandCount, std::memory_order_acq_rel);
        if (first >= overlay->maxCommands) {
            return false;
        }

        const uint32_t writable = std::min(commandCount, overlay->maxCommands - first);
        std::memcpy(&slot.commands[first], commands, static_cast<size_t>(writable) * sizeof(FuserOverlayCommand));
        uint32_t spinCount = 0;
        while (slot.readyCommandCount.load(std::memory_order_acquire) != first) {
            fuserOverlaySpinWait(spinCount);
        }
        slot.readyCommandCount.store(first + writable, std::memory_order_release);
        return writable == commandCount;
    }

    bool publishFrame(uint64_t sequence) {
        FuserOverlayRing* const overlay = ring();
        if (!overlay || overlay->enabled.load(std::memory_order_acquire) == 0 || sequence == 0) {
            return false;
        }

        FuserOverlaySlot& slot = overlay->frameSlots[sequence % overlay->slotCount];
        if (slot.sequence.load(std::memory_order_acquire) != sequence) {
            return false;
        }

        overlay->latestSequence.store(sequence, std::memory_order_release);
        slot.primaryPublished.store(1, std::memory_order_release);
        return signalReady();
    }

    bool publishFrameUpdate(uint64_t sequence) {
        FuserOverlayRing* const overlay = ring();
        if (!overlay || overlay->enabled.load(std::memory_order_acquire) == 0 || sequence == 0) {
            return false;
        }

        FuserOverlaySlot& slot = overlay->frameSlots[sequence % overlay->slotCount];
        if (slot.sequence.load(std::memory_order_acquire) != sequence) {
            return false;
        }
        if (slot.primaryPublished.load(std::memory_order_acquire) == 0) {
            return true;
        }
        if (overlay->latestSequence.load(std::memory_order_acquire) != sequence) {
            return true;
        }
        return signalReady();
    }

    bool isFrameSuperseded(uint64_t sequence) const {
        const FuserOverlayRing* const overlay = ring();
        if (!overlay || sequence == 0) {
            return false;
        }
        if (overlay->latestSequence.load(std::memory_order_acquire) > sequence) {
            return true;
        }

        const uint64_t slotSequence =
            overlay->frameSlots[sequence % overlay->slotCount].sequence.load(std::memory_order_acquire);
        return slotSequence != UINT64_MAX && slotSequence > sequence;
    }

    bool addLine(uint64_t sequence,
                 int x1,
                 int y1,
                 int x2,
                 int y2,
                 uint32_t colorRgba,
                 uint32_t thickness = 1,
                 FuserOverlayCoordinateSpace coordinateSpace = FuserOverlayCoordinateSpace::Pixels) {
        FuserOverlayCommand command{};
        command.type = FuserOverlayCommandType::Line;
        command.colorRgba = colorRgba;
        command.x1 = x1;
        command.y1 = y1;
        command.x2 = x2;
        command.y2 = y2;
        command.thickness = std::max(1u, thickness);
        command.coordinateSpace = coordinateSpace;
        return addCommand(sequence, command);
    }

    bool addRect(uint64_t sequence,
                 int x1,
                 int y1,
                 int x2,
                 int y2,
                 uint32_t colorRgba,
                 uint32_t thickness = 1,
                 FuserOverlayCoordinateSpace coordinateSpace = FuserOverlayCoordinateSpace::Pixels) {
        FuserOverlayCommand command{};
        command.type = FuserOverlayCommandType::Rect;
        command.colorRgba = colorRgba;
        command.x1 = x1;
        command.y1 = y1;
        command.x2 = x2;
        command.y2 = y2;
        command.thickness = std::max(1u, thickness);
        command.coordinateSpace = coordinateSpace;
        return addCommand(sequence, command);
    }

    bool addFilledRect(uint64_t sequence,
                       int x1,
                       int y1,
                       int x2,
                       int y2,
                       uint32_t colorRgba,
                       FuserOverlayCoordinateSpace coordinateSpace = FuserOverlayCoordinateSpace::Pixels) {
        FuserOverlayCommand command{};
        command.type = FuserOverlayCommandType::FilledRect;
        command.colorRgba = colorRgba;
        command.x1 = x1;
        command.y1 = y1;
        command.x2 = x2;
        command.y2 = y2;
        command.coordinateSpace = coordinateSpace;
        return addCommand(sequence, command);
    }

    bool addCornerRect(uint64_t sequence,
                       int x1,
                       int y1,
                       int x2,
                       int y2,
                       uint32_t colorRgba,
                       uint32_t thickness = 1,
                       FuserOverlayCoordinateSpace coordinateSpace = FuserOverlayCoordinateSpace::Pixels) {
        FuserOverlayCommand command{};
        command.type = FuserOverlayCommandType::CornerRect;
        command.colorRgba = colorRgba;
        command.x1 = x1;
        command.y1 = y1;
        command.x2 = x2;
        command.y2 = y2;
        command.thickness = std::max(1u, thickness);
        command.coordinateSpace = coordinateSpace;
        return addCommand(sequence, command);
    }

    bool addCircleOutline(uint64_t sequence,
                          int x,
                          int y,
                          int radius,
                          uint32_t colorRgba,
                          uint32_t thickness = 1,
                          FuserOverlayCoordinateSpace coordinateSpace = FuserOverlayCoordinateSpace::Pixels) {
        FuserOverlayCommand command{};
        command.type = FuserOverlayCommandType::CircleOutline;
        command.colorRgba = colorRgba;
        command.x1 = x;
        command.y1 = y;
        command.radius = radius;
        command.thickness = std::max(1u, thickness);
        command.coordinateSpace = coordinateSpace;
        return addCommand(sequence, command);
    }

    bool addFilledCircle(uint64_t sequence,
                         int x,
                         int y,
                         int radius,
                         uint32_t colorRgba,
                         FuserOverlayCoordinateSpace coordinateSpace = FuserOverlayCoordinateSpace::Pixels) {
        FuserOverlayCommand command{};
        command.type = FuserOverlayCommandType::FilledCircle;
        command.colorRgba = colorRgba;
        command.x1 = x;
        command.y1 = y;
        command.radius = radius;
        command.thickness = 1;
        command.coordinateSpace = coordinateSpace;
        return addCommand(sequence, command);
    }

    bool addCross(uint64_t sequence,
                  int x,
                  int y,
                  int size,
                  uint32_t colorRgba,
                  uint32_t thickness = 1,
                  FuserOverlayCoordinateSpace coordinateSpace = FuserOverlayCoordinateSpace::Pixels) {
        FuserOverlayCommand command{};
        command.type = FuserOverlayCommandType::Cross;
        command.colorRgba = colorRgba;
        command.x1 = x;
        command.y1 = y;
        command.radius = size;
        command.thickness = std::max(1u, thickness);
        command.coordinateSpace = coordinateSpace;
        return addCommand(sequence, command);
    }

    bool addXMark(uint64_t sequence,
                  int x,
                  int y,
                  int size,
                  uint32_t colorRgba,
                  uint32_t thickness = 1,
                  FuserOverlayCoordinateSpace coordinateSpace = FuserOverlayCoordinateSpace::Pixels) {
        FuserOverlayCommand command{};
        command.type = FuserOverlayCommandType::XMark;
        command.colorRgba = colorRgba;
        command.x1 = x;
        command.y1 = y;
        command.radius = size;
        command.thickness = std::max(1u, thickness);
        command.coordinateSpace = coordinateSpace;
        return addCommand(sequence, command);
    }

    bool addFilledSquare(uint64_t sequence,
                         int x,
                         int y,
                         int radius,
                         uint32_t colorRgba,
                         FuserOverlayCoordinateSpace coordinateSpace = FuserOverlayCoordinateSpace::Pixels) {
        FuserOverlayCommand command{};
        command.type = FuserOverlayCommandType::FilledSquare;
        command.colorRgba = colorRgba;
        command.x1 = x;
        command.y1 = y;
        command.radius = radius;
        command.thickness = 1;
        command.coordinateSpace = coordinateSpace;
        return addCommand(sequence, command);
    }

    bool addText(uint64_t sequence,
                 int x,
                 int y,
                 const char* text,
                 uint32_t colorRgba,
                 uint32_t scale = 1,
                 FuserOverlayTextAnchor anchor = FuserOverlayTextAnchor::TopLeft,
                 uint32_t backgroundColorRgba = 0,
                 int32_t padding = 0,
                 FuserOverlayCoordinateSpace coordinateSpace = FuserOverlayCoordinateSpace::Pixels) {
        FuserOverlayCommand command{};
        command.type = FuserOverlayCommandType::Text;
        command.colorRgba = colorRgba;
        command.x1 = x;
        command.y1 = y;
        command.scale = std::max(1u, scale);
        command.backgroundColorRgba = backgroundColorRgba;
        command.padding = padding;
        command.textAnchor = anchor;
        command.coordinateSpace = coordinateSpace;
        if (text) {
            strncpy_s(command.text, sizeof(command.text), text, _TRUNCATE);
        }
        return addCommand(sequence, command);
    }
};

class FuserOverlayReader final : public FuserOverlayMapping {
public:
    const FuserOverlaySlot* latestSlot(uint64_t* sequenceOut = nullptr) const {
        const FuserOverlayRing* const overlay = ring();
        if (!overlay) {
            return nullptr;
        }

        const uint64_t sequence = overlay->latestSequence.load(std::memory_order_acquire);
        if (sequence == 0) {
            return nullptr;
        }

        const FuserOverlaySlot& slot = overlay->frameSlots[sequence % overlay->slotCount];
        if (slot.sequence.load(std::memory_order_acquire) != sequence) {
            return nullptr;
        }
        if (slot.primaryPublished.load(std::memory_order_acquire) == 0) {
            return nullptr;
        }

        if (sequenceOut) {
            *sequenceOut = sequence;
        }
        return &slot;
    }
};

} // namespace Helios
