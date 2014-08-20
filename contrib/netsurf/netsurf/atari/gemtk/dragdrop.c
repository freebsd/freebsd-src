/*
*	Routine pour Drag and drop sous MultiTos
*	source: D&D Atari, demo OEP (Alexander Lorenz)
*
*	 Struktur OEP (oep.apid): AES-ID der Applikation
*
*	(Tab = 4)
*/

#ifdef __PUREC__
#include <tos.h>
#else
#include <osbind.h>
#include <mintbind.h>
#include <signal.h>
#endif

#include <string.h>
#include <stdio.h>

#include "gemtk.h"
#include "cflib.h"

#ifndef EACCDN
#define EACCDN (-36)
#endif

#ifndef FA_HIDDEN
#define FA_HIDDEN 0x02
#endif

static char pipename[] = "U:\\PIPE\\DRAGDROP.AA";
static long pipesig;

/*
*	Routinen fÅr den Sender
*/

/*
*	Erzeugt Pipe fÅr D&D
*
* 	Eingabeparameter:
*	pipe	-	Pointer auf 2 Byte Buffer fÅr Pipeextension
*
*	Ausgabeparameters:
*	keine
*
*	Returnwert:
* 	>0: Filehandle der Pipe
* 	-1: Fehler beim Erzeugen der Pipe
*/

short gemtk_dd_create(short *pipe)
{
	long fd = -1;

	pipename[17] = 'A';
	pipename[18] = 'A' - 1;

	do	/* ouvre un pipe inoccupÇ */
	{
		pipename[18]++;
		if (pipename[18] > 'Z')
		{
			pipename[17]++;
			if (pipename[17] > 'Z')
				break;
			else
				pipename[18] = 'A';
		}

		/* FA_HIDDEN fÅr Pipe notwendig! */

		fd = Fcreate(pipename, FA_HIDDEN);

	} while (fd == (long) EACCDN);

	if (fd < 0L)
		return(-1);

	*pipe = (pipename[17] << 8) | pipename[18];


	/* Signalhandler konfigurieren */

	gemtk_dd_getsig(&pipesig);


	return((short) fd);
}



/*
*	Sendet AP_DRAGDROP an EmpfÑngerapplikation
*
* 	Eingabeparameter:
* 	apid		- 	AES-ID der EmfÑngerapp.
* 	fd			- 	Filehandle der D&D-Pipe
*	winid		-	Handle des Zielfensters (0 fÅr Desktopfenster)
*	mx/my		-	Maus X und Y Koord.
*					(-1/-1 fÅr einen fake Drag&Drop)
* 	kstate		-	Sondertastenstatus
* 	pipename	-	Extension der D&D-Pipe
*
*	Ausgabeparameter:
*	keine
*
*	Returnwert:
* 	>0: kein Fehler
* 	-1: EmpfÑngerapp. gibt DD_NAK zurÅck
*	-2: EmpfÑngerapp. antwortet nicht (Timeout)
*	-3: Fehler bei appl_write()
*/

short gemtk_dd_message(short apid, short fd, short winid, short mx, short my, short kstate, short pipeid)
{
	char c;
	short i, msg[8];
	long fd_mask;


	/* AES-Message define and post */

	msg[0] = AP_DRAGDROP;
	msg[1] = _AESapid;
	msg[2] = 0;
	msg[3] = winid;
	msg[4] = mx;
	msg[5] = my;
	msg[6] = kstate;
	msg[7] = pipeid;

	i = appl_write(apid, 16, msg);

	if (i == 0)
	{
		gemtk_dd_close(fd);
		return(-3);
	}


	/* receiver reaction */

	fd_mask = (1L << fd);
	i = Fselect(DD_TIMEOUT, &fd_mask, 0L, 0L);
	if (!i || !fd_mask)
	{
		/* Timeout eingetreten */

		gemtk_dd_close(fd);
		return(-2);
	}


	/* le recepteur refuse (lecture du pipe) */

	if (Fread(fd, 1L, &c) != 1L)
	{
		gemtk_dd_close(fd);
		return(-1);
	}

	if (c != DD_OK)
	{
		gemtk_dd_close(fd);
		return(-1);
	}

	return(1);
}



/*
*	Liest die 8 "bevorzugten" Extensionen der EmpfÑngerapplikation
*
* 	Eingabeparameter:
* 	fd		- 	Filehandle der D&D-Pipe
*
*	Ausgabeparameters:
*	exts	-	32 Bytebuffer fÅr die 8 bevorzugten Extensionen
*				der Zielapp.
*
*	Returnwert:
* 	>0: kein Fehler
* 	-1: Fehler beim Lesen aus der Pipe
*/

