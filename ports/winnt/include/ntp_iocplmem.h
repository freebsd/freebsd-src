/*
 * ntp_iocplmem.h - separate memory pool for IOCPL related objects
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 *
 * --------------------------------------------------------------------
 * Notes on the implementation:
 *
 * Implements a thin layer over Windows Memory pools
 */
#ifndef NTP_IOCPLMEM_H
#define NTP_IOCPLMEM_H

#include <stdlib.h>

extern void IOCPLPoolInit(size_t initSize);
extern void IOCPLPoolDone(void);

extern void* __fastcall	IOCPLPoolAlloc(size_t size, const char*	desc);
extern void* __fastcall	IOCPLPoolMemDup(const void* psrc, size_t size, const char* desc);
extern void  __fastcall	IOCPLPoolFree(void* ptr, const char* desc);

#endif /*!defined(NTP_IOCPLMEM_H)*/
