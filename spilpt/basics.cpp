#include "spifns.h"
#include <Windows.h>
#include <stdio.h>
#include "basics.h"
#include <tchar.h>

/*
 * README:
 * I attempted to reverse engineer spilpt.dll the best I could.
 * However, compiling it with visual studio 2010 did not allow me to yield an identical DLL, and I don't have VS 2005 (original was compiled with this) installed.
 * For now, I'll leave it at this :)
 * Feel free to use this to write your own 
*/

#pragma auto_inline(off) //Prevent inlining, to create code closer to the original (to check)

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

void __cdecl spifns_debugout(const char *szFormat, ...);
void __cdecl spifns_debugout_readwrite(unsigned short nAddress, char cOperation, unsigned short nLength, unsigned short *pnData);

//RE Check: Completely identical.
void __cdecl spifns_getvarlist(const SPIVARDEF **ppList, unsigned int *pnCount) {
	*ppList=g_pVarList;
	*pnCount=sizeof(g_pVarList)/sizeof(*g_pVarList);
}
//RE Check: Opcodes functionally identical
//Original uses 'add g_nRef, 1'
//Compiler makes 'inc g_nRef'
int __cdecl spifns_init() {
	g_nRef+=1;
	return 0;
}
//RE Check: Completely identical (one opcode off, jns should be jge, but shouldn't matter. Probably unsigned/signed issues)
const char * __cdecl spifns_getvar(const char *szName) {
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
unsigned int __cdecl spifns_get_last_error(unsigned short *pnErrorAddress, const char **pszErrorString) {
	if (pnErrorAddress)
		*pnErrorAddress=g_nErrorAddress;
	if (pszErrorString)
		*pszErrorString=g_szErrorString;
	return g_nError;
}
//RE Check: Completely identical
void __cdecl spifns_set_debug_callback(spifns_debug_callback pCallback) {
	g_pDebugCallback=pCallback;
}
//RE Check: Completely identical
int __cdecl spifns_get_version() {
	return SPIFNS_VERSION;
}
//RE Check: Completely identical
HANDLE __cdecl spifns_open_port(int nPort) {
	OSVERSIONINFOA ovi;
	char szFilename[20];
	const unsigned char pTestBuffer[]={0xCB};

	ovi.dwOSVersionInfoSize=sizeof(ovi);
	GetVersionExA(&ovi);
	if (ovi.dwPlatformId!=VER_PLATFORM_WIN32_NT)
		return INVALID_HANDLE_VALUE;
	sprintf(szFilename,"\\\\.\\LPTSPI%d",nPort);
	HANDLE hDevice=CreateFileA(szFilename,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,0,0);
	if (hDevice==INVALID_HANDLE_VALUE)
		return INVALID_HANDLE_VALUE;
	DWORD nWritten;
	if (!WriteFile(hDevice,pTestBuffer,sizeof(pTestBuffer),&nWritten,0)) {
		CloseHandle(hDevice);
		return INVALID_HANDLE_VALUE;
	}
	return hDevice;
}
//RE Check: Fully identical
void __cdecl spifns_close_port() {
	if (g_hDevice) {
		CloseHandle(g_hDevice);
		g_hDevice=0;
		g_nSpiPort=0;
	}
}
//RE Check: TODO
bool __cdecl SPITransfer(int nInput, int nBits, int *pnRetval, bool bEndTransmission) {
	if (pnRetval)
		*pnRetval=0;
	if (!g_hDevice) {
		static const char szError[]="No LPT port selected";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_NO_LPT_PORT_SELECTED;
		return 0;
	}
	unsigned int nShiftPeriod=g_nSpiShiftPeriod;
	unsigned int nMaxDataLength=2*g_nSpiShiftPeriod*(nBits+4);
	unsigned int nDataWritten=0;
	unsigned int bit;
	unsigned char *pDataInput=(unsigned char *)malloc(nMaxDataLength);
	unsigned char *pDataOutput=(unsigned char *)malloc(nMaxDataLength);
	if (!pDataInput || !pDataOutput) {
		free(pDataInput);
		free(pDataOutput);
		static const char szError[]="Malloc failed";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_MALLOC_FAILED;
		return 0;
	}
	if (nShiftPeriod) {
		memset(pDataInput,g_bCurrentOutput,nShiftPeriod);
		nDataWritten=nShiftPeriod;
	}
	unsigned char bOutput=g_bCurrentOutput;
	for (bit=nBits; bit; bit--) {
		//unsigned char *pDataInputIterator=&pDataInput[nDataWritten];
		if ((1<<(bit-1)) & nInput)
			bOutput|=BV_MOSI;
		else
			bOutput&=~BV_MOSI;
		bOutput&=~BV_CLK;
		if (nShiftPeriod) {
			memset(&pDataInput[nDataWritten],bOutput,nShiftPeriod);
			nDataWritten+=nShiftPeriod;
			//pDataInputIterator+=nShiftPeriod;
		}
		bOutput|=BV_CLK;
		if (nShiftPeriod) {
			memset(&pDataInput[nDataWritten],bOutput,nShiftPeriod);
			nDataWritten+=nShiftPeriod;
			//pDataInputIterator+=nShiftPeriod;
		}
	}
	g_bCurrentOutput=bOutput;
	if (bEndTransmission) {
		nDataWritten-=nShiftPeriod;
		bOutput&=~BV_CLK;
		if (nShiftPeriod) {
			memset(&pDataInput[nDataWritten],bOutput,nShiftPeriod);
			nDataWritten+=nShiftPeriod;
		}
		bOutput|=BV_CSB;
		g_bCurrentOutput=bOutput;
		if (nShiftPeriod) {
			memset(&pDataInput[nDataWritten],bOutput,nShiftPeriod);
			nDataWritten+=nShiftPeriod;
			if (nShiftPeriod) {
				memset(&pDataInput[nDataWritten],bOutput|BV_CLK,nShiftPeriod);
				nDataWritten+=nShiftPeriod;
				if (nShiftPeriod) {
					memset(&pDataInput[nDataWritten],bOutput,nShiftPeriod);
					nDataWritten+=nShiftPeriod;
					if (nShiftPeriod) {
						memset(&pDataInput[nDataWritten],bOutput|BV_CLK,nShiftPeriod);
						nDataWritten+=nShiftPeriod;
					}
				}
			}
		}
	}
	//loc10001496
	DWORD nBytesReturned;
	if (!DeviceIoControl(g_hDevice,0,pDataInput,nDataWritten,pDataOutput,nDataWritten,&nBytesReturned,0)) {
		static const char szError[]="IOCTL failed";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_IOCTL_FAILED;
		free(pDataInput);
		free(pDataOutput);
		return 0;
	}
	unsigned int nDataRead=0;
	int nRetval=0;
	for (bit=nBits+1; bit; bit--) {
		nRetval*=2;
		if (pDataOutput[nDataRead] & BV_MISO)
			nRetval|=1;
		nDataRead+=2*g_nSpiShiftPeriod;
	}
	free(pDataInput);
	free(pDataOutput);
	if (pnRetval)
		*pnRetval=nRetval;
	return 1;
}
//RE Check: Completely identical
bool __cdecl spifns_pre_transmit() {
	if (g_nSpiMul)
		g_bCurrentOutput|=BV_CSB;
	else
		g_bCurrentOutput=(g_bCurrentOutput & ~BV_MUL)|BV_CSB;
	bool retval=SPITransfer(0,2,0,0);
	if (g_nSpiMul)
		g_bCurrentOutput&=~BV_CSB;
	else {
		if (g_nSpiMulConfig)
			g_bCurrentOutput|=BV_CSB|BV_MUL;
		else
			g_bCurrentOutput&=~(BV_CSB|BV_MUL);
	}
	return retval;
}
//RE Check: Completely identical
void __cdecl spifns_debugout(const char *szFormat, ...) {
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
void __cdecl spifns_close() {
	g_nRef--;
	if (g_nRef==0)
		spifns_close_port();
}
//RE Check: Completely identical
void __cdecl spifns_chip_select(int nChip) {
	g_bCurrentOutput=g_bCurrentOutput&0xD3|0x10;
	g_nSpiMul=0;
	g_nSpiMulConfig=nChip;
	g_nSpiMulChipNum=nChip;
	spifns_debugout(
		"ChipSelect: %04x mul:%d confog:%d chip_num:%d\n",
		g_bCurrentOutput,
		0,
		nChip,
		nChip);
}
//RE Check: Completely identical
const char* __cdecl spifns_command(const char *szCmd) {
	if (stricmp(szCmd,"SPISLOWER")==0) {
		if (g_nSpiShiftPeriod<40) {
			//SPI Shift Period seems to be done about 1.5 times, plus 1 to compensate for rounding down (for example in 1)
			spifns_debugout("Delays now %d\n",g_nSpiShiftPeriod=g_nSpiShiftPeriod + (g_nSpiShiftPeriod>>2) + 1); //>>2 => /2 ?
		}
	}
	return 0;
}
//RE Check: Opcodes functionally identical
//Original uses 'add esi, 1
//Compiler makes 'inc esi'
void __cdecl spifns_enumerate_ports(spifns_enumerate_ports_callback pCallback, void *pData) {
	char szPortName[8];
	sprintf(szPortName,"LPT1",1); //Tiny bug ?
	pCallback(g_nSpiPort,szPortName,pData);
	for (int nPort=2; nPort<=16; nPort++) {
		if (spifns_open_port(nPort)!=INVALID_HANDLE_VALUE) {
			sprintf(szPortName,"LPT%d",nPort);
			pCallback(nPort,szPortName,pData);
		}
	}
}
//RE Check: Opcodes functionally identical
//Original passes arguments through eax
//Compiled takes argument on stack. Maybe change it to __fastcall ?
void __cdecl spifns_sequence_setvar_spishiftperiod(int nPeriod) {
	spifns_debugout("Delays set to %d\n",g_nSpiShiftPeriod=nPeriod);
}
//RE Check: Opcodes functionally identical, slightly re-ordered (but no impact on result)
//Original passes arguments through eax
//Compiled takes argument on stack. Maybe change it to __fastcall ?
bool __cdecl  spifns_sequence_setvar_spiport(int nPort) {
	spifns_close_port();
	if (INVALID_HANDLE_VALUE==(g_hDevice=spifns_open_port(nPort)))
		return false;
	g_nSpiPort=nPort;
	g_nSpiShiftPeriod=1;
	return true;
}
//RE Check: Functionally equivalent, but completely different ASM code :(
void __cdecl spifns_debugout_readwrite(unsigned short nAddress, char cOperation, unsigned short nLength, unsigned short *pnData) {
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
int __cdecl spifns_sequence_write(unsigned short nAddress, unsigned short nLength, unsigned short *pnInput) {
	spifns_debugout_readwrite(nAddress,'w',nLength,pnInput);
	if (!spifns_pre_transmit())
		return 1;
	if (!SPITransfer(g_nCmdWriteBits|2,8,0,0))
		return 1;
	if (!SPITransfer(nAddress,16,0,0))
		return 1;
	while (nLength) {
		if (!SPITransfer(*(pnInput++),16,0,0))
			return 1;
		nLength--;
	}
	return 0;
}
//RE Check: Functionally identical, register choice, calling convention, and some ordering changes.
void __cdecl spifns_sequence_setvar_spimul(unsigned int nMul) {
	BYTE bNewOutput=g_bCurrentOutput&~BV_CLK;
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
	spifns_debugout("MulitplexSelect: %04x mul:%d config:%d chip_num:%d\n",g_bCurrentOutput,g_nSpiMul,g_nSpiMulConfig,g_nSpiMulChipNum);
}
//RE Check: Functionally identical, register choice, calling convention, stack size, and some ordering changes.
int __cdecl spifns_sequence_setvar(const char *szName, const char *szValue) {
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
bool __cdecl spifns_sequence_read_start(unsigned short nAddress) {
	unsigned char bReadCommand=g_nCmdReadBits|3;
	if (!SPITransfer(bReadCommand,8,0,0))
		return false;
	int nReturn;
	if (!SPITransfer(nAddress,16,0,0) || !SPITransfer(0,16,&nReturn,0))
		return false;
	//Cast to short, to drop any additional bits
	if (((unsigned short)nReturn) != ((nAddress >> 8)|(bReadCommand<<8))) {
		spifns_debugout_readwrite(nAddress,'r',0,0);
		const char szError[]="Unable to read";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nErrorAddress=nAddress;
		g_nError=SPIERR_READ_FAILED;
		return false;
	}
	return true;
}
//RE Check: Functionally identical, can't get the ASM code to match.
int __cdecl spifns_sequence_read(unsigned short nAddress, unsigned short nLength, unsigned short *pnOutput) {
	if (!spifns_pre_transmit())
		return 1;
	//loc_10001D10
	if (!spifns_sequence_read_start(nAddress))
		return 1;
	//Not accurate, but output should be the same.
	unsigned short *pnOutputIterator=pnOutput;
	for (unsigned short i=nLength; i>0; i--) {
		int nTemp;
		if (!SPITransfer(0,16,&nTemp,i==1))
			return 1;
		*pnOutputIterator=(unsigned short)nTemp;
		pnOutputIterator++;
	}
	//loc_10001D63
	spifns_debugout_readwrite(nAddress,'r',nLength,pnOutput);
	return 0;
}
//RE Check: Functionally identical, can't get the ASM code to match.
int __cdecl spifns_sequence(SPISEQ *pSequence, unsigned int nCount) {
	int nRetval=0;
	while (nCount--) {
		switch (pSequence->nType) {
		case SPISEQ::TYPE_READ:{
			if (spifns_sequence_read(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
			if (!spifns_pre_transmit())
				nRetval=1;
							   }break;
		case SPISEQ::TYPE_WRITE:{
			if (spifns_sequence_write(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
			if (!spifns_pre_transmit())
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
int __cdecl spifns_bluecore_xap_stopped() {
	if (!spifns_pre_transmit())
		return -1;
	if (!SPITransfer(0,0,0,0))
		return -1;
	int nSpiRetval;
	if (!SPITransfer(0,0,&nSpiRetval,0))
		return -1;
	if (!spifns_pre_transmit())
		return -1;
	g_bCurrentOutput&=~BV_CLK; //Set Chip Select low
	if (!nSpiRetval)
		return 0; //Not stopped
	unsigned short nTemp;
	if (spifns_sequence_read(0xFF9A,0x0001,&nTemp) == 0) {
		return 1;
	}
	g_nSpiShiftPeriod=1;
	return -1;
}