// Диагностика падений: пишет адреса стека в km_crash.log (расшифровка: addr2line -e "KnopMops Engine.exe" -f -C <адрес>).
#include <windows.h>
#include <cstdio>

namespace {

LONG WINAPI CrashFilter(EXCEPTION_POINTERS* info) {
    FILE* f = std::fopen("km_crash.log", "w");
    if (!f) f = stderr;
    std::fprintf(f, "Exception 0x%08lX at %p\n", info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress);
    void* frames[48];
    USHORT n = CaptureStackBackTrace(0, 48, frames, nullptr);
    for (USHORT i = 0; i < n; ++i) std::fprintf(f, "  #%u %p\n", (unsigned)i, frames[i]);
    if (f != stderr) std::fclose(f);
    return EXCEPTION_CONTINUE_SEARCH;
}

}

void InstallCrashHandler() {
    SetUnhandledExceptionFilter(CrashFilter);
}
