/* 
 * hopf_PCI_io.c
 * Read data from a hopf PCI clock using the ATLSoft WinNT driver.
 *
 * Date: 21.03.2000 Revision: 01.10 
 *
 * Copyright (C) 1999, 2000 by Bernd Altmeier altmeier@ATLSoft.de
 * 
 */

/*
 * Ignore nonstandard extension warning.
 * This happens when including winioctl.h
 */
#pragma warning(disable: 4201)
#define _FILESYSTEMFSCTL_

#include <config.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <winioctl.h>

#include "ntp_stdlib.h"
#include "hopf_PCI_io.h"


#define ATL_PASSTHROUGH_READ_TOSIZE	(3 * sizeof(ULONG))
#define ATL_PASSTHROUGH_READ_FROMSIZE	0
#define IOCTL_ATLSOFT_PASSTHROUGH_READ	CTL_CODE(			\
						FILE_DEVICE_UNKNOWN,	\
						0x805,			\
						METHOD_BUFFERED,	\
						FILE_ANY_ACCESS)


HANDLE	hDevice = NULL;	// this is the handle to the PCI Device

HANDLE		hRdEvent;
OVERLAPPED	Rdoverlapped;
OVERLAPPED *	pRdOverlapped;

ULONG		iobuffer[256];
DWORD		cbReturned;
BOOL		HaveBoard = FALSE;

struct {
	ULONG	region;
	ULONG	offset;
	ULONG	count;
} io_params;


BOOL
OpenHopfDevice(void)
{
	OSVERSIONINFO	VersionInfo;
	ULONG		deviceNumber;
	CHAR		deviceName[255];
			
	VersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&VersionInfo);
	switch (VersionInfo.dwPlatformId) {

	case VER_PLATFORM_WIN32_WINDOWS:	// Win95/98
		return FALSE;	// "NTP does not support Win 95-98."
		break;

	case VER_PLATFORM_WIN32_NT:	// WinNT
		deviceNumber = 0;
		snprintf(deviceName, sizeof(deviceName),
			 "\\\\.\\hclk6039%d", deviceNumber + 1);
		hDevice = CreateFile(
			deviceName,
			GENERIC_WRITE | GENERIC_READ,
			FILE_SHARE_WRITE | FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_OVERLAPPED,
			NULL);
		break;

	default:
		hDevice = INVALID_HANDLE_VALUE;
		break;
	} // end switch

	if (INVALID_HANDLE_VALUE == hDevice) // the system didn't return a handle
		return FALSE;  //"A handle to the driver could not be obtained properly"

	// an event to be used for async transfers
	hRdEvent = CreateEvent(
		NULL,
		TRUE,
		FALSE,
		NULL);	

	if (INVALID_HANDLE_VALUE == hRdEvent) 
		return FALSE;  // the system didn't return a handle

	pRdOverlapped = &Rdoverlapped;
	pRdOverlapped->hEvent = hRdEvent;

	HaveBoard = TRUE; // board installed and we have access

	return TRUE;
} // end of OpenHopfDevice()


BOOL
CloseHopfDevice(void)
{
	CloseHandle(hRdEvent);// When done, close the handle to the driver

	return CloseHandle(hDevice);
} // end of CloseHopfDevice()


void
ReadHopfDevice(void)
{
	if (!HaveBoard)
		return;

	DeviceIoControl(
		hDevice,
		IOCTL_ATLSOFT_PASSTHROUGH_READ,
		&io_params,
		ATL_PASSTHROUGH_READ_TOSIZE,
		iobuffer,
		ATL_PASSTHROUGH_READ_FROMSIZE
		 + io_params.count * sizeof(ULONG),
		&cbReturned, 
		pRdOverlapped
		);
}


#ifdef NOTUSED
void
GetHardwareData(
	LPDWORD	Data32,
	WORD	Ofs
	)
{
	io_params.region = 1;
	io_params.offset = Ofs;
	io_params.count = 1;
	ReadHopfDevice();
	*Data32 = iobuffer[0];
}
#endif	/* NOTUSED */


void
GetHopfTime(
	LPHOPFTIME	Data,
	DWORD		Offset
	)
{
	io_params.region = 1;
	io_params.offset = Offset;
	io_params.count = 4;

	ReadHopfDevice();

	Data->wHour = 0;
	Data->wMinute = 0;
	Data->wSecond = 0;
	while (iobuffer[0] >= 60 * 60 * 1000) {
		iobuffer[0] = iobuffer[0] - 60 * 60 * 1000;
		Data->wHour++;
	}
	while (iobuffer[0] >= 60 * 1000) {
		iobuffer[0] = iobuffer[0] - 60 * 1000;
		Data->wMinute++;
	}
	while (iobuffer[0] >= 1000) {
		iobuffer[0] = iobuffer[0] - 1000;
		Data->wSecond++;
	}
	Data->wMilliseconds = LOWORD(iobuffer[0]);
	Data->wDay = HIBYTE(HIWORD(iobuffer[1]));
	Data->wMonth = LOBYTE(HIWORD(iobuffer[1]));
	Data->wYear = LOWORD(iobuffer[1]);
	Data->wDayOfWeek = HIBYTE(HIWORD(iobuffer[2]));
	if (Data->wDayOfWeek == 7) // Dow Korrektur
		Data->wDayOfWeek = 0;
	
 	io_params.region = 1;
	io_params.offset += 0x08;
	io_params.count = 1;

	ReadHopfDevice();

	Data->wStatus = LOBYTE(HIWORD(iobuffer[0]));
}


