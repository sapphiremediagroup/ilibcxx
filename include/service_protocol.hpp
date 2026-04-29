#pragma once

#include <cstddef.hpp>
#include <cstdint.hpp>
#include <cstring.hpp>
#include <syscall.hpp>

namespace std::services {

struct MessageHeader {
    std::uint16_t version;
    std::uint16_t opcode;
    std::uint32_t reserved;
};

inline constexpr std::uint16_t STATUS_OK = 0;
inline constexpr std::uint16_t STATUS_BAD_VERSION = 1;
inline constexpr std::uint16_t STATUS_BAD_OPCODE = 2;
inline constexpr std::uint16_t STATUS_BAD_PAYLOAD = 3;

template <typename T>
inline bool encode_request(std::IPCMessage* message, const T& payload) noexcept {
    if (message == nullptr || sizeof(T) > sizeof(message->data)) {
        return false;
    }

    std::memset(message, 0, sizeof(*message));
    message->flags = std::IPC_MESSAGE_REQUEST;
    message->size = sizeof(T);
    std::memcpy(message->data, &payload, sizeof(T));
    return true;
}

template <typename T>
inline bool decode_message(const std::IPCMessage& message, T* payload) noexcept {
    if (payload == nullptr || message.size < sizeof(T)) {
        return false;
    }

    std::memcpy(payload, message.data, sizeof(T));
    return true;
}

template <typename T>
inline bool decode_reply(const void* buffer, std::uint64_t size, T* payload) noexcept {
    if (payload == nullptr || buffer == nullptr || size < sizeof(T)) {
        return false;
    }

    std::memcpy(payload, buffer, sizeof(T));
    return true;
}

namespace graphics_compositor {
inline constexpr char NAME[] = "graphics.compositor";
inline constexpr std::uint16_t VERSION = 1;

enum class Opcode : std::uint16_t {
    Hello = 1,
};

struct HelloRequest {
    MessageHeader header;
};

struct HelloReply {
    MessageHeader header;
    std::uint16_t status;
    std::uint16_t reserved0;
    char service_name[32];
};
}

namespace surfaces {
inline constexpr std::uint32_t FORMAT_BGRA8 = 1;
}

namespace input_manager {
inline constexpr char NAME[] = "input.manager";
inline constexpr std::uint16_t VERSION = 1;
}

namespace storage_manager {
inline constexpr char NAME[] = "storage.manager";
inline constexpr std::uint16_t VERSION = 1;
}

namespace process_manager {
inline constexpr char NAME[] = "process.manager";
inline constexpr std::uint16_t VERSION = 1;
}

namespace font_manager {
inline constexpr char NAME[] = "font.manager";
inline constexpr std::uint16_t VERSION = 1;
}

namespace session_manager {
inline constexpr char NAME[] = "session.manager";
inline constexpr std::uint16_t VERSION = 1;
}

}
