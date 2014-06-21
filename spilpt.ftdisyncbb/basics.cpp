#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "spifns.h"
#include "basics.h"
#include "spi.h"

#ifdef __WINE__
# define _snprintf snprintf
# define stricmp strcasecmp
# define _stricmp strcasecmp
#endif

/*
 * README:
 * I attempted to reverse engineer spilpt.dll the best I could.
 * However, compiling it with visual studio 2010 did not allow me to yield an identical DLL, and I don't have VS 2005 (original was compiled with this) installed.
 * For now, I'll leave it at this 
 * Feel free to use this to write your own 
*/

#define VARLIST_SPISPORT 0
#define VARLIST_SPIMUL 1
#define VARLIST_SPISHIFTPERIOD 2
#define VARLIST_SPICLOCK 3
#define VARLIST_SPICMDBITS 4
#define VARLIST_SPICMDREADBITS 5
#define VARLIST_SPICMDWRITEBITS 6
#define VARLIST_SPIMAXCLOCK 7

const SPIVARDEF g_pVarList[]={
	{"SPIPORT","1",1},
	{"SPIMUL","0",0},
	{"SPISHIFTPERIOD","0",0},
	{"SPICLOCK","0",0},
	{"SPICMDBITS","0",0},
	{"SPICMDREADBITS","0",0},
	{"SPICMDWRITEBITS","0",0},
	{"SPIMAXCLOCK","1000",0}
};

unsigned int g_nSpiMulChipNum=-1;
int g_nSpiPort=1;
unsigned char g_bCurrentOutput=0x10;
unsigned int g_nSpiShiftPeriod=1;
char g_szMaxClock[16]="1000";
char g_szErrorString[256]="No error";
unsigned int g_nError=SPIERR_NO_ERROR;
//Some gs_check data here. Maybe this indicates that this should be a file split ?
HANDLE g_hDevice=0;
unsigned int g_nRef=0;
int g_nCmdReadBits=0;
int g_nCmdWriteBits=0;
int g_nSpiMul=0;
int g_nSpiMulConfig=0;
unsigned short g_nErrorAddress=0;
spifns_debug_callback g_pDebugCallback=0;

#define BV_MOSI 0x40
#define BV_CSB 0x01
#define BV_MUL 0x02
#define BV_CLK 0x80
#define BV_MISO 0x40 //NOTE: Use status register instead of data

DLLEXPORT void __cdecl spifns_debugout(const char *szFormat, ...);
DLLEXPORT void __cdecl spifns_debugout_readwrite(unsigned short nAddress, char cOperation, unsigned short nLength, unsigned short *pnData);

//RE Check: Completely identical.
DLLEXPORT void __cdecl spifns_getvarlist(const SPIVARDEF **ppList, unsigned int *pnCount) {
	*ppList=g_pVarList;
	*pnCount=sizeof(g_pVarList)/sizeof(*g_pVarList);
}
//RE Check: Opcodes functionally identical
//Original uses 'add g_nRef, 1'
//Compiler makes 'inc g_nRef'
DLLEXPORT int __cdecl spifns_init() {
	g_nRef+=1;
	return 0;
}
//RE Check: Completely identical (one opcode off, jns should be jge, but shouldn't matter. Probably unsigned/signed issues)
DLLEXPORT const char * __cdecl spifns_getvar(const char *szName) {
	if (!szName) {
		return "";
	} else if (_stricmp(szName,"SPIPORT")==0) {
		static char szReturn[20];
		sprintf(szReturn,"%d",g_nSpiPort);
		return szReturn;
	} else if (_stricmp(szName,"SPIMUL")==0) {
		static char szReturn[20];
		sprintf(szReturn,"%d",g_nSpiMulChipNum);
		return szReturn;
	} else if (_stricmp(szName,"SPISHIFTPERIOD")==0) {
		static char szReturn[20];
		sprintf(szReturn,"%d",g_nSpiShiftPeriod);
		return szReturn;
	} else if (_stricmp(szName,"SPICLOCK")==0) {
		static char szReturn[64];
		sprintf(szReturn,"%f",172.41/(double)g_nSpiShiftPeriod);
		return szReturn;
	} else if (_stricmp(szName,"SPIMAXCLOCK")==0) {
		static char szReturn[24];
		sprintf(szReturn,"%s",g_szMaxClock);
		return szReturn;
	} else {
		return "";
	}
}
//RE Check: Completely identical
DLLEXPORT unsigned int __cdecl spifns_get_last_error(unsigned short *pnErrorAddress, const char **pszErrorString) {
	if (pnErrorAddress)
		*pnErrorAddress=g_nErrorAddress;
	if (pszErrorString)
		*pszErrorString=g_szErrorString;
	return g_nError;
}
//RE Check: Completely identical
DLLEXPORT void __cdecl spifns_set_debug_callback(spifns_debug_callback pCallback) {
	g_pDebugCallback=pCallback;
}
//RE Check: Completely identical
DLLEXPORT int __cdecl spifns_get_version() {
	return SPIFNS_VERSION;
}
//
//RE Check: Completely identical
DLLEXPORT HANDLE __cdecl spifns_open_port(int nPort) {
    if (spi_open() < 0)
        return INVALID_HANDLE_VALUE;
    return (HANDLE)1;
}

