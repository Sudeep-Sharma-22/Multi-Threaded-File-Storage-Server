#pragma once

#include <cstdint>
#include <string>

namespace mtfss {

constexpr uint16_t PROTOCOL_VERSION    = 1;
constexpr uint16_t DEFAULT_PORT        = 9090;
constexpr int      DEFAULT_THREAD_POOL = 4;
constexpr size_t   TRANSFER_BUFFER_SIZE = 8192;

} // namespace mtfss