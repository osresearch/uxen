#ifndef PROCESS_H_
#define PROCESS_H_

#include <windows.h>

DWORD find_pid(const char *name);
DWORD suspend_pid(DWORD pid);
DWORD resume_pid(DWORD pid);

#endif