//RE Check: Fully identical
DLLEXPORT void __cdecl spifns_close_port() {
    if (spi_isopen())
        spi_close();
}
//RE Check: Completely identical
DLLEXPORT void __cdecl spifns_debugout(const char *szFormat, ...) {
	if (g_pDebugCallback) {
		static char szDebugOutput[256];
		va_list args;
		va_start(args,szFormat);
		vsprintf(szDebugOutput,szFormat,args);
		g_pDebugCallback(szDebugOutput);
		va_end(args);
	}
}
//RE Check: Opcodes functionally identical
//Original uses 'sub g_nRef, 1'
//Compiler makes 'dec g_nRef'
DLLEXPORT void __cdecl spifns_close() {
	g_nRef--;
	if (g_nRef==0)
		spifns_close_port();
}
//RE Check: Completely identical
DLLEXPORT void __cdecl spifns_chip_select(int nChip) {
	/*g_bCurrentOutput=g_bCurrentOutput&0xD3|0x10;
	g_nSpiMul=0;
	g_nSpiMulConfig=nChip;
	g_nSpiMulChipNum=nChip;
	spifns_debugout(
		"ChipSelect: %04x mul:%d confog:%d chip_num:%d\n",
		g_bCurrentOutput,
		0,
		nChip,
		nChip);*/
}
//RE Check: Completely identical
DLLEXPORT const char* __cdecl spifns_command(const char *szCmd) {
	if (stricmp(szCmd,"SPISLOWER")==0) {
		//TODO!
		/*if (g_nSpiShiftPeriod<40) {
			//SPI Shift Period seems to be done about 1.5 times, plus 1 to compensate for rounding down (for example in 1)
			spifns_debugout("Delays now %d\n",g_nSpiShiftPeriod=g_nSpiShiftPeriod + (g_nSpiShiftPeriod>>2) + 1); //>>2 => /2 ?
		}*/
	}
	return 0;
}
//RE Check: Opcodes functionally identical
//Original uses 'add esi, 1
//Compiler makes 'inc esi'
DLLEXPORT void __cdecl spifns_enumerate_ports(spifns_enumerate_ports_callback pCallback, void *pData) {
	char szPortName[8];
	for (int nPort=2; nPort<=16; nPort++) {
		HANDLE hDevice;
		if ((hDevice=spifns_open_port(nPort))!=INVALID_HANDLE_VALUE) {
			CloseHandle(hDevice);
			sprintf(szPortName,"COM%d",nPort);
			pCallback(nPort,szPortName,pData);
		}
	}
}
//RE Check: Opcodes functionally identical
//Original passes arguments through eax
//Compiled takes argument on stack. Maybe change it to __fastcall ?
DLLEXPORT void __cdecl spifns_sequence_setvar_spishiftperiod(int nPeriod) {
	//TODO
	spifns_debugout("Delays set to %d\n",g_nSpiShiftPeriod=nPeriod);
}
//RE Check: Opcodes functionally identical, slightly re-ordered (but no impact on result)
//Original passes arguments through eax
//Compiled takes argument on stack. Maybe change it to __fastcall ?
DLLEXPORT bool __cdecl spifns_sequence_setvar_spiport(int nPort) {
	spifns_close_port();
	if (INVALID_HANDLE_VALUE==(g_hDevice=spifns_open_port(nPort)))
		return false;
	g_nSpiPort=nPort;
	//TODO: Do this properly!
	g_nSpiShiftPeriod=1;
	return true;
}
//RE Check: Functionally equivalent, but completely different ASM code 
DLLEXPORT void __cdecl spifns_debugout_readwrite(unsigned short nAddress, char cOperation, unsigned short nLength, unsigned short *pnData) {
	if (g_pDebugCallback) {
		static const char * const pszTable[]={
			"%04X     %c ????\n",
			"%04X     %c %04X\n",
			"%04X-%04X%c %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X %04X %04X ...\n"
		};
		unsigned short bCopy[8];
#define _MIN(x, y)  ( ((x) < (y)) ? (x) : (y) )
		if (pnData)
			memcpy(bCopy,pnData,sizeof(unsigned short)*_MIN(nLength,8));
		else
			memset(bCopy,0,sizeof(bCopy));
		if (nLength<2) {
			spifns_debugout(pszTable[nLength],nAddress,cOperation,bCopy[0]);
		} else {
			spifns_debugout(pszTable[_MIN(nLength, 9)],nAddress,nAddress+nLength-1,cOperation,bCopy[0],bCopy[1],bCopy[2],bCopy[3],bCopy[4],bCopy[5],bCopy[6],bCopy[7]);
		}
#undef _MIN
	}
}

