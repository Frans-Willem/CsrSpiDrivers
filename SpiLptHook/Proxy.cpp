#include <Windows.h>
#include <tchar.h>

//#undef _tprintf
//#define _tprintf(...)

FARPROC original_spifns_bluecore_xap_stopped=0;
FARPROC original_spifns_chip_select=0;
FARPROC original_spifns_close=0;
FARPROC original_spifns_command=0;
FARPROC original_spifns_enumerate_ports=0;
FARPROC original_spifns_get_last_error=0;
FARPROC original_spifns_get_version=0;
FARPROC original_spifns_getvar=0;
FARPROC original_spifns_getvarlist=0;
FARPROC original_spifns_init=0;
FARPROC original_spifns_sequence=0;
FARPROC original_spifns_set_debug_callback=0;

void InitializeProxy(HMODULE hOriginal) {
	original_spifns_bluecore_xap_stopped=GetProcAddress(hOriginal,"spifns_bluecore_xap_stopped");
	original_spifns_chip_select=GetProcAddress(hOriginal,"spifns_chip_select");
	original_spifns_close=GetProcAddress(hOriginal,"spifns_close");
	original_spifns_command=GetProcAddress(hOriginal,"spifns_command");
	original_spifns_enumerate_ports=GetProcAddress(hOriginal,"spifns_enumerate_ports");
	original_spifns_get_last_error=GetProcAddress(hOriginal,"spifns_get_last_error");
	original_spifns_get_version=GetProcAddress(hOriginal,"spifns_get_version");
	original_spifns_getvar=GetProcAddress(hOriginal,"spifns_getvar");
	original_spifns_getvarlist=GetProcAddress(hOriginal,"spifns_getvarlist");
	original_spifns_init=GetProcAddress(hOriginal,"spifns_init");
	original_spifns_sequence=GetProcAddress(hOriginal,"spifns_sequence");
	original_spifns_set_debug_callback=GetProcAddress(hOriginal,"spifns_set_debug_callback");
}


//Returns -1 on error, 0 on XAP running, 1 on XAP stopped.
typedef int (__cdecl *type_spifns_bluecore_xap_stopped)();
int __cdecl spifns_bluecore_xap_stopped() {
	_tprintf(TEXT("spifns_bluecore_xap_stopped()...\n"));
	int nRetval=((type_spifns_bluecore_xap_stopped)original_spifns_bluecore_xap_stopped)();
	_tprintf(TEXT("spifns_bluecore_xap_stopped() = %08X\n"),nRetval);
	return nRetval;
}

typedef void (__cdecl *type_spifns_chip_select)(unsigned int nChipSelect);
void __cdecl spifns_chip_select(unsigned int nChipSelect) {
	_tprintf(TEXT("spifns_chip_select(0x%08X)\n"),nChipSelect);
	((type_spifns_chip_select)original_spifns_chip_select)(nChipSelect);
}

typedef void (__cdecl *type_spifns_close)();
//NOTE: Seems to be reference counted
void __cdecl spifns_close() {
	_tprintf(TEXT("spifns_close()\n"));
	((type_spifns_close)original_spifns_close)();
}

typedef const char *(__cdecl *type_spifns_command)(const char *szCmd);
const char * __cdecl spifns_command(const char *szCmd) {
	const char *szRetval=((type_spifns_command)original_spifns_command)(szCmd);
	_tprintf(TEXT("spifns_command(\"%hs\") = \"%hs\"\n"),szCmd,szRetval?szRetval:"(null)");
	return szRetval;
}

void __declspec(naked) spifns_enumerate_ports() {
	__asm {
		jmp original_spifns_enumerate_ports
	}
}

void __declspec(naked) spifns_get_last_error() {
	__asm {
		jmp original_spifns_get_last_error
	}
}

typedef int (__cdecl *type_spifns_get_version)();
int __cdecl spifns_get_version() {
	int nRetval=((type_spifns_get_version)original_spifns_get_version)();
	_tprintf(TEXT("spifns_get_version() = %d;\n"),nRetval);
	return nRetval;
}

typedef const char * (__cdecl *type_spifns_getvar)(const char *szName);
const char * __cdecl spifns_getvar(const char *szName) {
	const char *szRetval=((type_spifns_getvar)original_spifns_getvar)(szName);
	_tprintf(TEXT("spifns_getvar(\"%hs\") = \"%hs\"\n"),szName,szRetval);
	return szRetval;
}

