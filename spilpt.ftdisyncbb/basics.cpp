#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef __WINE__
#include "wine/debug.h"
#endif

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

#ifdef __WINE__
WINE_DEFAULT_DEBUG_CHANNEL(spilpt);
#else
#define WINE_TRACE(args...)     do { } while(0)
#define WINE_WARN(args...)      do { } while(0)
#define WINE_ERR(args...)       do { } while(0)
#endif

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
    WINE_TRACE("\n");

	*ppList=g_pVarList;
	*pnCount=sizeof(g_pVarList)/sizeof(*g_pVarList);
}
//RE Check: Opcodes functionally identical
//Original uses 'add g_nRef, 1'
//Compiler makes 'inc g_nRef'
DLLEXPORT int __cdecl spifns_init() {
    WINE_TRACE("\n");
	g_nRef+=1;
	return 0;
}
//RE Check: Completely identical (one opcode off, jns should be jge, but shouldn't matter. Probably unsigned/signed issues)
DLLEXPORT const char * __cdecl spifns_getvar(const char *szName) {
    WINE_TRACE("(%s)\n", szName);
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
    WINE_TRACE("\n");
	if (pnErrorAddress)
		*pnErrorAddress=g_nErrorAddress;
	if (pszErrorString)
		*pszErrorString=g_szErrorString;
	return g_nError;
}

DLLEXPORT void spifns_clear_last_error(void)
{
    WINE_TRACE("\n");
	static const char szError[]="No error";
	memcpy(g_szErrorString,szError,sizeof(szError));
    g_nErrorAddress=0;
    g_nError=SPIERR_NO_ERROR;
}

//RE Check: Completely identical
DLLEXPORT void __cdecl spifns_set_debug_callback(spifns_debug_callback pCallback) {
    WINE_TRACE("\n");
	g_pDebugCallback=pCallback;
    spi_set_error_cb((spi_error_cb)pCallback);
}
//RE Check: Completely identical
DLLEXPORT int __cdecl spifns_get_version() {
    WINE_TRACE("return: 0x%02x\n", SPIFNS_VERSION);
	return SPIFNS_VERSION;
}
//
//RE Check: Completely identical
DLLEXPORT HANDLE __cdecl spifns_open_port(int nPort) {
    int rc;

    WINE_TRACE("(%d)\n", nPort);

    rc = spi_open();

    WINE_TRACE("spi_open() returns %d\n", rc);
    if (spi_open() < 0)
        return INVALID_HANDLE_VALUE;
    return (HANDLE)&spifns_open_port;
}

