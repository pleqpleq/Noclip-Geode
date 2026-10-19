#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <MinHook.h>
#include "shared.hpp"
#include "timing.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace {
using HandleButtonFn = void(__fastcall*)(void*,bool,int,bool);
using ProcessCommandsFn = void(__fastcall*)(void*,float,bool,bool);
using ResetLevelFn = void(__fastcall*)(void*);
ResetLevelFn g_resetOriginal = nullptr;
HandleButtonFn g_handleOriginal = nullptr;
ProcessCommandsFn g_processOriginal = nullptr;
buffer::Control* g_control = nullptr;
HANDLE g_mapping = nullptr;
HANDLE g_pipe = INVALID_HANDLE_VALUE;
HANDLE g_thread = nullptr;
std::uint64_t g_tick = 0;
void* g_lastLayer = nullptr;
buffer::Queue g_queue;
bool g_dispatching = false;
bool g_lastBypassed = false;
bool g_warnedVersion = false;

// All offset values below are **for Windows GD 2.2081 only**.
// They come from independently published analyses, NOT portable APIs.
constexpr std::uintptr_t kRvaHandleButton = 0x2338e0;
constexpr std::uintptr_t kRvaProcessCommands = 0x239c60;
constexpr std::uintptr_t kRvaResetLevel = 0x3b8eb0;
constexpr std::size_t kOffPlayer1 = 0xda0;
constexpr std::size_t kOffDual = 0x422;
constexpr std::size_t kOffWave = 0x9bc;
constexpr std::size_t kOffDead = 0x9c0;
constexpr std::size_t kOffPlatformer = 0xb70;

template<class T> T field(void* object,std::size_t offset) {
    if(!object) return T{};
    T out{};
    __try {
        std::memcpy(&out, reinterpret_cast<unsigned char*>(object)+offset,sizeof(T));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return T{};
    }
    return out;
}

void logLine(char const* line) {
    if(g_pipe==INVALID_HANDLE_VALUE) {
        g_pipe=CreateFileW(buffer::kPipe,GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,nullptr);
        if(g_pipe==INVALID_HANDLE_VALUE) return;
    }
    DWORD n=0;
    if(!WriteFile(g_pipe,line,static_cast<DWORD>(std::strlen(line)),&n,nullptr)) {
        CloseHandle(g_pipe);
        g_pipe=INVALID_HANDLE_VALUE;
    }
}
void logf(char const* fmt, unsigned long long a=0,
          unsigned long long b=0,unsigned long long c=0) {
    char str[400]{};
    sprintf_s(str,fmt,a,b,c);
    logLine(str);
}
bool isActive() {
    if(!g_control || g_control->magic!=static_cast<long>(buffer::kMagic)
       || g_control->protocol != static_cast<long>(buffer::kProtocol))return false;
    DWORD last=static_cast<DWORD>(g_control->programAliveMs);
    // Fail open when launcher terminates. Never hold buttons indefinitely.
    if(GetTickCount() - last > 3500UL) return false;
    int stage=g_control->stage;
    return g_control->armed && stage>=0 && stage<4;
}
bool isWaveSolo(void* layer) {
    if(!layer || field<bool>(layer,kOffDual))return false;
    void* p = field<void*>(layer,kOffPlayer1);
    if(!p || field<bool>(p,kOffDead) || field<bool>(p,kOffPlatformer))return false;
    return field<bool>(p,kOffWave);
}
void dispatch(void* layer,buffer::Edge const& e) {
    g_dispatching=true;
    g_handleOriginal(layer,e.down,e.button,e.p1);
    g_dispatching=false;
    unsigned long long wait=g_tick>=e.received?g_tick-e.received:0;
    logf("[InputBuffer] DELIVERED +%llu TICKS (%llu -> %llu)\n",
         wait,e.received,g_tick);
}
void flush(void* layer) {
    if(!layer || !g_handleOriginal){g_queue.clear();return;}
    g_queue.flush([layer](buffer::Edge e){dispatch(layer,e);});
}
void __fastcall handleHook(void* layer,bool down,int button,bool isPlayer1) {
    if(!g_handleOriginal)return;
    if(g_dispatching) {g_handleOriginal(layer,down,button,isPlayer1);return;}
    if(layer!=g_lastLayer) {
        // New level / editor instance. Stale inputs belong to old layer.
        g_queue.clear();g_tick=0;g_lastLayer=layer;
    }
    if(!isActive() || !isPlayer1 || button!=1 || !isWaveSolo(layer)) {
        if(!g_queue.empty())flush(layer);
        g_handleOriginal(layer,down,button,isPlayer1);
        return;
    }
    unsigned n=static_cast<unsigned>(buffer::kStageWave[g_control->stage]);
    std::uint64_t due=(g_control->mode==1)
        ?buffer::fixedTarget(g_tick,static_cast<unsigned>(
             std::clamp(static_cast<int>(g_control->exactTicks),1,8)))
        :buffer::gridTarget(g_tick,n);
    if(due==g_tick && g_queue.empty()){
        g_handleOriginal(layer,down,button,isPlayer1);
        return;
    }
    g_queue.push(buffer::Edge{down,true,button,g_tick,due});
    if(g_control->logEdges) {
        char str[256];
        sprintf_s(str,"[InputBuffer] QUEUED %s tick=%llu -> %llu (mode=%s)\n",
            down?"PRESS":"RELEASE",static_cast<unsigned long long>(g_tick),
            static_cast<unsigned long long>(due),g_control->mode==1?"EXACT":"GRID");
        logLine(str);
    }
}
void __fastcall processHook(void* layer,float dt,bool half,bool last) {
    if(!g_processOriginal)return;
    if(layer!=g_lastLayer){g_lastLayer=layer;g_tick=0;g_queue.clear();}
    if(!half)++g_tick;
    if(!g_queue.empty()) {
        if(!isActive() || !isWaveSolo(layer))flush(layer);
        else if(!half)g_queue.fireDue(g_tick,[layer](buffer::Edge e){dispatch(layer,e);});
    }
    g_processOriginal(layer,dt,half,last);
}

