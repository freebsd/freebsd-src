/* cu.h
   Header file for cu.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

/* The user settable variables supported by cu.  */

/* The escape character used to introduce a special command.  The
   escape character is the first character of this string.  */
extern const char *zCuvar_escape;

/* Whether to delay for a second before printing the host name after
   seeing an escape character.  */
extern boolean fCuvar_delay;

/* The input characters which finish a line.  The escape character is
   only recognized following one of these characters.  */
extern const char *zCuvar_eol;

/* Whether to transfer binary data (nonprintable characters other than
   newline and tab) when sending a file.  If this is FALSE, then
   newline is changed to carriage return.  */
extern boolean fCuvar_binary;

/* A prefix string to use before sending a binary character from a
   file; this is only used if fCuvar_binary is TRUE.  */
extern const char *zCuvar_binary_prefix;

/* Whether to check for echoes of characters sent when sending a file.
   This is ignored if fCuvar_binary is TRUE.  */
extern boolean fCuvar_echocheck;

/* A character to look for after each newline is sent when sending a
   file.  The character is the first character in this string, except
   that a '\0' means that no echo check is done.  */
extern const char *zCuvar_echonl;

/* The timeout to use when looking for an character.  */
extern int cCuvar_timeout;

/* The character to use to kill a line if an echo check fails.  The
   first character in this string is sent.  */
extern const char *zCuvar_kill;

/* The number of times to try resending a line if the echo check keeps
   failing.  */
extern int cCuvar_resend;

/* The string to send at the end of a file sent with ~>.  */
extern const char *zCuvar_eofwrite;

/* The string to look for to finish a file received with ~<.  For tip
   this is a collection of single characters, but I don't want to do
   that because it means that there are characters which cannot be
   received.  */
extern const char *zCuvar_eofread;

/* Whether to provide verbose information when sending or receiving a
   file.  */
extern boolean fCuvar_verbose;
