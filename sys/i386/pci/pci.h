/**************************************************************************
**
**  $Id: pci.h,v 2.0.0.1 94/08/18 23:09:26 wolf Exp $
**
**  #define   for pci bus device drivers
**
**  386bsd / FreeBSD
**
**-------------------------------------------------------------------------
**
** Copyright (c) 1994 Wolfgang Stanglmeier.  All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**-------------------------------------------------------------------------
**
**  $Log:	pci.h,v $
**  Revision 2.0.0.1  94/08/18  23:09:26  wolf
**  Copyright message.
**  
**  Revision 2.0  94/07/10  15:53:30  wolf
**  FreeBSD release.
**  
**  Revision 1.0  94/06/07  20:02:21  wolf
**  Beta release.
**  
***************************************************************************
*/

#ifndef __PCI_H__
#define __PCI_H__

/*
**  main pci initialization function.
**  called at boot time from autoconf.c
*/

void pci_configure(void);

/*
**  pci configuration id
**
**  is constructed from: bus, device & function numbers.
*/

typedef union {
	u_long	 cfg1;
        struct {
		 u_char   enable;
		 u_char   forward;
		 u_short  port;
	       } cfg2;
	} pcici_t;

/*
**  Each pci device has an unique device id.
**  It is used to find a matching driver.
*/

typedef u_long pcidi_t;

#endif	/*__PCI_H__*/
