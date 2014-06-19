#ifndef SPIFNS_H
#define SPIFNS_H

#ifdef DLLEXPORT
# undef DLLEXPORT
#endif

#ifdef __WINE__
# define DLLEXPORT  extern "C"
#else
# define DLLEXPORT  /* Empty */
#endif


#define SPIFNS_VERSION 259

#define SPIERR_NO_ERROR 0x100
#define SPIERR_MALLOC_FAILED 0x101
#define SPIERR_NO_LPT_PORT_SELECTED	0x102
#define SPIERR_READ_FAILED 0x103
#define SPIERR_IOCTL_FAILED 0x104

struct SPIVARDEF {
	const char *szName;
	const char *szDefault;
	int nUnknown;
};

struct SPISEQ {
	enum {
		TYPE_READ=0,
		TYPE_WRITE=1,
		TYPE_SETVAR=2
	} nType;
	union {
		struct {
			unsigned short nAddress;
			unsigned short nLength;
			unsigned short *pnData;
		} rw;
		struct {
			const char *szName;
			const char *szValue;
		} setvar;
	};
};

typedef void (__cdecl *spifns_enumerate_ports_callback)(unsigned int nPortNumber, const char *szPortName, void *pData);
typedef void (__cdecl *spifns_debug_callback)(const char *szDebug);

DLLEXPORT int __cdecl spifns_init(); //Return 0 on no error, negative on error
DLLEXPORT void __cdecl spifns_close();
DLLEXPORT void __cdecl spifns_getvarlist(const SPIVARDEF **ppList, unsigned int *pnCount);
DLLEXPORT const char * __cdecl spifns_getvar(const char *szName);
DLLEXPORT int __cdecl spifns_get_version(); //Should return 259
DLLEXPORT void __cdecl spifns_enumerate_ports(spifns_enumerate_ports_callback pCallback, void *pData);
DLLEXPORT void __cdecl spifns_chip_select(int nUnknown);
DLLEXPORT const char* __cdecl spifns_command(const char *szCmd); //Return 0 on no error, or string on error.
DLLEXPORT unsigned int __cdecl spifns_get_last_error(unsigned short *pnErrorAddress, const char **szErrorString); //Returns where the error occured, or 0x100 for none
DLLEXPORT int __cdecl spifns_bluecore_xap_stopped(); //Returns -1 on error, 0 on XAP running, 1 on stopped
DLLEXPORT int __cdecl spifns_sequence(SPISEQ *pSequence, unsigned int nCount); //Return 0 on no error
DLLEXPORT void __cdecl spifns_set_debug_callback(spifns_debug_callback pCallback);
#endif//SPIFNS_H
