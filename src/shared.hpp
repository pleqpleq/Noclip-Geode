#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <cstdint>

namespace buffer {
constexpr wchar_t kMapping[] = L"Local\\SupleqNativeBuffer_v1";
constexpr wchar_t kPipe[] = L"\\\\.\\pipe\\SupleqNativeBuffer_v1";
constexpr std::uint32_t kMagic = 0x53555051; // SUPQ
constexpr std::uint32_t kProtocol = 1;
struct Control {
    volatile long magic;
    volatile long protocol;
    volatile long armed;
    volatile long stage;
    volatile long mode;           // 0=grid, 1=exact delay
    volatile long exactTicks;
    volatile long logEdges;
    volatile long programAliveMs;
};
constexpr int kStageAll[] = {2, 4, 8, 16, 0};
constexpr int kStageWave[] = {2, 4, 8, 8, 0};
constexpr int kStages = 5;
}
