/*audio.c--------------------------------------------------------------------

	Matsushita(Panasonic) / Creative CD-ROM Driver	(matcd)
	Authored by Frank Durda IV

	Copyright 1994, 1995  Frank Durda IV.  All rights reserved.
	"FDIV" is a trademark of Frank Durda IV.


	Redistribution and use in source and binary forms, with or
	without modification, are permitted provided that the following
	conditions are met:
	1.  Redistributions of source code must retain the above copyright
	    notice positioned at the very beginning of this file without
	    modification, all copyright strings, all related programming
	    codes that display the copyright strings, this list of
	    conditions and the following disclaimer.
	2.  Redistributions in binary form must contain all copyright strings
	    and related programming code that display the copyright strings.
	3.  Redistributions in binary form must reproduce the above copyright
	    notice, this list of conditions and the following disclaimer in
	    the documentation and/or other materials provided with the
	    distribution.
	4.  All advertising materials mentioning features or use of this
	    software must display the following acknowledgement:
		"The Matsushita/Panasonic CD-ROM driver  was developed
		 by Frank Durda IV for use with "FreeBSD" and similar
		 operating systems."
	    "Similar operating systems" includes mainly non-profit oriented
	    systems for research and education, including but not restricted
	    to "NetBSD", "386BSD", and "Mach" (by CMU).  The wording of the
	    acknowledgement (in electronic form or printed text) may not be
	    changed without permission from the author.
	5.  Absolutely no warranty of function, fitness or purpose is made
	    by the author Frank Durda IV.
	6.  Neither the name of the author nor the name "FreeBSD" may
	    be used to endorse or promote products derived from this software
	    without specific prior written permission.
	    (The author can be reached at   bsdmail@nemesis.lonestar.org)
	7.  The product containing this software must meet all of these
	    conditions even if it is unsupported, not a complete system
	    and/or does not contain compiled code.
	8.  These conditions will be in force for the full life of the
	    copyright.
	9.  If all the above conditions are met, modifications to other
	    parts of this file may be freely made, although any person
	    or persons making changes do not receive the right to add their
	    name or names to the copyright strings and notices in this
	    software.  Persons making changes are encouraged to insert edit
	    history in matcd.c and to put your name and details of the
	    change there.
	10. You must have prior written permission from the author to
	    deviate from these terms.

	Vendors who produce product(s) containing this code are encouraged
	(but not required) to provide copies of the finished product(s) to
	the author and to correspond with the author about development
	activity relating to this code.   Donations of development hardware
	and/or software are also welcome.  (This is one of the faster ways
	to get a driver developed for a device.)

 	THIS SOFTWARE IS PROVIDED BY THE DEVELOPER(S) ``AS IS'' AND ANY
 	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 	PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER(S) BE
 	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 	OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


-----No changes are allowed above this line------------------------------------

	The following functions are related to the audio playback
	capabilities of the drive.   They can be omitted from the
	finished driver using the FULLDRIVER conditional.

	The full set of features the drive is capable of are currently
	not implemented but will be added in upcoming releases.
-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
	matcd_playtracks - Plays one or more audio tracks
-----------------------------------------------------------------------------*/

static int matcd_playtracks(int ldrive, int cdrive, int controller,
			    struct ioc_play_track *pt)
{
	struct	matcd_data *cd;
	int	start,end;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	start=pt->start_track;
	end=pt->end_track;

	if (start < 1 ||		/*Starting track valid?*/
	    end < 1 ||			/*Ending track valid?*/
	    start > end || 		/*Start higher than end?*/
	    end > cd->volinfo.trk_high)	/*End track higher than disc size?*/
		return(ESPIPE);		/*Track out of range*/

	lockbus(controller, ldrive);	/*<16>Request bus*/
	i=matcd_setmode(ldrive, MODE_DA);/*Force drive into audio mode*/
	unlockbus(controller, ldrive);	/*<16>Release bus*/
	if (i!=0) {
		return(i);		/*Not legal for this media?*/
	}
	zero_cmd(cmd);
	cmd[0]=PLAYTRKS;		/*Play Audio Track/Index*/
	cmd[1]=start;
	cmd[2]=pt->start_index;
	cmd[3]=end;
	cmd[4]=pt->end_index;
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Play track results %d \n",ldrive,i);
#endif /*DEBUGIOCTL*/
	if (i==0) cd->status=CD_AS_PLAY_IN_PROGRESS;	/*<14>*/
	return(i);
}


