#include <Windows.h>
#include "Proxy.h"
#include "Hooks.h"
#include <tchar.h>

HINSTANCE g_hInstance;
HMODULE g_hOriginal;
HANDLE g_hCom;

void SetupCom() {
	DCB dcb;
	GetCommState(g_hCom,&dcb);
	dcb.BaudRate=CBR_256000;
    dcb.ByteSize=8;
    dcb.StopBits=ONESTOPBIT;
    dcb.Parity=NOPARITY;
	TCHAR szTemp[1024];
	wsprintf(szTemp,TEXT("Baud rate: %d\r\n"),dcb.BaudRate);
	OutputDebugString(szTemp);
	SetCommState(g_hCom,&dcb);
}

BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:{
		g_hInstance=hinstDll;
		g_hCom=CreateFileA("\\\\.\\COM10",GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		if (g_hCom==INVALID_HANDLE_VALUE)
			return FALSE;
		SetupCom();
		TCHAR szTemp[MAX_PATH];
		GetModuleFileName(hinstDll,szTemp,MAX_PATH);
		lstrcpy(&szTemp[lstrlen(szTemp)-3],TEXT("clone.dll"));
		g_hOriginal=LoadLibrary(szTemp);
		if (!g_hOriginal) {
			CloseHandle(g_hCom);
			return FALSE;
		}
		InitializeProxy(g_hOriginal);
		InitializeHooks(g_hOriginal);
							}break;
	case DLL_PROCESS_DETACH:{
		FreeLibrary(g_hOriginal);
		CloseHandle(g_hCom);
		DeinitializeHooks();
							}break;
	}
	return TRUE;
}