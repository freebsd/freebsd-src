/*
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: modem.h,v 1.16 1998/01/21 02:15:23 brian Exp $
 *
 *	TODO:
 */

struct physical;

extern int RawModem(struct physical *);
extern void WriteModem(struct physical *, int, const char *, int);
extern void ModemStartOutput(struct physical *);
extern int OpenModem(struct physical *);
extern int ModemSpeed(struct physical *);
extern int ModemQlen(struct physical *);
extern int DialModem(struct physical *);
extern speed_t IntToSpeed(int);
extern void ModemTimeout(void *);
extern void DownConnection(void);
extern void ModemOutput(struct physical *modem, int, struct mbuf *);
extern int ChangeParity(struct physical *, const char *);
extern void HangupModem(struct physical *, int);
extern int ShowModemStatus(struct cmdargs const *);
extern void Enqueue(struct mqueue *, struct mbuf *);
extern struct mbuf *Dequeue(struct mqueue *);
extern void SequenceQueues(struct physical *physical);
extern void ModemAddInOctets(struct physical *modem, int);
extern void ModemAddOutOctets(struct physical *modem, int);

