#include "spifns.h"
#include <windows.h>
#include <stdio.h>
#include "basics.h"
#include <stdlib.h>

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
//RE Check: Completely identical
DLLEXPORT HANDLE __cdecl spifns_open_port(int nPort) {
	OSVERSIONINFOA ovi;
	char szFilename[20];

	ovi.dwOSVersionInfoSize=sizeof(ovi);
	GetVersionExA(&ovi);
	if (ovi.dwPlatformId!=VER_PLATFORM_WIN32_NT)
		return INVALID_HANDLE_VALUE;
	sprintf(szFilename,"\\\\.\\COM%d",nPort);
	HANDLE hDevice=CreateFileA(szFilename,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,0,0);
	if (hDevice==INVALID_HANDLE_VALUE)
		return INVALID_HANDLE_VALUE;
	DCB dcb={0};
	dcb.BaudRate=CBR_256000;
	dcb.DCBlength=sizeof(dcb);
	dcb.fBinary=FALSE;
	dcb.fParity=FALSE;
	dcb.fOutxCtsFlow=FALSE;
	dcb.fOutxDsrFlow=FALSE;
	dcb.fDtrControl=DTR_CONTROL_DISABLE;
	dcb.fDsrSensitivity=FALSE;
	dcb.fTXContinueOnXoff=FALSE;
	dcb.fOutX=FALSE;
	dcb.fInX=FALSE;
	dcb.fErrorChar=FALSE;
	dcb.fNull=FALSE;
	dcb.fRtsControl=RTS_CONTROL_DISABLE;
	dcb.fAbortOnError=FALSE;
	dcb.fDummy2=FALSE;
	dcb.wReserved=0;
	dcb.XonLim=dcb.XoffLim=0;
	dcb.ByteSize=8;
	dcb.Parity=NOPARITY;
	dcb.StopBits=ONESTOPBIT;
	dcb.XonChar=dcb.XoffChar=0;
	dcb.ErrorChar=0;
	dcb.EofChar=0;
	dcb.EvtChar=0;
	dcb.wReserved1=0;
	if (!SetCommState(hDevice,&dcb)) {
		CloseHandle(hDevice);
		return INVALID_HANDLE_VALUE;
	}
  EscapeCommFunction(hDevice,CLRDTR);
  Sleep(10);
  EscapeCommFunction(hDevice,SETDTR);
  BYTE bExpected[]={'C','S','R','S','P','I','1'};
  BYTE bTemp[10];
	DWORD nWritten;
  if (!ReadFile(hDevice,bTemp,sizeof(bExpected),&nWritten,0) || nWritten!=sizeof(bExpected) || memcmp(bExpected,bTemp,sizeof(bExpected))!=0) {
		CloseHandle(hDevice);
		return INVALID_HANDLE_VALUE;
	}
	return hDevice;
}
//RE Check: Fully identical
DLLEXPORT void __cdecl spifns_close_port() {
	if (g_hDevice) {
		CloseHandle(g_hDevice);
		g_hDevice=0;
		g_nSpiPort=0;
	}
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
		if (pnData)
			memcpy(bCopy,pnData,sizeof(unsigned short)*((nLength<8)?nLength:8));
		else
			memset(bCopy,0,sizeof(bCopy));
		if (nLength<2) {
			spifns_debugout(pszTable[nLength],nAddress,cOperation,bCopy[0]);
		} else {
			spifns_debugout(pszTable[((nLength<9)?nLength:9)],nAddress,nAddress+nLength-1,cOperation,bCopy[0],bCopy[1],bCopy[2],bCopy[3],bCopy[4],bCopy[5],bCopy[6],bCopy[7]);
		}
	}
}
//RE Check: Functionally equivalent, register choice and initialization difference.
DLLEXPORT int __cdecl spifns_sequence_write(unsigned short nAddress, unsigned short nLength, unsigned short *pnInput) {
	if (!g_hDevice) {
		static const char szError[]="No COM port selected";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_NO_LPT_PORT_SELECTED;
		return 1;
	}
	BYTE bStart[]={0x02,nAddress>>8,nAddress&0xFF};
	DWORD dwWritten;
	if (!WriteFile(g_hDevice,bStart,sizeof(bStart),&dwWritten,0) || dwWritten!=sizeof(bStart)) {
		const char szError[]="Unable to start writing";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nErrorAddress=nAddress;
		g_nError=SPIERR_IOCTL_FAILED;
		return 1;
	}
	BYTE bBlockLength=0;
	BYTE bBuffer[256];
	while (true) {
		if (!ReadFile(g_hDevice,&bBlockLength,1,&dwWritten,0) || dwWritten!=1)
			break;
		if (bBlockLength>nLength)
      bBlockLength=nLength;
    if (!WriteFile(g_hDevice,&bBlockLength,1,&dwWritten,0) || dwWritten!=1)
      break;
    if (bBlockLength==0)
      break;
		for (unsigned int i=0; i<bBlockLength; i++) {
			bBuffer[(i*2)]=(*pnInput)>>8;
			bBuffer[(i*2)+1]=(*pnInput)&0xFF;
			pnInput++;
			nLength--;
		}
		if (!WriteFile(g_hDevice,bBuffer,bBlockLength*2,&dwWritten,0) || dwWritten!=bBlockLength*2)
			break;
	}
	if (nLength!=0) {
		const char szError[]="Not all data written";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nErrorAddress=nAddress;
		g_nError=SPIERR_IOCTL_FAILED;
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
//RE Check: Functionally identical, can't get the ASM code to match.
DLLEXPORT int __cdecl spifns_sequence_read(unsigned short nAddress, unsigned short nLength, unsigned short *pnOutput) {
	if (!g_hDevice) {
		static const char szError[]="No COM port selected";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_NO_LPT_PORT_SELECTED;
		return 1;
	}
	BYTE bStart[]={0x01,nAddress>>8,nAddress&0xFF};
	DWORD dwWritten;
	if (!WriteFile(g_hDevice,bStart,sizeof(bStart),&dwWritten,0) || dwWritten!=sizeof(bStart) || !ReadFile(g_hDevice,bStart,1,&dwWritten,0) || dwWritten!=1 || bStart[0]!=0) {
		spifns_debugout_readwrite(nAddress,'r',0,0);
		const char szError[]="Unable to read";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nErrorAddress=nAddress;
		g_nError=SPIERR_READ_FAILED;
		return 1;
	}
	BYTE bBlockLength;
	BYTE bBuffer[256];
	while (true) {
		bBlockLength=min(nLength,64);
		nLength-=bBlockLength;
		if (!WriteFile(g_hDevice,&bBlockLength,1,&dwWritten,0) || dwWritten!=1) {
			spifns_debugout_readwrite(nAddress,'r',0,0);
			const char szError[]="Unable to read (requesting buffer)";
			memcpy(g_szErrorString,szError,sizeof(szError));
			g_nErrorAddress=nAddress;
			g_nError=SPIERR_READ_FAILED;
			return 1;
		}
		//Break when all data is received
		if (bBlockLength==0)
			break;
		if (!ReadFile(g_hDevice,bBuffer,bBlockLength*sizeof(short),&dwWritten,0) || dwWritten!=bBlockLength*2) {
			spifns_debugout_readwrite(nAddress,'r',0,0);
			const char szError[]="Unable to read (reading buffer)";
			memcpy(g_szErrorString,szError,sizeof(szError));
			g_nErrorAddress=nAddress;
			g_nError=SPIERR_READ_FAILED;
			return 1;
		}
		for (unsigned int i=0; i<bBlockLength; i++)
			*(pnOutput++)=(bBuffer[(i*2)]<<8)|bBuffer[(i*2)+1];
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
//RE Check: Mostly identical, only registers don't match because of calling convention changes on called functions.
DLLEXPORT int __cdecl spifns_bluecore_xap_stopped() {
	if (!g_hDevice)
		return -1;
	BYTE bData[1]={0x03};
	DWORD dwWritten;
	if (!WriteFile(g_hDevice,bData,1,&dwWritten,0) || dwWritten!=1 || !ReadFile(g_hDevice,bData,1,&dwWritten,0) || dwWritten!=1)
		return -1;
	if (bData[0]==0 || bData[0]==1)
		return bData[0];
	return -1;
}
