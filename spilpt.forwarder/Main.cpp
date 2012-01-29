#include <Windows.h>
#include "spifns.h"
#include <stdio.h>
#include <tchar.h>

HINSTANCE g_hInstance=0;
HMODULE g_hForward=0;
bool g_bDebug=false;

typedef int (__cdecl *type_spifns_init)(); //Return 0 on no error, negative on error
typedef void (__cdecl *type_spifns_close)();
typedef void (__cdecl *type_spifns_getvarlist)(SPIVARDEF **ppList, unsigned int *pnCount);
typedef const char * (__cdecl *type_spifns_getvar)(const char *szName);
typedef int (__cdecl *type_spifns_get_version)(); //Should return 259
typedef void (__cdecl *type_spifns_enumerate_ports)(spifns_enumerate_ports_callback pCallback, void *pData);
typedef void (__cdecl *type_spifns_chip_select)(unsigned int nUnknown);
typedef const char* (__cdecl *type_spifns_command)(const char *szCmd); //Return 0 on no error, or string on error.
typedef unsigned int (__cdecl *type_spifns_get_last_error)(unsigned short *pnErrorAddress, const char **szErrorString); //Returns where the error occured, or 0x100 for none
typedef int (__cdecl *type_spifns_bluecore_xap_stopped)(); //Returns -1 on error, 0 on XAP running, 1 on stopped
typedef int (__cdecl *type_spifns_sequence)(SPISEQ *pSequence, unsigned int nCount); //Return 0 on no error
typedef void (__cdecl *type_spifns_set_debug_callback)(spifns_debug_callback pCallback);

type_spifns_init forward_spifns_init=0;
type_spifns_close forward_spifns_close=0;
type_spifns_getvarlist forward_spifns_getvarlist=0;
type_spifns_getvar forward_spifns_getvar=0;
type_spifns_get_version forward_spifns_get_version=0;
type_spifns_enumerate_ports forward_spifns_enumerate_ports=0;
type_spifns_chip_select forward_spifns_chip_select=0;
type_spifns_command forward_spifns_command=0;
type_spifns_get_last_error forward_spifns_get_last_error=0;
type_spifns_bluecore_xap_stopped forward_spifns_bluecore_xap_stopped=0;
type_spifns_sequence forward_spifns_sequence=0;
type_spifns_set_debug_callback forward_spifns_set_debug_callback=0;

BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:{
		g_hInstance=hinstDll;
		TCHAR szSpiFwd[MAX_PATH];
		TCHAR szSpiFwdDebug[1024];
		if (!GetEnvironmentVariable(TEXT("SPIFWD"),szSpiFwd,sizeof(szSpiFwd)/sizeof(*szSpiFwd)))
			lstrcpy(szSpiFwd,TEXT(""));
		if (!GetEnvironmentVariable(TEXT("SPIFWD.DEBUG"),szSpiFwdDebug,sizeof(szSpiFwdDebug)/sizeof(*szSpiFwdDebug)))
			lstrcpy(szSpiFwdDebug,TEXT(""));
		g_bDebug=(lstrcmpi(szSpiFwdDebug,TEXT("true"))==0 || lstrcmpi(szSpiFwdDebug,TEXT("1"))==0);
		g_hForward=LoadLibrary(szSpiFwd);
		if (!g_hForward) {
			_tprintf(TEXT("spilpt.forwarded could not load '%s' (SPIFWD environment variable)\n"),szSpiFwd);
			return FALSE;
		}
		if (g_bDebug)
			_tprintf(TEXT("spilpt.forwarded loaded\n"));
		forward_spifns_init=(type_spifns_init)GetProcAddress(g_hForward,"spifns_init");
		forward_spifns_close=(type_spifns_close)GetProcAddress(g_hForward,"spifns_close");
		forward_spifns_getvarlist=(type_spifns_getvarlist)GetProcAddress(g_hForward,"spifns_getvarlist");
		forward_spifns_getvar=(type_spifns_getvar)GetProcAddress(g_hForward,"spifns_getvar");
		forward_spifns_get_version=(type_spifns_get_version)GetProcAddress(g_hForward,"spifns_get_version");
		forward_spifns_enumerate_ports=(type_spifns_enumerate_ports)GetProcAddress(g_hForward,"spifns_enumerate_ports");
		forward_spifns_chip_select=(type_spifns_chip_select)GetProcAddress(g_hForward,"spifns_chip_select");
		forward_spifns_command=(type_spifns_command)GetProcAddress(g_hForward,"spifns_command");
		forward_spifns_get_last_error=(type_spifns_get_last_error)GetProcAddress(g_hForward,"spifns_get_last_error");
		forward_spifns_bluecore_xap_stopped=(type_spifns_bluecore_xap_stopped)GetProcAddress(g_hForward,"spifns_bluecore_xap_stopped");
		forward_spifns_sequence=(type_spifns_sequence)GetProcAddress(g_hForward,"spifns_sequence");
		forward_spifns_set_debug_callback=(type_spifns_set_debug_callback)GetProcAddress(g_hForward,"spifns_set_debug_callback");
							}break;
	case DLL_PROCESS_DETACH:{
		if (g_bDebug)
			_tprintf(TEXT("spilpt.forwarded unloading\n"));
		FreeLibrary(g_hForward);
							}break;
	}
	return TRUE;
}

