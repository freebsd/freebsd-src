/*-
 * Copyright (c) 1992, 1993, 1995 Eugene W. Stark
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Eugene W. Stark.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY EUGENE W. STARK (THE AUTHOR) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Device capabilities
 */

#define ISLIGHT		1		/* Is device a light? */
#define CANQUERY	2		/* Responds to status query */

/*
 * Device status
 */

typedef enum {
  IDLE,
  SELECTED,
  DIMMING,
  BRIGHTENING,
  REQUESTED,
  HAILED
 } SELECT;

typedef struct {
  unsigned int devcap;		/* device capabilities */
  unsigned int changed;		/* status changed since last checkpoint? */
  time_t lastchange;		/* time status last changed */
  SELECT selected;		/* select status of device */
  unsigned int onoff;		/* nonzero if on */
  unsigned int brightness;	/* value in range 0-15 */
} STATUS;

typedef struct {
  int inuse;			/* Is entry in use? */
  FILE *user;			/* Socket to notify user */
  int house;			/* House code of device to monitor */
  int unit;			/* Unit code of device to monitor */
} MONENTRY;

#define MAXMON 5		/* Maximum number of monitor entries */

extern FILE *Log;		/* Log file */
extern FILE *User;		/* User connection */
extern STATUS Status[16][16];	/* Device status table */
extern int status;		/* Status file descriptor */
extern int tw523;		/* tw523 controller */
extern MONENTRY Monitor[MAXMON];/* Monitor table */

extern char *thedate();
