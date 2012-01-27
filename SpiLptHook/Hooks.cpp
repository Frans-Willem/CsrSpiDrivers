#include <Windows.h>
#include <map>
#include <stdlib.h>
#include <tchar.h>

struct FAKEHANDLEINFO {
	char *szFakeFile;
};

CRITICAL_SECTION csFakes;

std::map<HANDLE,FAKEHANDLEINFO> g_mFakeHandles;
extern HANDLE g_hCom;

HANDLE WINAPI HookedCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess,DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
	if (lstrcmpA(lpFileName,"\\\\.\\LPTSPI1")==0) {
		EnterCriticalSection(&csFakes);
		char szTempPath[MAX_PATH]; GetTempPathA(MAX_PATH,szTempPath);
		char szTempFile[MAX_PATH];
		GetTempFileNameA(szTempPath,"lptspi",0,szTempFile);
		HANDLE hTemp=CreateFileA(szTempFile,dwDesiredAccess,dwShareMode,lpSecurityAttributes,CREATE_ALWAYS,dwFlagsAndAttributes,hTemplateFile);
		if (hTemp==INVALID_HANDLE_VALUE) {
			LeaveCriticalSection(&csFakes);
			return INVALID_HANDLE_VALUE;
		}
		FAKEHANDLEINFO fhi;
		fhi.szFakeFile=new char[lstrlenA(szTempFile)+1];
		lstrcpyA(fhi.szFakeFile,szTempFile);
		g_mFakeHandles[hTemp]=fhi;
		LeaveCriticalSection(&csFakes);
		return hTemp;
	}
	return CreateFileA(lpFileName,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
}

BOOL WINAPI HookedCloseHandle(HANDLE hObject) {
	EnterCriticalSection(&csFakes);
	std::map<HANDLE,FAKEHANDLEINFO>::iterator found=g_mFakeHandles.find(hObject);
	if (found!=g_mFakeHandles.end()) {
		CloseHandle(hObject);
		DeleteFileA(found->second.szFakeFile);
		delete[] found->second.szFakeFile;
		g_mFakeHandles.erase(found);
		LeaveCriticalSection(&csFakes);
		return TRUE;
	}
	LeaveCriticalSection(&csFakes);
	return CloseHandle(hObject);
}

#define BV_CHIPSELECT (1<<0) //1
#define BV_MOSI (1<<6) //0x40
#define BV_MISO (1<<6) /* NOTE: On status byte, not data byte! */
#define BV_CLK (1<<7) //0x80
#define IOBUFFER 100
//#define min(a,b) (((a)<(b))?(a):(b))

BOOL Bitbang(BYTE *pIn, DWORD dwInLength, BYTE *pOut, DWORD dwOutLength, DWORD *pdwReturned) {
	_tprintf(TEXT("Bitbang out:"));
	for (DWORD i=0; i<dwInLength; i++)
		_tprintf((i==0 || pIn[i]!=pIn[i-1])?TEXT(" %02X"):TEXT("."),pIn[i]);
	_tprintf(TEXT("\n"));
	BYTE *pRealIn=pIn;
	pIn=new BYTE[dwInLength];
	for (DWORD i=0; i<dwInLength; i++) {
		pIn[i]=0;
		if (pRealIn[i] & BV_CHIPSELECT)
			pIn[i]|=(1<<2); //
		if (pRealIn[i] & BV_MOSI)
			pIn[i]|=(1<<3); //MOSI
		if (pRealIn[i] & BV_CLK)
			pIn[i]|=(1<<5); //CLK
	}
	BYTE bTemp[IOBUFFER];
	for (DWORD dwIndex=0; dwIndex<dwInLength; dwIndex+=IOBUFFER) {
		DWORD dwWritten,dwRead;
		if (!WriteFile(g_hCom,&pIn[dwIndex],min(dwInLength-dwIndex,IOBUFFER),&dwWritten,0) || dwWritten!=min(dwInLength-dwIndex,IOBUFFER)) {
			delete[] pIn;
			return FALSE;
		}
		if (!ReadFile(g_hCom,bTemp,min(dwInLength-dwIndex,IOBUFFER),&dwRead,0) || dwRead!=min(dwInLength-dwIndex,IOBUFFER)) {
			delete[] pIn;
			return FALSE;
		}
		if (pOut && dwIndex<dwOutLength)
			memcpy(&pOut[dwIndex],bTemp,min(dwRead,dwOutLength-dwIndex));
	}
	if (pOut)
		for (DWORD i=0; i<dwOutLength; i++)
			pOut[i]=(pOut[i] & (1<<4))?(1<<6):0;
	if (pdwReturned)
		*pdwReturned=min(dwOutLength,dwInLength);
	_tprintf(TEXT("Bitbang in:"));
	for (DWORD i=0; i<dwOutLength; i++)
		_tprintf((i==0 || pOut[i]!=pOut[i-1])?TEXT(" %02X"):TEXT("."),pOut[i]);
	_tprintf(TEXT("\n"));
	//PrintInputOutput(pRealIn,pOut,nInBufferSize);
	delete[] pIn;
	return TRUE;
}

