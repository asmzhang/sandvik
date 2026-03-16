#pragma once
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <string>

#define RTLD_NOW   0
#define RTLD_LOCAL 0
#define RTLD_GLOBAL 0

/* Sentinel: dlopen(nullptr) means "search all loaded modules" */
#define _DLOPEN_SELF ((void*)(uintptr_t)1)

inline void* dlopen(const char* path, int /*flags*/) {
    if (!path || path[0] == '\0') return _DLOPEN_SELF;
    return (void*)LoadLibraryA(path);
}

inline void* dlsym(void* handle, const char* name) {
    if (handle == _DLOPEN_SELF) {
        /* Search all loaded modules in this process */
        HANDLE hProc = GetCurrentProcess();
        HMODULE mods[1024];
        DWORD needed = 0;
        if (EnumProcessModules(hProc, mods, sizeof(mods), &needed)) {
            DWORD count = needed / sizeof(HMODULE);
            for (DWORD i = 0; i < count; ++i) {
                void* sym = (void*)GetProcAddress(mods[i], name);
                if (sym) return sym;
            }
        }
        return nullptr;
    }
    return (void*)GetProcAddress((HMODULE)handle, name);
}

inline int dlclose(void* handle) {
    if (handle == _DLOPEN_SELF) return 0;
    return FreeLibrary((HMODULE)handle) ? 0 : -1;
}

inline const char* dlerror() {
    static char buf[512];
    DWORD err = GetLastError();
    if (!err) return nullptr;
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, 0, buf, sizeof(buf), nullptr);
    return buf;
}
#else
#include_next <dlfcn.h>
#endif