/*-----------------------------------------------------------------------------
	matcd_playmsf - Plays between a range of blocks
-----------------------------------------------------------------------------*/

static int matcd_playmsf(int ldrive, int cdrive, int controller,
			    struct ioc_play_msf *pt)
{
	struct matcd_data *cd;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

#ifdef DEBUGIOCTL
	printf("matcd%d: playmsf %2x %2x %2x -> %2x %2x %2x\n",
	       ldrive,pt->start_m, pt->start_s, pt->start_f, pt->end_m,
	       pt->end_s,pt->end_f);
#endif /*DEBUGIOCTL*/

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	if ((cd->volinfo.vol_msf[0]==0 &&
	     cd->volinfo.vol_msf[1]<2) ||	/*Must be after 0'1"75F*/
	     msf_to_blk((char *)&pt->start_m) >
	     msf_to_blk((char *)&cd->volinfo.vol_msf)) {
#ifdef DEBUGIOCTL
	printf("matcd%d: Invalid block combination\n",ldrive);
#endif /*DEBUGIOCTL*/
		return(ESPIPE);		/*Track out of range*/
	}


	lockbus(controller, ldrive);	/*<16>Request bus*/
	i=matcd_setmode(ldrive, MODE_DA);/*Force drive into audio mode*/
	unlockbus(controller, ldrive);	/*<16>Release bus*/
	if (i!=0) {
		return(i);		/*Not legal for this media?*/
	}
	zero_cmd(cmd);
	cmd[0]=PLAYBLOCKS;		/*Play Audio Blocks*/
	cmd[1]=pt->start_m;
	cmd[2]=pt->start_s;
	cmd[3]=pt->start_f;
	cmd[4]=pt->end_m;
	cmd[5]=pt->end_s;
	cmd[6]=pt->end_f;
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
	if (i==0) cd->status=CD_AS_PLAY_IN_PROGRESS;	/*<14>*/
	return(i);
}


/*-----------------------------------------------------------------------------
	matcd_pause - Pause or Resume audio playback
-----------------------------------------------------------------------------*/

static int matcd_pause(int ldrive, int cdrive, int controller, int action)
{
	struct	matcd_data *cd;
	int	i,z,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	zero_cmd(cmd);
	cmd[0]=NOP;			/*<14>Just find out whats going on*/

	lockbus(controller, ldrive);	/*<16>Request bus*/
	matcd_slowcmd(port,ldrive,cdrive,cmd);	/*<14>*/
	i=waitforit(10*TICKRES,DTEN,port,"matpau");	/*<25>*/
	z=get_stat(port,ldrive);	/*<14>Read status byte*/
	if ((z & MATCD_ST_ERROR)) {	/*<14>Something went wrong*/
		i=get_error(port, ldrive, cdrive);	/*<14>*/
		unlockbus(controller, ldrive);	/*<16>Release bus*/
		return(EIO);		/*<14>*/
	}				/*<14>*/
	unlockbus(controller, ldrive);	/*<16>Release bus*/

	if (z & MATCD_ST_AUDIOBSY==0 &&	/*<14>If drive is idle*/
	    cd->status==CD_AS_PLAY_IN_PROGRESS) {	/*<14>but was playing*/
		cd->status=CD_AS_PLAY_COMPLETED;	/*<14>then its done*/
		return(0);
	}

	if (action) {			/*<14>Set state for subq ioctl*/
#ifndef KRYTEN
		if (cd->status==CD_AS_PLAY_IN_PROGRESS) {/*<14>Don't resume*/
			return(0);	/*<14>if already playing*/
		}			/*<14>Max Headroom sound occurs*/
#endif /*KRYTEN*/
		cd->status=CD_AS_PLAY_IN_PROGRESS;	/*<14>to read*/
	} else {			/*<14>There is no way to ask the*/
		cd->status=CD_AS_PLAY_PAUSED;/*<14>drive if it is paused*/
	}				/*<14>*/

	cmd[0]=PAUSE;			/*Pause or Resume playing audio*/
	cmd[1]=action;
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Pause / Resume results %d \n",ldrive,i);
#endif /*DEBUGIOCTL*/
	return(i);
}



/*-----------------------------------------------------------------------------
	matcd_stop  - Stop audio playback
-----------------------------------------------------------------------------*/