BOOL WINAPI HookedDeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
	EnterCriticalSection(&csFakes);
	std::map<HANDLE,FAKEHANDLEINFO>::iterator found=g_mFakeHandles.find(hDevice);
	if (found!=g_mFakeHandles.end()) {
		BOOL bRet=Bitbang((BYTE *)lpInBuffer,nInBufferSize,(BYTE *)lpOutBuffer,nOutBufferSize,lpBytesReturned);
		LeaveCriticalSection(&csFakes);
		return bRet;
	}
	LeaveCriticalSection(&csFakes);
	return DeviceIoControl(hDevice,dwIoControlCode,lpInBuffer,nInBufferSize,lpOutBuffer,nOutBufferSize,lpBytesReturned,lpOverlapped);
}

BOOL WINAPI HookedWriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {
	EnterCriticalSection(&csFakes);
	std::map<HANDLE,FAKEHANDLEINFO>::iterator found=g_mFakeHandles.find(hFile);
	if (found!=g_mFakeHandles.end()) {
		BOOL bRet=Bitbang((BYTE *)lpBuffer,nNumberOfBytesToWrite,0,0,0);
		if (lpNumberOfBytesWritten)
			*lpNumberOfBytesWritten=nNumberOfBytesToWrite;
		LeaveCriticalSection(&csFakes);
		return bRet;
	}
	LeaveCriticalSection(&csFakes);
	return WriteFile(hFile,lpBuffer,nNumberOfBytesToWrite,lpNumberOfBytesWritten,lpOverlapped);
}

FARPROC HookImport(HMODULE hModule, LPSTR szDllName, LPSTR szFunctionName, FARPROC pHook) {
	BYTE *pBase=(BYTE *)hModule;
	IMAGE_DOS_HEADER *pDosHeader=(IMAGE_DOS_HEADER *)pBase;
	IMAGE_NT_HEADERS *pNtHeaders=(IMAGE_NT_HEADERS *)&pBase[pDosHeader->e_lfanew];
	if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE)
		return false;
	IMAGE_IMPORT_DESCRIPTOR *pImportDescriptor=(IMAGE_IMPORT_DESCRIPTOR *)&pBase[pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress];
	for (; pImportDescriptor->Characteristics != 0; pImportDescriptor++) {
		PSTR szImportDllName=(PSTR)&pBase[pImportDescriptor->Name];
		if (lstrcmpiA(szDllName,szImportDllName)==0) {
			if (pImportDescriptor->FirstThunk && pImportDescriptor->OriginalFirstThunk) {
				IMAGE_THUNK_DATA *pThunk=(IMAGE_THUNK_DATA *)&pBase[pImportDescriptor->FirstThunk];
				IMAGE_THUNK_DATA *pOriginalThunk=(IMAGE_THUNK_DATA *)&pBase[pImportDescriptor->OriginalFirstThunk];
				for (; pOriginalThunk->u1.Function!=NULL; pOriginalThunk++, pThunk++) {
					if (!(pOriginalThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
						IMAGE_IMPORT_BY_NAME *pImportByName=(IMAGE_IMPORT_BY_NAME *)&pBase[pOriginalThunk->u1.AddressOfData];
						if (lstrcmpA((LPCSTR)pImportByName->Name,szFunctionName)==0) {
							MEMORY_BASIC_INFORMATION mbi;
							FARPROC pRetval=0;
							if (VirtualQuery(pThunk,&mbi, sizeof(mbi) ) == sizeof(mbi)) {
								DWORD dwOldProtect;
								if (VirtualProtect(mbi.BaseAddress,mbi.RegionSize,PAGE_EXECUTE_READWRITE,&dwOldProtect)) {
									pRetval=(FARPROC)pThunk->u1.Function;
									pThunk->u1.Function = (DWORD)pHook;
									VirtualProtect(mbi.BaseAddress,mbi.RegionSize,dwOldProtect,&dwOldProtect);
								}
							}
							return pRetval;
						}
					}
				}
			}
		}
	}
	return NULL;
}

bool InitializeHooks(HMODULE hOriginal) {
	InitializeCriticalSection(&csFakes);
	HookImport(hOriginal,"kernel32.dll","CreateFileA",(FARPROC)&HookedCreateFileA);
	HookImport(hOriginal,"kernel32.dll","CloseHandle",(FARPROC)&HookedCloseHandle);
	HookImport(hOriginal,"kernel32.dll","DeviceIoControl",(FARPROC)&HookedDeviceIoControl);
	HookImport(hOriginal,"kernel32.dll","WriteFile",(FARPROC)&HookedWriteFile);
	return true;
}

void DeinitializeHooks() {
	DeleteCriticalSection(&csFakes);
}