/* Reimplemented using our SPI impl. */
DLLEXPORT int __cdecl spifns_sequence_write(unsigned short nAddress, unsigned short nLength, unsigned short *pnInput) {
    uint8_t outbuf1[] = {
        0x02,                       /* Command: write */
        (uint8_t)(nAddress >> 8),   /* Address high byte */
        (uint8_t)(nAddress & 0xff), /* Address low byte */
    };
    uint8_t *outbuf2;

    /* IO is done in two byte words */
    outbuf2 = (uint8_t *)malloc(nLength * sizeof(unsigned short));
    if (outbuf2 == NULL) {
		static const char szError[]="Allocate buffer failed";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_MALLOC_FAILED;
		return 1;
	}

    if (!spi_isopen()) {
		static const char szError[]="No FTDI device selected";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_NO_LPT_PORT_SELECTED;
		return 1;
	}

    if (spi_xfer_begin() < 0) {
		spifns_debugout_readwrite(nAddress,'r',0,0);
		const char szError[]="Unable to begin transfer";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nErrorAddress=nAddress;
		g_nError=SPIERR_READ_FAILED;
		return 1;
	}

    if (spi_write(outbuf1, sizeof(outbuf1)) < 0) {
		spifns_debugout_readwrite(nAddress,'r',0,0);
		const char szError[]="Unable to start write";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nErrorAddress=nAddress;
		g_nError=SPIERR_READ_FAILED;
		return 1;
	}

    for (unsigned int i=0; i < nLength; i++) {
        outbuf2[i * 2] = *pnInput >> 8;
        outbuf2[i * 2 + 1] = *pnInput & 0xff;
        pnInput++;
    }

    if (spi_write(outbuf2, nLength * sizeof(unsigned short)) < 0) {
        spifns_debugout_readwrite(nAddress,'r',0,0);
        const char szError[]="Unable to write (writing buffer)";
        memcpy(g_szErrorString,szError,sizeof(szError));
        g_nErrorAddress=nAddress;
        g_nError=SPIERR_READ_FAILED;
        return 1;
    }

    free(outbuf2);

    if (spi_xfer_end() < 0) {
		spifns_debugout_readwrite(nAddress,'r',0,0);
		const char szError[]="Unable to end transfer";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nErrorAddress=nAddress;
		g_nError=SPIERR_READ_FAILED;
		return 1;
	}

    return 0;
}