struct SPIFNSVAR {
	const char *szName;
	const char *szDefault;
	int nUnknown;
};
typedef void (__cdecl *type_spifns_getvarlist)(SPIFNSVAR **ppList, unsigned int *pnCount);
void __cdecl spifns_getvarlist(SPIFNSVAR **ppList, unsigned int *pnCount) {
	((type_spifns_getvarlist)original_spifns_getvarlist)(ppList,pnCount);
	_tprintf(TEXT("spifns_getvarlist()\n"));
	/*for (unsigned int i=0; i<*pnCount; i++) {
		_tprintf(TEXT("\t\"%hs\" => \"%hs\" (%d)\n"),(*ppList)[i].szName,(*ppList)[i].szDefault,(*ppList)[i].nUnknown);
	}*/
	return;
}


typedef int (__cdecl *type_spifns_init)();
int __cdecl spifns_init() {
	int nRetval=((type_spifns_init)original_spifns_init)();
	_tprintf(TEXT("spifns_init() = %d\n"),nRetval);
	return nRetval;
}

struct SPIFNS_SEQUENCE {
	unsigned int nType;
	union {
		struct {
			union {
				struct {
					unsigned short a;
					unsigned short b;
				};
				unsigned int ab;
			};
			unsigned int c;
		} unk;
		struct {
			const char *szName;
			const char *szValue;
		} setvar;
		struct {
			unsigned short nAddress;
			unsigned short nLength;
			unsigned short *pDestination;
		} read;
	};
};
typedef int (__cdecl *type_spifns_sequence)(SPIFNS_SEQUENCE *pSeq, unsigned int nCount);
int __cdecl spifns_sequence(SPIFNS_SEQUENCE *pSeq, unsigned int nCount) {
	_tprintf(TEXT("spifns_sequence(%d);\n"),nCount);
	int nRetval=((type_spifns_sequence)original_spifns_sequence)(pSeq,nCount);
	for (unsigned int i=0; i<nCount; i++) {
		switch (pSeq[i].nType) {
		case 0:{
			//_tprintf(TEXT("\tread (0x%04X,0x%04X,0x%08X)\n"),pSeq[i].unk.a,pSeq[i].unk.b,pSeq[i].unk.c);
			_tprintf(TEXT("spifns_sequence_read(0x%04X,0x%04X);\n"),pSeq[i].read.nAddress,pSeq[i].read.nLength);
			_tprintf(TEXT("\t"));
			for (unsigned int j=0; j<pSeq[i].read.nLength; j++) {
				_tprintf(TEXT("%04X "),(pSeq[i].read.pDestination)[j]);
			}
			_tprintf(TEXT("\n"));
			   }break;
		case 1:{
			_tprintf(TEXT("spifns_sequence_write(0x%04X,0x%04X);\n"),pSeq[i].read.nAddress,pSeq[i].read.nLength);
			_tprintf(TEXT("\t"));
			for (unsigned int j=0; j<pSeq[i].read.nLength; j++) {
				_tprintf(TEXT("%04X "),(pSeq[i].read.pDestination)[j]);
			}
			_tprintf(TEXT("\n"));
			   }break;
		case 2:
			_tprintf(TEXT("spifns_sequence_setvar(\"%hs\",\"%hs\");\n"),pSeq[i].setvar.szName,pSeq[i].setvar.szValue);
			break;
		default:
			_tprintf(TEXT("\t%d(%d,%d,%d)\n"),pSeq[i].nType,pSeq[i].unk.a,pSeq[i].unk.b,pSeq[i].unk.c);
			break;
		}
	}
	_tprintf(TEXT("spifns_sequence_result(%d);\n"),nRetval);
	return nRetval;
}

typedef void (__cdecl *spifns_debug_callback)(const char *szDebug);
typedef void (__cdecl *type_spifns_set_debug_callback)(spifns_debug_callback pCallback);

spifns_debug_callback pRealCallback=0;

void __cdecl debug_callback(const char *szDebug) {
	_tprintf(TEXT("debug(\"%hs\");\n"),szDebug);
	if (pRealCallback)
		pRealCallback(szDebug);
}

void __cdecl spifns_set_debug_callback(spifns_debug_callback pCallback) {
	pRealCallback=pCallback;
	((type_spifns_set_debug_callback)original_spifns_set_debug_callback)(debug_callback);
	return;
}