static int matcd_stop(int ldrive, int cdrive, int controller)
{
	struct	matcd_data *cd;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	zero_cmd(cmd);
	cmd[0]=ABORT;			/*Abort playing audio*/
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Abort results %d \n",ldrive,i);
#endif /*DEBUGIOCTL*/
	cd->status=CD_AS_PLAY_COMPLETED;/*<14>the drive if it is paused*/
	return(i);
}


/*-----------------------------------------------------------------------------
	matcd_level - Read or set the audio levels
<12>	New for Edit 12
-----------------------------------------------------------------------------*/

static int matcd_level(int ldrive, int cdrive, int controller,
		       struct ioc_vol * level, unsigned long action)
{
	struct	matcd_data *cd;
	int	i,z,port;
	unsigned char c;
	unsigned char cmd[MAXCMDSIZ];
	unsigned char data[5];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	zero_cmd(cmd);
	if (action==CDIOCSETVOL) {	/*We are setting new volume settings*/

/*	Here we set the volume levels.  Note that the same command
	also sets the patching (routing) of audio, so we have to rely
	on previously-stored settings to fill in these fields.
*/
		cmd[0]=MODESELECT;	/*Write drive settings*/
		cmd[1]=AUDIOPARM;	/*Audio/routing settings*/

/*	Although the drive allows a left and right channel volume to be
	specified separately, the drive refuses the settings if the
	values are different.
*/
		c=level->vol[0] | level->vol[1];	/*Or them together*/

		cmd[4]=cd->volume[0]=c;	/*Channel 0 (Left) volume*/
		cmd[6]=cd->volume[1]=c;	/*Channel 1 (Right) volume*/
		cmd[3]=cd->patch[0];	/*Channel 0 (Left)  patching*/
		cmd[5]=cd->patch[1];	/*Channel 1 (Right)  patching*/
		i=docmd(cmd,ldrive,cdrive,controller,port);/*Issue cmd*/
#ifdef DEBUGIOCTL
		printf("matcd%d: Volume set %d\n",ldrive,i);
#endif /*DEBUGIOCTL*/
		return(i);
	} else {			/*Read existing settings*/


/*	This code reads the settings for the drive back - note that
	volume and patching are both returned so we have to keep
	both internally.
*/
		cmd[0]=MODESENSE;	/*Read drive settings*/
		cmd[1]=AUDIOPARM;	/*Audio/routing settings*/
		lockbus(controller, ldrive);	/*<16>Request bus*/
		matcd_slowcmd(port,ldrive,cdrive,cmd);
		i=waitforit(10*TICKRES,DTEN,port,"matlvl");	/*<25>*/
		matcd_pread(port, 5, data);	/*Read data returned*/
		z=get_stat(port,ldrive);/*Read status byte*/
		unlockbus(controller, ldrive);	/*<16>Release bus*/
#ifdef DEBUGIOCTL
		printf("matcd%d: Data got was %x %x %x %x %x   ",ldrive,
		       data[0],data[1],data[2], data[3],data[4]);
		printf("status byte %x\n",z);
#endif	/*DEBUGIOCTL*/
		cd->volume[0]=level->vol[0]=	/*Channel 0 (Left) volume*/
			      data[2];
		cd->volume[1]=level->vol[1]=	/*Channel 1 (Right) volume*/
			      data[4];
		level->vol[2]=level->vol[3]=0;	/*Channel 2 & 3 not avail*/

		cd->patch[0]=data[1];	/*Channel 0 (Left) patching*/
		cd->patch[1]=data[3];	/*Channel 1 (Right) patching*/

		return(0);
	}

}


/*-----------------------------------------------------------------------------
	matcd_routing - Set the audio routing (patching)
<12>	New for Edit 12
-----------------------------------------------------------------------------*/