//RE Check: Functionally identical, register choice, calling convention, and some ordering changes.
DLLEXPORT void __cdecl spifns_sequence_setvar_spimul(unsigned int nMul) {
/*	BYTE bNewOutput=g_bCurrentOutput&~BV_CLK;
	if ((g_nSpiMulChipNum=nMul)<=16) {
		//Left side
		if (g_nSpiMulConfig != nMul || g_nSpiMul != 1)
			g_nSpiShiftPeriod=1;
		g_nSpiMulConfig=nMul;
		g_nSpiMul=1;
		bNewOutput=(bNewOutput&(BV_CLK|BV_MOSI|BV_CSB))|((BYTE)nMul<<1);
	} else {
		//Right side
		//loc_10001995
		if (g_nSpiMulConfig != 0 || g_nSpiMul!=0)
			g_nSpiShiftPeriod=1;
		g_nSpiMul=0;
		g_nSpiMulConfig=0;
		bNewOutput&=BV_CLK|BV_MOSI|BV_CSB;
	}
	g_bCurrentOutput=bNewOutput;
	SPITransfer(0,2,0,0);
	LARGE_INTEGER liFrequency,liPCStart,liPCEnd;
	QueryPerformanceFrequency(&liFrequency);
	QueryPerformanceCounter(&liPCStart);
	do {
		Sleep(0);
		QueryPerformanceCounter(&liPCEnd);
	} while (1000 * (liPCEnd.QuadPart - liPCStart.QuadPart) / liFrequency.QuadPart < 5);
	spifns_debugout("MulitplexSelect: %04x mul:%d config:%d chip_num:%d\n",g_bCurrentOutput,g_nSpiMul,g_nSpiMulConfig,g_nSpiMulChipNum);*/
}
//RE Check: Functionally identical, register choice, calling convention, stack size, and some ordering changes.
DLLEXPORT int __cdecl spifns_sequence_setvar(const char *szName, const char *szValue) {
	if (szName==0)
		return 1;
	if (szValue==0)
		return 1;
	long nValue=strtol(szValue,0,0);
	for (unsigned int i=0; i<(sizeof(g_pVarList)/sizeof(*g_pVarList)); i++) {
		if (stricmp(szName,g_pVarList[i].szName)==0) {
			switch (i) {
			case VARLIST_SPISPORT:{
				if (!spifns_sequence_setvar_spiport(nValue)) {
					const char szError[]="Couldn't find LPT port";
					memcpy(g_szErrorString,szError,sizeof(szError));
					return 1;
				}
								  }break;
			case VARLIST_SPIMUL:{
				spifns_sequence_setvar_spimul(nValue);
								}break;
			case VARLIST_SPISHIFTPERIOD:{
				spifns_sequence_setvar_spishiftperiod(nValue);
										}break;
			case VARLIST_SPICLOCK:{
				double dblValue=strtod(szValue,0);
				static const double dblNull=0.0;
				if (dblValue==dblNull)
					return 1; //ERROR!
				int nShiftPeriod=(172.41/dblValue)+0.5;
				if (nShiftPeriod<1) nShiftPeriod=1;
				spifns_sequence_setvar_spishiftperiod(nShiftPeriod);
								  }break;
			case VARLIST_SPICMDBITS:
			case VARLIST_SPICMDREADBITS:
			case VARLIST_SPICMDWRITEBITS:{
				if (i!=VARLIST_SPICMDREADBITS)
					g_nCmdWriteBits=nValue;
				if (i!=VARLIST_SPICMDWRITEBITS)
					g_nCmdReadBits=nValue;
										 }break;
			case VARLIST_SPIMAXCLOCK:{
				double fValue=atof(szValue);
				if (fValue<1.0)
					fValue=1.0;
				_snprintf(g_szMaxClock,10,"%f",fValue); //I have no idea why they only allow 11 characters to be output everything indicates the stack could allocate 16 :/
									 }break;
			}
		}
	}
	return 0;
}

