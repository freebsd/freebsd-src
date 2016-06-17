/* 
 *
 *  (C) 1990 - 2000 Specialix International Ltd., Byfleet, Surrey, UK.
 *
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#ifndef _cirrusprots_h
#define _cirrusprots_h

#ifdef RTA
extern void cd1400_reset ( int uart) ;
extern void cd1400_init ( int uart ) ;
extern void ccr_wait ( int priority, int port) ;
extern void cd1400_txstart( int port) ;
extern void cd1400_rxstart ( int port) ;
extern void command_acknowledge ( PHB *port_header ) ;
extern int close_port ( ushort port, PHB *port_header, ushort preemptive, int pseudo) ;
extern void command_preemptive ( PKT *packet) ;
extern void rup_service ( void ) ;
extern ushort GetModemLines(struct PHB *, register short *);
extern void cd1400_intr (Process *cirrus_p, ushort *RtaType) ;
extern void cd1400_mdint ( short port) ;
extern void cd1400_rxint ( short port) ;
extern void cd1400_rxexcept ( short port) ;
extern void cd1400_txdata ( short port, PHB *port_header, PKT *packet) ;
extern void cd1400_fast_clock(void);
extern void cd1400_map_baud ( ushort host_rate, ushort *prescaler, ushort *divisor) ;
extern void cd1400_modem ( ushort port, ushort way) ;
extern void cd1400_txcommand ( short port, PHB *port_header, PKT *packet) ;
extern void cd1400_txint ( int port) ;
void Rprintf( char *RIOPrBuf, char *Str, ... );
#if defined(DCIRRUS)
void debug_packet(PKT *pkt, int option, char *string, int channel);
#endif	/* defined(DCIRRUS) */
#endif

#ifdef HOST
extern void wflush (PHB *);
extern void command_preemptive (PKT *);
#endif

#endif /* _cirrusprots_h */
