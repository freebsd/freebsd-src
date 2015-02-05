/* 
 * hopf_PCI_io.h
 * structur definition and prototype Function declarations
 *
 * Date: 21.03.2000 Revision: 01.10 
 *
 * Copyright (C) 1999, 2000 by Bernd Altmeier altmeier@ATLSoft.de
 * 
 */

#if defined(__cplusplus)
extern "C"{
#endif

#ifndef __inpREAD_H
#define __inpREAD_H


typedef struct _CLOCKVER {    
	CHAR cVersion[255];  
} CLOCKVER, *PCLOCKVER, *LPCLOCKVER;

typedef struct _HOPFTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
    WORD wStatus;
} HOPFTIME, *PHOPFTIME, *LPHOPFTIME;

typedef struct _SATSTAT {    
	BYTE wVisible;    
	BYTE wMode;
    BYTE wSat0;
	BYTE wRat0;
	BYTE wSat1;
	BYTE wRat1;
	BYTE wSat2;
	BYTE wRat2;
	BYTE wSat3;
	BYTE wRat3;
	BYTE wSat4;
	BYTE wRat4;
	BYTE wSat5;
	BYTE wRat5;
	BYTE wSat6;
	BYTE wRat6;
	BYTE wSat7;
	BYTE wRat7;
} SATSTAT, *PSATSTAT, *LPSATSTAT;


typedef struct _GPSPOS {    
	LONG wAltitude;    // Höhe immer 0
	LONG wLongitude;   // Länge in Msec
	LONG wLatitude;    // Breite in Msec  
} GPSPOS, *PGPSPOS, *LPGPSPOS;


typedef struct _DCFANTENNE {    
	BYTE bStatus;    
	BYTE bStatus1;    
	WORD wAntValue;    
} DCFANTENNE, *PDCFANTENNE, *LPDCFANTENNE;



// Function declarations
BOOL  OpenHopfDevice();
BOOL  CloseHopfDevice();
VOID  GetHopfTime(LPHOPFTIME Data, DWORD Offset);
VOID  GetHopfLocalTime(LPHOPFTIME Data);
VOID  GetHopfSystemTime(LPHOPFTIME Data);
VOID  GetSatData(LPSATSTAT Data);
VOID  GetDiffTime(LPLONG Data);
VOID  GetPosition(LPGPSPOS Data);
VOID  GetHardwareVersion(LPCLOCKVER Data);
VOID  GetHardwareData(LPDWORD Data,WORD Ofs);
VOID  GetDCFAntenne(LPDCFANTENNE Data);
 
 
#if defined(__cplusplus)
}
#endif 

#endif /* inpREAD_H */

