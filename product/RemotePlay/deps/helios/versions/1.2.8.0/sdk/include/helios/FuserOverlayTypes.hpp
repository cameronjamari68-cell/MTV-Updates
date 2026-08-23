#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Helios {

constexpr std::size_t FUSER_OVERLAY_CACHELINE_SIZE = 64;
constexpr uint32_t FUSER_OVERLAY_SLOT_COUNT = 8;
constexpr uint32_t FUSER_OVERLAY_MAX_COMMANDS = 4096;
constexpr uint32_t FUSER_OVERLAY_TEXT_CAPACITY = 64;
constexpr uint32_t FUSER_OVERLAY_DESIGN_WIDTH = 1920;
constexpr uint32_t FUSER_OVERLAY_DESIGN_HEIGHT = 1080;

enum class FuserOverlayCommandType : uint32_t {
    None = 0,
    Line = 1,
    Rect = 2,
    CornerRect = 3,
    CircleOutline = 4,
    Cross = 5,
    XMark = 6,
    FilledSquare = 7,
    Text = 8,
    FilledRect = 9,
    FilledCircle = 10,
};

enum class FuserOverlayTextAnchor : uint32_t {
    TopLeft = 0,
    TopCenter = 1,
    TopRight = 2,
    CenterLeft = 3,
    Center = 4,
    CenterRight = 5,
    BottomLeft = 6,
    BottomCenter = 7,
    BottomRight = 8,
};

enum class FuserOverlayCoordinateSpace : uint32_t {
    Pixels = 0,
    Design1080p = 1,
};

struct alignas(FUSER_OVERLAY_CACHELINE_SIZE) FuserOverlayCommand {
    FuserOverlayCommandType type{FuserOverlayCommandType::None};
    uint32_t colorRgba{0xFFFFFFFF};
    int32_t x1{0};
    int32_t y1{0};
    int32_t x2{0};
    int32_t y2{0};
    int32_t radius{0};
    uint32_t thickness{1};
    uint32_t scale{1};
    uint32_t flags{0};
    uint32_t backgroundColorRgba{0};
    int32_t padding{0};
    FuserOverlayTextAnchor textAnchor{FuserOverlayTextAnchor::TopLeft};
    FuserOverlayCoordinateSpace coordinateSpace{FuserOverlayCoordinateSpace::Pixels};
    char text[FUSER_OVERLAY_TEXT_CAPACITY]{};
};

static_assert(sizeof(FuserOverlayCommand) == 128, "Fuser overlay command size must remain fixed.");
static_assert(alignof(FuserOverlayCommand) == FUSER_OVERLAY_CACHELINE_SIZE);

struct alignas(FUSER_OVERLAY_CACHELINE_SIZE) FuserOverlaySlot {
    std::atomic<uint64_t> sequence{0};
    std::atomic<uint32_t> commandCount{0};
    std::atomic<uint32_t> readyCommandCount{0};
    uint32_t frameWidth{0};
    uint32_t frameHeight{0};
    uint64_t timestampNs{0};
    std::atomic<uint32_t> primaryPublished{0};
    uint32_t reserved[8]{};
    FuserOverlayCommand commands[FUSER_OVERLAY_MAX_COMMANDS]{};
};

static_assert(offsetof(FuserOverlaySlot, commands) == 128,
              "Fuser overlay slot header layout must remain fixed.");
static_assert(alignof(FuserOverlaySlot) == FUSER_OVERLAY_CACHELINE_SIZE);
static_assert(sizeof(FuserOverlaySlot) % FUSER_OVERLAY_CACHELINE_SIZE == 0);

struct alignas(FUSER_OVERLAY_CACHELINE_SIZE) FuserOverlayRing {
    std::atomic<uint64_t> latestSequence{0};
    std::atomic<uint32_t> enabled{0};
    uint32_t slotCount{FUSER_OVERLAY_SLOT_COUNT};
    uint32_t maxCommands{FUSER_OVERLAY_MAX_COMMANDS};
    char overlayReadyEventName[64]{};
    std::atomic<uint32_t> drawingEnabled{1};
    uint32_t reserved[11]{};
    FuserOverlaySlot frameSlots[FUSER_OVERLAY_SLOT_COUNT]{};
};

