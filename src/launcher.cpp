#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include "shared.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace {
std::mutex g_output;
std::atomic<bool> g_quit=false;
void line(char const* s) {
    std::lock_guard<std::mutex> lock(g_output);
    std::printf("%s\n",s);std::fflush(stdout);
}
DWORD findGame() {
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snap==INVALID_HANDLE_VALUE)return 0;
    PROCESSENTRY32W p{};p.dwSize=sizeof(p);
    DWORD pid=0;
    if(Process32FirstW(snap,&p))do {
        if(_wcsicmp(p.szExeFile,L"GeometryDash.exe")==0) {pid=p.th32ProcessID;break;}
    } while(Process32NextW(snap,&p));
    CloseHandle(snap);return pid;
}
std::uintptr_t remoteBase(DWORD pid,wchar_t const* module) {
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(snap==INVALID_HANDLE_VALUE)return 0;
    MODULEENTRY32W m{};m.dwSize=sizeof(m);
    std::uintptr_t addr=0;
    if(Module32FirstW(snap,&m))do {
        if(_wcsicmp(m.szModule,module)==0){addr=reinterpret_cast<std::uintptr_t>(m.modBaseAddr);break;}
    } while(Module32NextW(snap,&m));
    CloseHandle(snap);return addr;
}
std::wstring helperPath() {
    wchar_t path[32768]{};
    DWORD len=GetModuleFileNameW(nullptr,path,32768);
    if(!len || len>=32768)return {};
    return (std::filesystem::path(path).parent_path()/L"SupleqBufferHook.dll").wstring();
}
// Deliberately limited to a game process the user launched. No persistence.
bool inject(DWORD pid, std::wstring const& dll) {
    HANDLE proc=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|
        PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
    if(!proc)return false;
    auto finish=[&](bool ok){CloseHandle(proc);return ok;};
    HMODULE k32=GetModuleHandleW(L"kernel32.dll");
    auto load=k32 ? GetProcAddress(k32,"LoadLibraryW") : nullptr;
    if(!load)return finish(false);
    // LoadLibraryW may be forwarded into KernelBase on modern Windows.
    // Find the true exporting module instead of assuming kernel32.
    HMODULE localOwner=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(load), &localOwner) || !localOwner)
        return finish(false);
    wchar_t modulePath[MAX_PATH]{};
    if(!GetModuleFileNameW(localOwner,modulePath,MAX_PATH))return finish(false);
    auto baseName=std::filesystem::path(modulePath).filename().wstring();
    std::uintptr_t remoteOwner=remoteBase(pid,baseName.c_str());
    if(!remoteOwner)return finish(false);
    std::uintptr_t loadRemote=remoteOwner+(
        reinterpret_cast<std::uintptr_t>(load)-reinterpret_cast<std::uintptr_t>(localOwner));
    size_t bytes=(dll.size()+1)*sizeof(wchar_t);
    void* remote=VirtualAllocEx(proc,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!remote)return finish(false);
    SIZE_T written=0;
    bool okay=WriteProcessMemory(proc,remote,dll.c_str(),bytes,&written)
               &&written==bytes;
    HANDLE th=okay?CreateRemoteThread(proc,nullptr,0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(loadRemote),remote,0,nullptr):nullptr;
    DWORD code=0;
    bool ok=th && WaitForSingleObject(th,7000)==WAIT_OBJECT_0 &&
         GetExitCodeThread(th,&code) && code!=0;
    if(th)CloseHandle(th);
    VirtualFreeEx(proc,remote,0,MEM_RELEASE);
    return finish(ok);
}
void pipeReader() {
    while(!g_quit) {
        HANDLE p=CreateNamedPipeW(buffer::kPipe,PIPE_ACCESS_INBOUND,
           PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,1,4096,4096,0,nullptr);
        if(p==INVALID_HANDLE_VALUE)return;
        BOOL ok=ConnectNamedPipe(p,nullptr);
        if(!ok && GetLastError()!=ERROR_PIPE_CONNECTED){CloseHandle(p);continue;}
        char b[600]{};DWORD n=0;
        while(!g_quit && ReadFile(p,b,sizeof(b)-1,&n,nullptr) && n>0) {
            b[n]=0;
            std::lock_guard<std::mutex> lock(g_output);
            std::fputs(b,stdout);std::fflush(stdout);
        }
        DisconnectNamedPipe(p);CloseHandle(p);
    }
}
}
int wmain() {
    SetConsoleTitleW(L"Predictive Input Buffer - Native Console [Experimental]");
    SetConsoleOutputCP(CP_UTF8);
    line("+---------------------------------------------------------------+");
    line("|   SUPLEQ INPUT BUFFER - LIVE CONSOLE  (NO GEODE REQUIRED)     |");
    line("+---------------------------------------------------------------+");
    line("[InputBuffer] This is an experimental native injector for GD 2.2081.");
    line("[InputBuffer] It does NOT yet predict collision-safe clicks.");
    line("[InputBuffer] GRID aligns clicks to a tick grid; EXACT tests a known delay.");
    line("[InputBuffer] Dual mode and unsupported gamemodes pass through raw.");
    line("[Controls] F6 stages: 2 / 4 / 8 / 8 / OFF | F7 ARM/SAFE | F8 GRID/EXACT");
    line("[Controls] F9 verbose logs | END exit and disarm");
    line("[InputBuffer] Start with EXACT mode on an EASY solo wave, not a rated level.");
    auto dll=helperPath();
    if(dll.empty() || !std::filesystem::exists(dll)) {
        line("[ERROR] SupleqBufferHook.dll must be next to the .exe");return 2;
    }
    HANDLE map=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,
        sizeof(buffer::Control),buffer::kMapping);
    if(!map){line("[ERROR] Control channel initialization failed.");return 3;}
    auto* cfg=static_cast<buffer::Control*>(MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,sizeof(buffer::Control)));
    if(!cfg){CloseHandle(map);return 4;}
    cfg->magic=buffer::kMagic;cfg->protocol=buffer::kProtocol;
    cfg->armed=0;cfg->stage=0;cfg->mode=1;cfg->exactTicks=2;cfg->logEdges=1;
    cfg->programAliveMs=static_cast<LONG>(GetTickCount());
    std::thread reader(pipeReader);reader.detach();
    bool prev[4]{};
    DWORD injected=0;
    DWORD lastPoll=0;
    while(true) {
        cfg->programAliveMs=static_cast<LONG>(GetTickCount());
        if(GetAsyncKeyState(VK_END)&1)break;
        int keys[]={VK_F6,VK_F7,VK_F8,VK_F9};
        for(int i=0;i<4;++i) {
            bool cur=(GetAsyncKeyState(keys[i])&0x8000)!=0;
            if(cur && !prev[i]) {
                if(i==0){cfg->stage=(cfg->stage+1)%buffer::kStages;
                    char msg[200];
                    if(cfg->stage==4)std::snprintf(msg,sizeof(msg),
                       "[InputBuffer] Stage 5: OFF (100%% raw input)");
                    else std::snprintf(msg,sizeof(msg),
                       "[InputBuffer] Stage %ld: %d Ticks All / %d Ticks Wave",
                       cfg->stage+1,buffer::kStageAll[cfg->stage],
                       buffer::kStageWave[cfg->stage]);
                    line(msg);
                } else if(i==1){cfg->armed=!cfg->armed;
                    line(cfg->armed?"[InputBuffer] ARMED (experimental input delay)":
                       "[InputBuffer] SAFE: raw input passthrough");
                } else if(i==2){cfg->mode=!cfg->mode;
                    line(cfg->mode==1?"[InputBuffer] EXACT: fixed 2 physics ticks":
                       "[InputBuffer] GRID: stage-aligned ticks (not predictive)");
                } else {cfg->logEdges=!cfg->logEdges;
                    line(cfg->logEdges?"[InputBuffer] Input logs ON":"[InputBuffer] Input logs OFF");}
            }
            prev[i]=cur;
        }
        auto time=GetTickCount();
        if(time-lastPoll>1400) {
            lastPoll=time;
            DWORD pid=findGame();
            if(pid && pid!=injected) {
                line("[InputBuffer] GeometryDash.exe detected; loading native helper...");
                if(inject(pid,dll)){injected=pid;line("[InputBuffer] Loader succeeded; waiting for hook confirmation...");}
                else {line("[WARN] Unable to load helper. Check GD version, access, antivirus.");lastPoll=time+5000;}
            } else if(!pid && injected){injected=0;cfg->armed=0;
                line("[InputBuffer] GD closed, correction disarmed.");}
        }
        Sleep(30);
    }
    cfg->armed=0;
    line("[InputBuffer] Exiting. Native correction disabled; restart GD to unload helper.");
    // The helper checks the heartbeat and stops intercepting when the launcher
    // is closed. The mapping and pipe live until Windows tears down the handles.
    Sleep(100);
    UnmapViewOfFile(cfg);CloseHandle(map);
    g_quit=true;
    return 0;
}