#ifdef NOTUSED
void
GetHopfLocalTime(
	LPHOPFTIME Data
	)
{
	DWORD Offset = 0;

	GetHopfTime(Data, Offset);
}
#endif	/* NOTUSED */


void
GetHopfSystemTime(
	LPHOPFTIME Data
	)
{
	DWORD Offset = 0x10;

	GetHopfTime(Data,Offset);
}


#ifdef NOTUSED
void
GetSatData(
	LPSATSTAT Data
	)
{
	io_params.region = 1;
	io_params.offset = 0xb0;
	io_params.count = 5;

	ReadHopfDevice();
				
	Data->wVisible	= HIBYTE(HIWORD(iobuffer[0]));
	Data->wMode	= LOBYTE(LOWORD(iobuffer[0]));
	Data->wSat0	= HIBYTE(HIWORD(iobuffer[1]));
	Data->wRat0	= LOBYTE(HIWORD(iobuffer[1]));
	Data->wSat1	= HIBYTE(LOWORD(iobuffer[1]));
	Data->wRat1	= LOBYTE(LOWORD(iobuffer[1]));
	Data->wSat2	= HIBYTE(HIWORD(iobuffer[2]));
	Data->wRat2	= LOBYTE(HIWORD(iobuffer[2]));
	Data->wSat3	= HIBYTE(LOWORD(iobuffer[2]));
	Data->wRat3	= LOBYTE(LOWORD(iobuffer[2]));
	Data->wSat4	= HIBYTE(HIWORD(iobuffer[3]));
	Data->wRat4	= LOBYTE(HIWORD(iobuffer[3]));
	Data->wSat5	= HIBYTE(LOWORD(iobuffer[3]));
	Data->wRat5	= LOBYTE(LOWORD(iobuffer[3]));
	Data->wSat6	= HIBYTE(HIWORD(iobuffer[4]));
	Data->wRat6	= LOBYTE(HIWORD(iobuffer[4]));
	Data->wSat7	= HIBYTE(LOWORD(iobuffer[4]));
	Data->wRat7	= LOBYTE(LOWORD(iobuffer[4]));
}


void
GetDiffTime(
	LPLONG Data
	)
{
	io_params.region = 1;
	io_params.offset = 0x0c;
	io_params.count = 1;

	ReadHopfDevice();

	*Data = iobuffer[0];
}


void
GetPosition(
	LPGPSPOS Data
	)
{
	io_params.region = 1;
	io_params.offset = 0x90; // Positionsdaten Länge
	io_params.count  = 1;

	ReadHopfDevice();

	Data->wLongitude = iobuffer[0]; //in Millisekunden
	io_params.region = 1;
	io_params.offset = 0xa0; // Positionsdaten Breite
	io_params.count  = 1;

	ReadHopfDevice();

	Data->wLatitude	= iobuffer[0];
	Data->wAltitude	= 0;
}


void
GetHardwareVersion(
	LPCLOCKVER Data
	)
{
	int i;

	io_params.region = 1;
	io_params.offset = 0x50;
	io_params.count = 12;

	ReadHopfDevice();
				
	Data->cVersion[0] = '\0';
	iobuffer[13] = 0;
	for (i = 0; i < 13; i++) {
		Data->cVersion[i * 4    ] = HIBYTE(HIWORD(iobuffer[i]));
		Data->cVersion[i * 4 + 1] = LOBYTE(HIWORD(iobuffer[i]));
		Data->cVersion[i * 4 + 2] = HIBYTE(LOWORD(iobuffer[i]));
		Data->cVersion[i * 4 + 3] = LOBYTE(LOWORD(iobuffer[i]));
	}
}


void
GetDCFAntenne(
	LPDCFANTENNE Data
	)
{
	io_params.region = 1;
	io_params.offset = 0xcc;
	io_params.count = 1;

	ReadHopfDevice();
	Data->bStatus1	= HIBYTE(HIWORD(iobuffer[0]));
	Data->bStatus	= LOBYTE(HIWORD(iobuffer[0]));
	Data->wAntValue	= LOWORD(iobuffer[0]);
}
#endif	/* NOTUSED */