static_assert(offsetof(FuserOverlayRing, frameSlots) == 192,
              "Fuser overlay ring header layout must remain fixed.");
static_assert(alignof(FuserOverlayRing) == FUSER_OVERLAY_CACHELINE_SIZE);
static_assert(sizeof(FuserOverlayRing) % FUSER_OVERLAY_CACHELINE_SIZE == 0);

inline uint32_t fuserOverlayColorRgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (static_cast<uint32_t>(a) << 24)
        | (static_cast<uint32_t>(r) << 16)
        | (static_cast<uint32_t>(g) << 8)
        | static_cast<uint32_t>(b);
}

inline uint32_t fuserOverlayColorFromBgr(uint8_t b, uint8_t g, uint8_t r, uint8_t a = 255) {
    return fuserOverlayColorRgba(r, g, b, a);
}

inline int32_t fuserOverlayScaleCoordinate(
    int32_t value,
    uint32_t frameExtent,
    uint32_t designExtent,
    FuserOverlayCoordinateSpace space) {
    if (space != FuserOverlayCoordinateSpace::Design1080p || frameExtent == 0) {
        return value;
    }

    const int64_t numerator = static_cast<int64_t>(value) * static_cast<int64_t>(frameExtent);
    const int64_t bias = value >= 0
        ? static_cast<int64_t>(designExtent / 2)
        : -static_cast<int64_t>(designExtent / 2);
    return static_cast<int32_t>((numerator + bias) / static_cast<int64_t>(designExtent));
}

inline int32_t fuserOverlayScaleX(
    int32_t value,
    uint32_t frameWidth,
    FuserOverlayCoordinateSpace space) {
    return fuserOverlayScaleCoordinate(value, frameWidth, FUSER_OVERLAY_DESIGN_WIDTH, space);
}

inline int32_t fuserOverlayScaleY(
    int32_t value,
    uint32_t frameHeight,
    FuserOverlayCoordinateSpace space) {
    return fuserOverlayScaleCoordinate(value, frameHeight, FUSER_OVERLAY_DESIGN_HEIGHT, space);
}

inline int32_t fuserOverlayScaleLength(
    int32_t value,
    uint32_t frameHeight,
    FuserOverlayCoordinateSpace space) {
    if (space != FuserOverlayCoordinateSpace::Design1080p || frameHeight == 0 || value == 0) {
        return value;
    }

    const int64_t signedValue = value;
    const uint64_t magnitude = static_cast<uint64_t>(signedValue < 0 ? -signedValue : signedValue);
    uint64_t scaled = (magnitude * static_cast<uint64_t>(frameHeight) + (FUSER_OVERLAY_DESIGN_HEIGHT / 2))
        / FUSER_OVERLAY_DESIGN_HEIGHT;
    if (scaled == 0) {
        scaled = 1;
    }
    return static_cast<int32_t>(signedValue < 0 ? -static_cast<int64_t>(scaled) : static_cast<int64_t>(scaled));
}

inline uint32_t fuserOverlayScaleLength(
    uint32_t value,
    uint32_t frameHeight,
    FuserOverlayCoordinateSpace space) {
    if (space != FuserOverlayCoordinateSpace::Design1080p || frameHeight == 0 || value == 0) {
        return value;
    }

    uint64_t scaled = (static_cast<uint64_t>(value) * static_cast<uint64_t>(frameHeight)
        + (FUSER_OVERLAY_DESIGN_HEIGHT / 2)) / FUSER_OVERLAY_DESIGN_HEIGHT;
    if (scaled == 0) {
        scaled = 1;
    }
    return static_cast<uint32_t>(scaled);
}

} // namespace Helios
