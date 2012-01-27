#ifndef HOOKS_H
#define HOOKS_H
#include <Windows.h>

bool InitializeHooks(HMODULE hOriginal);
void DeinitializeHooks();
#endif//HOOKS_H