//RE Check: Fully identical
DLLEXPORT void __cdecl spifns_close_port() {
    WINE_TRACE("\n");
    if (spi_isopen()) {
        spi_led(SPI_LED_OFF);
        spi_close();
    }
}
//RE Check: Completely identical
DLLEXPORT void __cdecl spifns_debugout(const char *szFormat, ...) {
    WINE_TRACE("\n");
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
    WINE_TRACE("\n");
	g_nRef--;
	if (g_nRef==0)
		spifns_close_port();
}
//RE Check: Completely identical
DLLEXPORT void __cdecl spifns_chip_select(int nChip) {
    WINE_TRACE("(%d)\n", nChip);
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
    WINE_TRACE("(%s)\n", szCmd);
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
    WINE_TRACE("\n");
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
    WINE_TRACE("(%d)\n", nPeriod);
	//TODO
	spifns_debugout("Delays set to %d\n",g_nSpiShiftPeriod=nPeriod);
}
//RE Check: Opcodes functionally identical, slightly re-ordered (but no impact on result)
//Original passes arguments through eax
//Compiled takes argument on stack. Maybe change it to __fastcall ?
DLLEXPORT bool __cdecl spifns_sequence_setvar_spiport(int nPort) {
    WINE_TRACE("(%d)\n", nPort);
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
    WINE_TRACE("(0x%04x, '%c', %d, buf)\n", nAddress, cOperation, nLength);
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
            WINE_TRACE(pszTable[nLength],nAddress,cOperation,bCopy[0]);
			spifns_debugout(pszTable[nLength],nAddress,cOperation,bCopy[0]);
		} else {
            WINE_TRACE(pszTable[_MIN(nLength, 9)],nAddress,nAddress+nLength-1,cOperation,bCopy[0],bCopy[1],bCopy[2],bCopy[3],bCopy[4],bCopy[5],bCopy[6],bCopy[7]);
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
    uint16_t *outbuf2;

    WINE_TRACE("(0x%04x, %d, %p)\n", nAddress, nLength, pnInput);

#define _ERR_RETURN(n, s) do { \
        g_nError = (n); \
        lstrcpynA(g_szErrorString, (s), sizeof(g_szErrorString)); \
        goto error; \
    } while (0)

    outbuf2 = NULL;

    spi_led(SPI_LED_WRITE);

    if (!spi_isopen())
        _ERR_RETURN(SPIERR_NO_LPT_PORT_SELECTED, "No FTDI device selected");

    /* IO is done in two byte words */
    outbuf2 = (uint16_t *)malloc(nLength * sizeof(uint16_t));
    if (outbuf2 == NULL)
        _ERR_RETURN(SPIERR_MALLOC_FAILED, "Allocate buffer failed");

    memcpy(outbuf2, pnInput, nLength * sizeof(unsigned short));

    if (spi_xfer_begin() < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to begin transfer");

    if (spi_xfer_8(SPI_XFER_WRITE, outbuf1, 3) < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to start write");

    if (spi_xfer_16(SPI_XFER_WRITE, outbuf2, nLength) < 0) {
        spifns_debugout_readwrite(nAddress,'w',nLength, outbuf2);
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to write (writing buffer)");
    }

    if (spi_xfer_end() < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to end transfer");

    free(outbuf2);
    outbuf2 = NULL;

    return 0;

#undef _ERR_RETURN

error:
    if (outbuf2)
        free(outbuf2);
    if (g_nError != SPIERR_NO_ERROR) {
        g_nErrorAddress=nAddress;
        WINE_ERR("%s\n", g_szErrorString);
    }
    return 1;
}

//RE Check: Functionally identical, register choice, calling convention, and some ordering changes.
DLLEXPORT void __cdecl spifns_sequence_setvar_spimul(unsigned int nMul) {
    WINE_TRACE("(%d)\n", nMul);
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
    WINE_TRACE("(%s, %s)\n", szName, szValue);
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
    uint8_t inbuf1[2];
    uint16_t *inbuf2;

    WINE_TRACE("(0x%02x, %d, %p)\n", nAddress, nLength, pnOutput);

#define _ERR_RETURN(n, s) do { \
        g_nError = (n); \
        lstrcpynA(g_szErrorString, (s), sizeof(g_szErrorString)); \
        goto error; \
    } while (0)

    inbuf2 = NULL;

    spi_led(SPI_LED_READ);

    if (!spi_isopen())
        _ERR_RETURN(SPIERR_NO_LPT_PORT_SELECTED, "No FTDI device selected");

    /* IO is done in two byte words */
    inbuf2 = (uint16_t *)malloc(nLength * sizeof(uint16_t));
    if (inbuf2 == NULL)
        _ERR_RETURN(SPIERR_MALLOC_FAILED, "Allocate buffer failed");

    if (spi_xfer_begin() < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to begin transfer");

    if (spi_xfer_8(SPI_XFER_WRITE, outbuf, 3) < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to start read");

    if (spi_xfer_8(SPI_XFER_READ, inbuf1, 2) < 0) {
        _ERR_RETURN(SPIERR_READ_FAILED,
                "Unable to start read (getting control data)");
    }

    if (inbuf1[0] != 3 || inbuf1[1] != (nAddress >> 8)) {
        _ERR_RETURN(SPIERR_READ_FAILED,
                "Unable to start read (invalid control data)");
    }

    if (spi_xfer_16(SPI_XFER_READ, inbuf2, nLength) < 0) {
        spifns_debugout_readwrite(nAddress,'r', nLength, inbuf2);
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to read (reading buffer)");
    }

    if (spi_xfer_end() < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to end transfer");

    memcpy(pnOutput, inbuf2, nLength * sizeof(unsigned short));

    free(inbuf2);
    inbuf2 = NULL;

	return 0;

#undef _ERR_RETURN

error:
    if (inbuf2)
        free(inbuf2);
    if (g_nError != SPIERR_NO_ERROR) {
        g_nErrorAddress=nAddress;
        WINE_ERR("%s\n", g_szErrorString);
    }
    return 1;
}
//RE Check: Functionally identical, can't get the ASM code to match.
DLLEXPORT int __cdecl spifns_sequence(SPISEQ *pSequence, unsigned int nCount) {
	int nRetval=0;

    WINE_TRACE("(%p, %d) type=%d\n", pSequence, nCount, pSequence->nType);

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
    /* Read chip version */
    uint8_t xferbuf[] = {
        3,      /* Command: read */
        GBL_CHIP_VERSION_GEN1_ADDR >> 8,   /* Address high byte */
        GBL_CHIP_VERSION_GEN1_ADDR & 0xff,   /* Address low byte */
    };
    uint8_t inbuf[2];

    WINE_TRACE("\n");

    if (spi_xfer_begin() < 0)
        return SPIFNS_XAP_NO_REPLY;
    if (spi_xfer_8(SPI_XFER_READ | SPI_XFER_WRITE, xferbuf, 3) < 0)
        return SPIFNS_XAP_NO_REPLY;
    if (spi_xfer_8(SPI_XFER_READ, inbuf, 2) < 0)
        return SPIFNS_XAP_NO_REPLY;
    if (spi_xfer_end() < 0)
        return SPIFNS_XAP_NO_REPLY;

    if (inbuf[0] != 3 || inbuf[1] != 0xff) {
        /* No chip present or not responding correctly, no way to find out. */
        return SPIFNS_XAP_NO_REPLY;
    }

    /* Check the response to read command */
    if (xferbuf[0])
        return SPIFNS_XAP_STOPPED;
    return SPIFNS_XAP_RUNNING;
}

#if SPIFNS_API == SPIFNS_API_1_4
/* This is a limited implementation of CSR SPI API 1.4. It supports only 1
 * stream and does not support all of the features. */

DLLEXPORT int __cdecl spifns_stream_init(spifns_stream_t *p_stream)
{
    WINE_TRACE("(%p)\n", p_stream);
    int rc;
    
    rc = spifns_init();
    if (rc == 0)
        *p_stream = (spifns_stream_t)0;
    
    return rc;
}

DLLEXPORT void __cdecl spifns_stream_close(spifns_stream_t stream)
{
    WINE_TRACE("(%d)\n", stream);
    spifns_close();
}

DLLEXPORT unsigned int __cdecl spifns_count_streams(void)
{
    WINE_TRACE("()\n");
    return g_nRef ? 1 : 0;
}

DLLEXPORT int __cdecl spifns_stream_sequence(spifns_stream_t stream, SPISEQ_1_4 *pSequence, int nCount)
{
	int nRetval=0;

    WINE_TRACE("(%d, %p, %d) type=%d\n", stream, pSequence, nCount, pSequence->nType);

	while (nCount--) {
		switch (pSequence->nType) {
		case SPISEQ_1_4::TYPE_READ:
			if (spifns_sequence_read(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
			break;
		case SPISEQ_1_4::TYPE_WRITE:
			if (spifns_sequence_write(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
			break;
		case SPISEQ_1_4::TYPE_SETVAR:
			if (spifns_sequence_setvar(pSequence->setvar.szName,pSequence->setvar.szValue)==1)
				nRetval=1;
			break;
        default:
            g_nError = SPIFNS_ERROR_INVALID_PARAMETER;
            snprintf(g_szErrorString, sizeof(g_szErrorString),
                    "sequence command %d not implemented", pSequence->nType);
            nRetval = 1;
		}
		pSequence++;
	}
	return nRetval;
}

DLLEXPORT const char* __cdecl spifns_stream_command(spifns_stream_t stream, const char *command)
{
    WINE_TRACE("(%d, %s)\n", stream, command);
    return spifns_command(command);
}

DLLEXPORT const char* __cdecl spifns_stream_getvar(spifns_stream_t stream, const char *var)
{
    WINE_TRACE("(%d, %s)\n", stream, var);
    return spifns_getvar(var);
}

DLLEXPORT void __cdecl spifns_stream_chip_select(spifns_stream_t stream, int which)
{
    WINE_TRACE("(%d, %d)\n", stream, which);
    spifns_chip_select(which);
}

DLLEXPORT int __cdecl spifns_stream_bluecore_xap_stopped(spifns_stream_t stream)
{
    WINE_TRACE("(%d)\n", stream);
    return spifns_bluecore_xap_stopped();
}

/* returns the last error code, and if a pointer is passed in, the problematic
 * address.*/
/* get_last_error and clear_last_error both deal with the error that occurred
 * in the current thread */
DLLEXPORT int __cdecl spifns_get_last_error32(uint32_t *addr, const char ** buf)
{
    unsigned short saddr;
    int rc;

    WINE_TRACE("(%p, %p)\n", addr, buf);

    rc = spifns_get_last_error(&saddr, buf);
    if (addr)
        *addr = saddr;
    return rc;
}

DLLEXPORT void __cdecl spifns_stream_set_debug_callback(spifns_stream_t stream, spifns_debug_callback fn, void *pvcontext)
{
    WINE_TRACE("(%d, %p, %p)\n", stream, fn, pvcontext);
    spifns_set_debug_callback(fn);
}

DLLEXPORT int __cdecl spifns_stream_get_device_id(spifns_stream_t stream, char *buf, size_t length)
{
    WINE_TRACE("(%d, %p, %u)\n", stream, buf, length);
    snprintf(buf, length, "FTDISyncBB");
    return 0;
}

DLLEXPORT int __cdecl spifns_stream_lock(spifns_stream_t stream, uint32_t timeout)
{
    WINE_TRACE("(%d, %u)\n", stream, timeout);
    return 0;
}

DLLEXPORT void __cdecl spifns_stream_unlock(spifns_stream_t stream)
{
    WINE_TRACE("(%d)\n", stream);
}

#endif /* SPIFNS_API == SPIFNS_API_1_4 */