int __cdecl spifns_init() {
	int nRetval=-1;
	if (forward_spifns_init)
		nRetval=forward_spifns_init();
	if (g_bDebug)
		_tprintf(TEXT("spifns_init(); returned %d\n"),nRetval);
	return nRetval;
}
void __cdecl spifns_close() {
	if (g_bDebug)
		_tprintf(TEXT("spifns_close();\n"));
	if (forward_spifns_close)
		forward_spifns_close();
}
void __cdecl spifns_getvarlist(SPIVARDEF **ppList, unsigned int *pnCount) {
	if (ppList)
		*ppList=0;
	if (*pnCount)
		*pnCount=0;
	if (forward_spifns_getvarlist)
		forward_spifns_getvarlist(ppList,pnCount);
	if (g_bDebug) {
		_tprintf(TEXT("spifns_getvarlist(&pList,&nCount); returned %d entries:\n"),pnCount?*pnCount:0);
		if (ppList && *ppList && pnCount) {
			for (int i=0; i<*pnCount; i++)
				_tprintf(TEXT("\t\"%hs\", \"%hs\", %d\n"),(*ppList)[i].szName,(*ppList)[i].szDefault,(*ppList)[i].nUnknown);
		} else {
			_tprintf(TEXT("\tNo list returned\n"));
		}
	}
}
const char * __cdecl spifns_getvar(const char *szName) {
	const char *szRetval=0;
	if (forward_spifns_getvar)
		szRetval=forward_spifns_getvar(szName);
	if (g_bDebug)
		_tprintf(TEXT("spifns_getvar(\"%hs\"); returned \"%hs\"\n"),szName?szName:"(null)",szRetval?szRetval:"(null)");
	return szRetval;
}
int __cdecl spifns_get_version() {
	int nRetval=0;
	if (forward_spifns_get_version)
		nRetval=forward_spifns_get_version();
	if (g_bDebug)
		_tprintf(TEXT("spifns_get_version(); returned %d\n"),nRetval);
	return nRetval;
}
struct spifns_port_enumerator_data {
	spifns_enumerate_ports_callback pForward;
	void *pForwardData;
};
void __cdecl spifns_port_enumerator(unsigned int nPort, const char *szPortName, void *pData) {
	if (g_bDebug)
		_tprintf(TEXT("\tPort: %d \"%hs\"\n"),nPort,szPortName?szPortName:"");
	spifns_port_enumerator_data *pCastData=(spifns_port_enumerator_data *)pData;
	pCastData->pForward(nPort,szPortName,pCastData->pForwardData);
}
void __cdecl spifns_enumerate_ports(spifns_enumerate_ports_callback pCallback, void *pData) {
	spifns_port_enumerator_data d;
	d.pForward=pCallback;
	d.pForwardData=pData;
	if (g_bDebug)
		_tprintf(TEXT("spifns_enumerator_ports(0x%08X,0x%08X);\n"),pCallback,pData);
	if (forward_spifns_enumerate_ports)
		forward_spifns_enumerate_ports(spifns_port_enumerator,(void*)&d);
}
void __cdecl spifns_chip_select(unsigned int nUnknown) {
	if (g_bDebug)
		_tprintf(TEXT("spifns_chip_select(%d);\n"),nUnknown);
	if (forward_spifns_chip_select)
		forward_spifns_chip_select(nUnknown);
}
const char* __cdecl spifns_command(const char *szCmd) {
	const char *szRetval=0;
	if (forward_spifns_command)
		szRetval=forward_spifns_command(szCmd);
	if (g_bDebug)
		_tprintf(TEXT("spifns_command(\"%hs\"); returned \"%hs\"\n"),szCmd?szCmd:"(null)",szRetval?szRetval:"(null)");
	return szRetval;
}
unsigned int __cdecl spifns_get_last_error(unsigned short *pnErrorAddress, const char **szErrorString) {
	unsigned int nError=SPIERR_NO_ERROR;
	if (forward_spifns_get_last_error)
		nError=forward_spifns_get_last_error(pnErrorAddress,szErrorString);
	if (g_bDebug) {
		char szErrorCode[1024];
		switch (nError) {
			case SPIERR_NO_ERROR: strcpy(szErrorCode,"SPIERR_NO_ERROR"); break;
			case SPIERR_MALLOC_FAILED: strcpy(szErrorCode,"SPIERR_MALLOC_FAILED"); break;
			case SPIERR_NO_LPT_PORT_SELECTED: strcpy(szErrorCode,"SPIERR_NO_LPT_PORT_SELECTED"); break;
			case SPIERR_READ_FAILED: strcpy(szErrorCode,"SPIERR_READ_FAILED"); break;
			case SPIERR_IOCTL_FAILED: strcpy(szErrorCode,"SPIERR_IOCTL_FAILED"); break;
			default: sprintf(szErrorCode,"0x%08X",nError); break;
		}
		_tprintf(TEXT("spifns_get_last_error(...,...); returned %hs, nAddress=0x%04X, \"%hs\"\n"),szErrorCode,pnErrorAddress?*pnErrorAddress:0,(szErrorString && *szErrorString)?*szErrorString:"(null)");
	}
	return nError;
}
int __cdecl spifns_bluecore_xap_stopped() {
	int nRetval=-1;
	if (forward_spifns_bluecore_xap_stopped)
		nRetval=forward_spifns_bluecore_xap_stopped();
	if (g_bDebug)
		_tprintf(TEXT("spifns_bluecore_xap_stopped(); returned %d\n"),nRetval);
	return nRetval;
}
int __cdecl spifns_sequence(SPISEQ *pSequence, unsigned int nCount) {
	int nRetval=1;
	if (g_bDebug) {
		_tprintf(TEXT("spifns_sequence({\n"));
		for (unsigned int i=0; i<nCount; i++) {
			switch (pSequence[i].nType) {
			case SPISEQ::TYPE_SETVAR:
				_tprintf(TEXT("\t{TYPE_SETVAR,\"%hs\",\"%hs\"},\n"),pSequence[i].setvar.szName?pSequence[i].setvar.szName:"(null)",pSequence[i].setvar.szValue?pSequence[i].setvar.szValue:"(null)");
				break;
			case SPISEQ::TYPE_READ:
				_tprintf(TEXT("\t{TYPE_READ,0x%04X,0x%04X},\n"),pSequence[i].rw.nAddress,pSequence[i].rw.nLength);
				break;
			case SPISEQ::TYPE_WRITE:{
				_tprintf(TEXT("\t{TYPE_WRITE,0x%04X,0x%04X,{"),pSequence[i].rw.nAddress,pSequence[i].rw.nLength);
				if (pSequence[i].rw.pnData==NULL) {
					_tprintf(TEXT("NULL}},\n"));
				} else {
					for (unsigned int j=0; j<pSequence[j].rw.nLength; j++) {
						if ((j%8)==0)
							_tprintf(TEXT("\n\t\t"));
						_tprintf(TEXT("0x%04X "),pSequence[i].rw.pnData[j]);
					}
					_tprintf(TEXT("\n\t} },\n"));
				}
									}break;
			default:
				_tprintf(TEXT("\t{%d}\n"),pSequence[i].nType);
			}
		}
		_tprintf(TEXT("},%d);\n"),nCount);
	}
	if (forward_spifns_sequence)
		nRetval=forward_spifns_sequence(pSequence,nCount);
	if (g_bDebug) {
		_tprintf(TEXT("return value: %d\n"),nRetval);
		if (nRetval==0) {
			for (unsigned int i=0; i<nCount; i++) {
				switch (pSequence[i].nType) {
				case SPISEQ::TYPE_READ:{
					_tprintf(TEXT("\tRead result %d:"),i);
					if (pSequence[i].rw.pnData) {
						for (unsigned int j=0; j<pSequence[i].rw.nLength; j++) {
							if ((j%8)==0)
								_tprintf(TEXT("\n\t\t"));
							_tprintf(TEXT("0x%04X "),pSequence[i].rw.pnData[j]);
						}
						_tprintf(TEXT("\n"));
					} else {
						_tprintf(TEXT(" NULL\n"));
					}
									   }break;
				}
			}
		}
	}
	return nRetval;
}


spifns_debug_callback pRealDebugCallback=0;
void __cdecl spifns_hooked_debug_callback(const char *szDebug) {
	if (g_bDebug)
		_tprintf(TEXT("spifns_debug_callback(\"%hs\");\n"),szDebug);
	if (pRealDebugCallback)
		pRealDebugCallback(szDebug);
}
void __cdecl spifns_set_debug_callback(spifns_debug_callback pCallback) {
	pRealDebugCallback=pCallback;
	if (forward_spifns_set_debug_callback)
		forward_spifns_set_debug_callback(pCallback?spifns_hooked_debug_callback:0);
}