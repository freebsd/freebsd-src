#!/usr/sbin/dtrace -s
/*
 * woof.d - Bark whenever new processes appear. Needs /dev/audio.
 *          Written in DTrace (Solaris 10 3/05).
 *
 * $Id: woof.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:       woof.d &
 *
 * SEE ALSO:    /usr/dt/bin/sdtaudiocontrol     # to set volume
 *
 * COPYRIGHT: Copyright (c) 2006 Brendan Gregg.
 *
 * CDDL HEADER START
 *
 *  The contents of this file are subject to the terms of the
 *  Common Development and Distribution License, Version 1.0 only
 *  (the "License").  You may not use this file except in compliance
 *  with the License.
 *
 *  You can obtain a copy of the license at Docs/cddl1.txt
 *  or http://www.opensolaris.org/os/licensing.
 *  See the License for the specific language governing permissions
 *  and limitations under the License.
 *
 * CDDL HEADER END
 *
 * 14-Aug-2006	Brendan Gregg	Created this.
 * 14-Aug-2006	   "      "	Last update.
 */

#pragma D option quiet
#pragma D option destructive
#pragma D option switchrate=10hz

inline int SCREEN_OUTPUT = 0;	/* Set to 1 for screen output */

/* barks prevents woof.d from barking too much (up to 20 barks/second) */
int barks;

dtrace:::BEGIN
{
	SCREEN_OUTPUT ? trace("Beware of the dog!\n") : 1;
}

/*
 * Call the shell to run a background audioplay command (cat > /dev/audio
 * doesn't always work). One problem this creates is a feedback loop,
 * where we bark at our own barks, or at other dogs barks; entertaining
 * as this is, it can really slog the system and has been avoided by
 * checking our ancestory.
 */
proc:::exec-success
/!progenyof($pid) && barks++ < 2/
{
	SCREEN_OUTPUT ? trace("Woof! ") : 1;
	system("audioplay /usr/share/audio/samples/au/bark.au &");
}

profile:::tick-10hz
{
	barks = 0;
}