short gemtk_dd_rexts(short fd, char *exts)
{
	if (Fread(fd, DD_EXTSIZE, exts) != DD_EXTSIZE)
	{
		gemtk_dd_close(fd);
		return(-1);
	}

	return(1);
}



/*
*	Testet, ob der EmpfÑnger einen Datentyp akzeptiert
*
*	Eingabeparameter:
*	fd		-	Filehandle (von gemtk_dd_create())
*	ext		-	Zeiger auf Datentyp (4 Bytes zB. "ARGS")
*	text	-	Zeiger auf Datenbeschreibung (optional, zB. "DESKTOP args")
*	name	-	Zeiger auf Datendateiname (optional, zB. "SAMPLE.TXT")
*	size	-	Anzahl Bytes der zu sendenden Daten
*
*	Ausgabeparameter:
*	keine
*
*	Returnwert:
* 	DD_OK			-	EmpfÑnger akzeptiert Datentyp
*	DD_NAK			-	EmpfÑnger brach Drag&Drop ab
*	DD_EXT			-	EmpfÑnger lehnt Datentyp ab
*	DD_LEN			-	EmpfÑnger kann Datenmenge nicht verarbeiten
*	DD_TRASH		-	Drop erfolgte auf MÅlleimer
*	DD_PRINTER		-	Drop erfolgte auf Drucker
*	DD_CLIPBOARD	-	Drop erfolgte auf Clipboard
*/

short gemtk_dd_stry(short fd, char *ext, char *text, char *name, long size)
{
	char c;
	short hdrlen, i;

	/* 4 Bytes fÅr "ext", 4 Bytes fÅr "size",
	   2 Bytes fÅr Stringendnullen */

	hdrlen = (short) (4 + 4 + strlen(text)+1 + strlen(name)+1);


	/* Header senden */

	if (Fwrite(fd, 2L, &hdrlen) != 2L)
		return(DD_NAK);

	i = (short) Fwrite(fd, 4L, ext);
	i += (short) Fwrite(fd, 4L, &size);
	i += (short) Fwrite(fd, strlen(text)+1, text);
	i += (short) Fwrite(fd, strlen(name)+1, name);

	if (i != hdrlen)
		return(DD_NAK);


	/* auf die Antwort warten */

	if (Fread(fd, 1L, &c) != 1L)
		return(DD_NAK);

	return(c);
}



/* Routinen fÅr Sender und EmpfÑnger */

/*
*	Pipe schlieûen (Drag&Drop beenden/abbrechen)
*/

void gemtk_dd_close(short fd)
{
	/* Signalhandler restaurieren */

	gemtk_dd_setsig(pipesig);


	Fclose(fd);
}


/*
*	Signalhandler fÅr D&D konfigurieren
*
*	Eingabeparameter:
*	oldsig	-	Zeiger auf 4 Byte Puffer fÅr alten Handlerwert
*
* 	Ausgabeparameter:
*	keine
*
*	Returnwerte:
*	keine
*/

void gemtk_dd_getsig(long *oldsig)
{
	*oldsig = (long) Psignal(SIGPIPE, (void *) SIG_IGN);
}


/*
*	Signalhandler nach D&D restaurieren
*
*	Eingabeparameter:
*	oldsig	-	Alter Handlerwert (von gemtk_dd_getsig)
*
* 	Ausgabeparameter:
*	keine
*
*	Returnwerte:
*	keine
*/

void gemtk_dd_setsig(long oldsig)
{
	if (oldsig != -32L)
		Psignal(SIGPIPE, (void *) oldsig);
}



/* Routinen fÅr EmpfÑnger */

/*
*	Drag&Drop Pipe îffnen
*
*	Eingabeparameter:
*	ddnam	-	Extension der Pipe (letztes short von AP_DRAGDROP)
*	ddmsg	-	DD_OK oder DD_NAK
*
* 	Ausgabeparameter:
*	keine
*
*	Returnwerte:
*	>0	-	Filehandle der Drag&Drop pipe
*	-1	-	Drag&Drop abgebrochen
*/

short gemtk_dd_open(short ddnam, char ddmsg)
{
	long fd;

	pipename[17] = (ddnam & 0xff00) >> 8;
	pipename[18] = ddnam & 0x00ff;

	fd = Fopen(pipename, 2);

	if (fd < 0L)
		return(-1);


	/* Signalhandler konfigurieren */

	gemtk_dd_getsig(&pipesig);


	if (Fwrite((short) fd, 1L, &ddmsg) != 1L)
	{
		gemtk_dd_close((short) fd);
		return(-1);
	}

	return((short) fd);
}