/* Reimplemented using our SPI impl. */
DLLEXPORT int __cdecl spifns_sequence_read(unsigned short nAddress, unsigned short nLength, unsigned short *pnOutput) {
    uint8_t outbuf[] = {
        3,                          /* Command: read */
        (uint8_t)(nAddress >> 8),   /* Address high byte */
        (uint8_t)(nAddress & 0xff), /* Address low byte */
    };
    uint8_t *inbuf;
    int inbufsize;

    /* IO is done in two byte words */
    inbufsize = nLength * sizeof(unsigned short);
    if (inbufsize < 2)
        inbufsize = 2;
    inbuf = (uint8_t *)malloc(inbufsize);
    if (inbuf == NULL) {
		static const char szError[]="Allocate buffer failed";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_MALLOC_FAILED;
		return 1;
	}

    if (!spi_isopen()) {
		static const char szError[]="No FTDI device selected";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_NO_LPT_PORT_SELECTED;
		return 1;
	}

    if (spi_xfer_begin() < 0) {
		spifns_debugout_readwrite(nAddress,'r',0,0);
		const char szError[]="Unable to begin transfer";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nErrorAddress=nAddress;
		g_nError=SPIERR_READ_FAILED;
		return 1;
	}

    if (spi_write(outbuf, sizeof(outbuf)) < 0) {
		spifns_debugout_readwrite(nAddress,'r',0,0);
		const char szError[]="Unable to start read";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nErrorAddress=nAddress;
		g_nError=SPIERR_READ_FAILED;
		return 1;
	}

    if (spi_read(inbuf, 2) < 0 || inbuf[0] != outbuf[0] || inbuf[1] != outbuf[1]) {
        spifns_debugout_readwrite(nAddress,'r',0,0);
        const char szError[]="Unable to start read (getting control data)";
        memcpy(g_szErrorString,szError,sizeof(szError));
        g_nErrorAddress=nAddress;
        g_nError=SPIERR_READ_FAILED;
        return 1;
    }

    if (spi_read(inbuf, nLength * sizeof(unsigned short)) < 0) {
        spifns_debugout_readwrite(nAddress,'r',0,0);
        const char szError[]="Unable to read (reading buffer)";
        memcpy(g_szErrorString,szError,sizeof(szError));
        g_nErrorAddress=nAddress;
        g_nError=SPIERR_READ_FAILED;
        return 1;
    }

    for (unsigned int i=0; i<nLength; i++)
        *(pnOutput++)=(inbuf[(i*2)]<<8)|inbuf[(i*2)+1];

    free(inbuf);

    if (spi_xfer_end() < 0) {
		spifns_debugout_readwrite(nAddress,'r',0,0);
		const char szError[]="Unable to end transfer";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nErrorAddress=nAddress;
		g_nError=SPIERR_READ_FAILED;
		return 1;
	}

	return 0;
}
//RE Check: Functionally identical, can't get the ASM code to match.
DLLEXPORT int __cdecl spifns_sequence(SPISEQ *pSequence, unsigned int nCount) {
	int nRetval=0;
	while (nCount--) {
		switch (pSequence->nType) {
		case SPISEQ::TYPE_READ:{
			if (spifns_sequence_read(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
							   }break;
		case SPISEQ::TYPE_WRITE:{
			if (spifns_sequence_write(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
								}break;
		case SPISEQ::TYPE_SETVAR:{
			if (spifns_sequence_setvar(pSequence->setvar.szName,pSequence->setvar.szValue)==1)
				nRetval=1;
								 }break;

		}
		pSequence++;
	}
	return nRetval;
}

/* Reimplemented using our own SPI driver */
DLLEXPORT int __cdecl spifns_bluecore_xap_stopped() {
    /* XXX: describe what is going on */
    uint8_t xferbuf[] = {
        3,      /* Command: read */
        0xff,   /* Address high byte */
        0x9a,   /* Address low byte */
    };
    uint8_t inbuf[2];

    if (spi_xfer_begin() < 0)
        return -1;
    if (spi_xfer(xferbuf, sizeof(xferbuf)) < 0)
        return -1;
    if (spi_read(inbuf, sizeof(inbuf)) < 0)
        return -1;
    if (spi_xfer_end() < 0)
        return -1;

    if (inbuf[0] != 3 || inbuf[1] != 0xff) {
        /* No chip present or not responding correctly, no way to find out. */
        return -1;
    }

    /* Check the response to read command */
    if (xferbuf[0])
        return 1;
    return 0;
}
