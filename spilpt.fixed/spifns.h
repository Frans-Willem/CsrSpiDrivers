#ifndef SPIFNS_H
#define SPIFNS_H

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

int __cdecl spifns_init(); //Return 0 on no error, negative on error
void __cdecl spifns_close();
void __cdecl spifns_getvarlist(SPIVARDEF **ppList, unsigned int *pnCount);
const char * __cdecl spifns_getvar(const char *szName);
int __cdecl spifns_get_version(); //Should return 259
void __cdecl spifns_enumerate_ports(spifns_enumerate_ports_callback pCallback, void *pData);
void __cdecl spifns_chip_select(unsigned int nUnknown);
const char* __cdecl spifns_command(const char *szCmd); //Return 0 on no error, or string on error.
unsigned int __cdecl spifns_get_last_error(unsigned short *pnErrorAddress, const char **szErrorString); //Returns where the error occured, or 0x100 for none
int __cdecl spifns_bluecore_xap_stopped(); //Returns -1 on error, 0 on XAP running, 1 on stopped
int __cdecl spifns_sequence(SPISEQ *pSequence, unsigned int nCount); //Return 0 on no error
void __cdecl spifns_set_debug_callback(spifns_debug_callback pCallback);
#endif//SPIFNS_H