/*
*	Schreibt die 8 "bevorzugten" Extensionen der Applikation
*
* 	Eingabeparameter:
* 	fd		- 	Filehandle der D&D-Pipe
*	exts	-	Liste aus acht 4 Byte Extensionen die verstanden
*				werden. Diese Liste sollte nach bevorzugten Datentypen
*				sortiert sein. Sollten weniger als DD_NUMEXTS
*				Extensionen unterstÅtzt werden, muû die Liste mit
*				Nullen (0) aufgefÅllt werden!
*
* 	Ausgabeparameter:
*	keine
*
*	Returnwert:
* 	>0: kein Fehler
* 	-1: Fehler beim Schreiben in die Pipe
*/

short gemtk_dd_sexts(short fd, char *exts)
{
	if (Fwrite(fd, DD_EXTSIZE, exts) != DD_EXTSIZE)
	{
		gemtk_dd_close(fd);
		return(-1);
	}

	return(1);
}



/*
*	NÑchsten Header vom Sender holen
*
*	Eingabeparameter:
*	fd		-	Filehandle der Pipe (von gemtk_dd_open())
*
*	Ausgabeparameters:
*	name	-	Zeiger auf Buffer fÅr Datenbeschreibung (min. DD_NAMEMAX!)
*	file	-	Zeiger auf Buffer fÅr Datendateiname (min. DD_NAMEMAX!)
*	whichext-	Zeiger auf Buffer fÅr Extension (4 Bytes)
*	size	-	Zeiger auf Buffer fÅr Datengrîûe (4 Bytes)
*
*	Returnwert:
*	>0: kein Fehler
*	-1: Sender brach Drag&Drop ab
*
*	On lit dans le pipe qui normalement est constituÇ de:
*	1 short: taille du header
*	4 CHAR: type de donnÇe (extension)
*	1 long:	taille des donnÇes
*	STRING: description des donnÇes
*	STRING: nom du fichiers
*		soit au minimun 11 octets (cas ou les string sont rÇduites Ö \0)
*		les string sont limitÇ a 128 octets
*/

short gemtk_dd_rtry(short fd, char *name, char *file, char *whichext, long *size)
{
	char buf[DD_NAMEMAX * 2];
	short hdrlen, i, len;

	if (Fread(fd, 2L, &hdrlen) != 2L)
		return(-1);


	if (hdrlen < 9)	/* il reste au minimum 11 - 2 = 9 octets a lire */
	{
		/* sollte eigentlich nie passieren */

		return(-1);	/* erreur taille incorrecte */
	}

	if (Fread(fd, 4L, whichext) != 4L)	/* lecture de l'extension */
		return(-1);

	if (Fread(fd, 4L, size) != 4L)		/* lecture de la longueurs des donnÇes */
		return(-1);

	hdrlen -= 8;	/* on a lu 8 octets */

	if (hdrlen > DD_NAMEMAX*2)
		i = DD_NAMEMAX*2;
	else
		i = hdrlen;

	len = i;

	if (Fread(fd, (long) i, buf) != (long) i)
		return(-1);

	hdrlen -= i;

	strncpy(name, buf, DD_NAMEMAX);

	i = (short) strlen(name) + 1;

	if (len - i > 0)
		strncpy(file, buf + i, DD_NAMEMAX);
	else
		file[0] = '\0';


	/* weitere Bytes im Header in den MÅll */

	while (hdrlen > DD_NAMEMAX*2)
	{
		if (Fread(fd, DD_NAMEMAX*2, buf) != DD_NAMEMAX*2)
			return(-1);

		hdrlen -= DD_NAMEMAX*2;
	}

	if (hdrlen > 0)
	{
		if (Fread(fd, (long) hdrlen, buf) != (long) hdrlen)
			return(-1);
	}

	return(1);
}



/*
*	Sendet der Senderapplikation eine 1 Byte Antwort
*
*	Eingabeparameter:
*	fd	-	Filehandle der Pipe (von gemtk_dd_open())
*	ack	-	Byte das gesendet werden soll (zB. DD_OK)
*
*	Ausgabeparameter:
*	keine
*
*	Returnwert:
*	>0: kein Fehler
*	-1: Fehler (die Pipe wird automatisch geschlossen!)
*/

short gemtk_dd_reply(short fd, char ack)
{
	if (Fwrite(fd, 1L, &ack) != 1L)
	{
		gemtk_dd_close(fd);
		return(-1);
	}

	return(1);
}