static int matcd_route(int ldrive, int cdrive, int controller,
		       unsigned long command)
{
	struct	matcd_data *cd;
	int	i,port;
	unsigned char l,r;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	zero_cmd(cmd);
	switch (command) {
	case	CDIOCSETMUTE:
		l=r=0;
		break;

	case	CDIOCSETLEFT:
		l=r=OUTLEFT;
		break;

	case	CDIOCSETRIGHT:
		l=r=OUTRIGHT;
		break;

	default:
	case	CDIOCSETSTEREO:
		l=OUTLEFT;
		r=OUTRIGHT;
		break;
	}

/*	Here we set the volume levels.  Note that the same command
	also sets the patching (routing) of audio, so we have to rely
	on previously-stored settings to fill in these fields.
*/
	cmd[0]=MODESELECT;		/*Write drive settings*/
	cmd[1]=AUDIOPARM;		/*Audio/routing settings*/


/*	Although the drive allows a left and right channel volume to be
	specified separately, the drive refuses the settings if the
	values are different.
*/
	cmd[4]=cd->volume[0];		/*Channel 0 (Left) volume*/
	cmd[6]=cd->volume[1];		/*Channel 1 (Right) volume*/
	cmd[3]=cd->patch[0]=l;		/*Channel 0 (Left)  patching*/
	cmd[5]=cd->patch[1]=r;		/*Channel 1 (Right)  patching*/
	i=docmd(cmd,ldrive,cdrive,controller,port);/*Issue cmd*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Routing set %d\n",ldrive,i);
#endif /*DEBUGIOCTL*/
	return(i);

}


/*-----------------------------------------------------------------------------
	matcd_patch - Set the audio routing (patching)
<12>	New for Edit 12
-----------------------------------------------------------------------------*/

static int matcd_patch(int ldrive, int cdrive, int controller,
		       struct ioc_patch * routing)
{
	struct	matcd_data *cd;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	zero_cmd(cmd);

/*	Here we set the volume levels.  Note that the same command
	also sets the patching (routing) of audio, so we have to rely
	on previously-stored settings to fill in these fields.
*/
	cmd[0]=MODESELECT;		/*Write drive settings*/
	cmd[1]=AUDIOPARM;		/*Audio/routing settings*/


/*	Although the drive allows a left and right channel volume to be
	specified separately, the drive refuses the settings if the
	values are different.
*/
	cmd[4]=cd->volume[0];		/*Channel 0 (Left) volume*/
	cmd[6]=cd->volume[1];		/*Channel 1 (Right) volume*/
	cmd[3]=cd->patch[0]=		/*Channel 0 (Left)  patching*/
	       (routing->patch[0] & 0x03);
	cmd[5]=cd->patch[1]=		/*Channel 1 (Right)  patching*/
	       (routing->patch[1] & 0x03);
	i=docmd(cmd,ldrive,cdrive,controller,port);/*Issue cmd*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Routing set %d\n",ldrive,i);
#endif /*DEBUGIOCTL*/
	return(i);

}

/*-----------------------------------------------------------------------------
	matcd_pitch - Change audio playback rate
		      Apart from making things sound funny, the only
		      other application might be Karaoke.  Ugh.
<12>	New for Edit 12
-----------------------------------------------------------------------------*/

static int matcd_pitch(int ldrive, int cdrive, int controller,
		       struct ioc_pitch * speed)
{
	struct	matcd_data *cd;
	short	i;
	int	z,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	zero_cmd(cmd);

/*	This function sets the audio playback rate.  In SCSI devices this is
	referred to as the logical block addresses per second parameter.
	Uh huh.  Sounds like they didn't want anyone to find it.
	Anyway, a study found that no one else has implemented this ioctl
	but the capability does exist in the SCSI standard so I am following
	the SCSI scheme even though it really doesn't fit this drive well.

	I define the parameter to this ioctl as -32767 to -1 being
	"play slower", 0x0000 flat and 1 to 32767 being "play faster"
	within the scale allowed by the device.  The value is scaled to fit
	the allowed by the device and any excess is treated as being
	the positive or negative limit.  No ioctl input value is considered
	invalid.

	This device has a +/- 13% playback pitch specified by a range
	-130 to +130.  The drive does a hard enforcement on this.

	SCSI defines a 16 bit LBAS count, and a "multiplier" that
	is either x1 or x(1/256).  The Matsushita drive only provides
	10 bits total for indicating pitch so the LSbits are discarded.
*/

	cmd[0]=MODESELECT;		/*Write drive settings*/
	cmd[1]=SPEEDPARM;		/*Audio speed settings*/

	i=speed->speed>>7;		/*Scale down to our usable range*/

	if (i!=0) {			/*Real pitch value*/
		if (i < -130) i=-130;	/*Force into range we support*/
		else if (i > 130) i=130;
		cmd[3]=((i>>8)&0x03) | 0x04;	/*Get upper bits*/
		cmd[4]=(i & 0xff);	/*Set lower bits*/
	}
	z=docmd(cmd,ldrive,cdrive,controller,port);/*Issue cmd*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Pitch set %d\n",ldrive,i);
#endif /*DEBUGIOCTL*/
	return(z);
}

/*End of audio.c*/