void __fastcall resetHook(void* layer) {
    g_queue.clear();g_tick=0;g_lastLayer=layer;
    if(g_resetOriginal)g_resetOriginal(layer);
}

bool regionLooksExecutable(unsigned char* p) {
    MEMORY_BASIC_INFORMATION mbi{};
    if(!VirtualQuery(p,&mbi,sizeof(mbi)))return false;
    if(mbi.State!=MEM_COMMIT || mbi.Protect&PAGE_GUARD)return false;
    DWORD prot=mbi.Protect&0xFF;
    return prot==PAGE_EXECUTE || prot==PAGE_EXECUTE_READ ||
           prot==PAGE_EXECUTE_READWRITE || prot==PAGE_EXECUTE_WRITECOPY;
}
DWORD WINAPI setup(LPVOID) {
    g_mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,buffer::kMapping);
    if(!g_mapping)return 1;
    g_control=static_cast<buffer::Control*>(MapViewOfFile(g_mapping,FILE_MAP_READ,0,0,
                                                  sizeof(buffer::Control)));
    if(!g_control)return 2;
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if(!base)return 3;
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return 4;
    auto* pe=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE ||
       pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return 5;
    auto imageSize=pe->OptionalHeader.SizeOfImage;
    if(imageSize<kRvaProcessCommands+512 || imageSize>0x10000000)return 6;
    auto* handle=base+kRvaHandleButton;
    auto* process=base+kRvaProcessCommands;
    auto* reset=base+kRvaResetLevel;
    if(!regionLooksExecutable(handle) || !regionLooksExecutable(process)
       || !regionLooksExecutable(reset))return 7;
    if(MH_Initialize()!=MH_OK)return 8;
    if(MH_CreateHook(handle,reinterpret_cast<LPVOID>(handleHook),
             reinterpret_cast<LPVOID*>(&g_handleOriginal))!=MH_OK)return 9;
    if(MH_CreateHook(process,reinterpret_cast<LPVOID>(processHook),
             reinterpret_cast<LPVOID*>(&g_processOriginal))!=MH_OK)return 10;
    if(MH_CreateHook(reset,reinterpret_cast<LPVOID>(resetHook),
            reinterpret_cast<LPVOID*>(&g_resetOriginal))!=MH_OK)return 12;
    if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK)return 11;
    logLine("[InputBuffer] Native hooks installed (Geometry Dash 2.2081 offsets)\n");
    logLine("[InputBuffer] GRID mode is tick alignment, NOT trajectory prediction.\n");
    return 0;
}
}
BOOL APIENTRY DllMain(HMODULE handle,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(handle);
        g_thread=CreateThread(nullptr,0,setup,nullptr,0,nullptr);
    }
    return TRUE;
}
