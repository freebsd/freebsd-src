/*
 *	Convert Phantasia 3.3.1 and 3.3.1+ characs file format to 3.3.2
 *
 */

#include "include.h"
#include "oldplayer.h"

struct oldplayer	Oldplayer;		/* old format structure */
struct player	Newplayer;			/* new format structure */

char	Oldpfile[] = DEST/characs";		/* old format file */
char	Newpfile[] = DEST/newcharacs";		/* new format file */

/************************************************************************
/
/ FUNCTION NAME: main()
/
/ FUNCTION: convert old Phantasia player file to new format
/
/ AUTHOR: C. Robertson, 9/1/85  E. A. Estes, 3/12/86
/
/ ARGUMENTS: none
/
/ RETURN VALUE: none
/
/ MODULES CALLED: time(), exit(), fread(), fopen(), srandom(), floor(), 
/	random(), strcmp(), fwrite(), strcpy(), fclose(), fprintf()
/
/ GLOBAL INPUTS: _iob[], Oldplayer, Newplayer
/
/ GLOBAL OUTPUTS: Oldplayer, Newplayer
/
/ DESCRIPTION:
/	Read in old player structures and write out to new file in
/	new format.
/	Old player file is unmodified.
/	New file is "DEST/newcharacs".
/	#define PHANTPLUS to convert from 3.3.1+.
/
/************************************************************************/

main()
{
FILE	*oldcharac, *newcharac;		/* to open old and new files */

    if ((oldcharac = fopen(Oldpfile, "r")) == NULL)
	{
	fprintf(stderr, "Cannot open original character file!\n");
	exit(1);
	}

    if ((newcharac = fopen(Newpfile, "w")) == NULL)
	{
	fprintf(stderr, "Cannot create new character file!\n");
	exit(1);
	}

    srandom((unsigned) time((long *) NULL));	/* prime random numbers */

    while (fread((char *) &Oldplayer, sizeof(struct oldplayer), 1, oldcharac) == 1)
	/* read and convert old structures into new */
	{
	Newplayer.p_experience = Oldplayer.o_experience;
	Newplayer.p_level = (double) Oldplayer.o_level;
	Newplayer.p_strength =  Oldplayer.o_strength;
	Newplayer.p_sword =  Oldplayer.o_sword;
	Newplayer.p_might =  0.0;		/* game will calculate */
	Newplayer.p_energy =  Oldplayer.o_energy;
	Newplayer.p_maxenergy =  Oldplayer.o_maxenergy;
	Newplayer.p_shield =  Oldplayer.o_shield;
	Newplayer.p_quickness = (double) Oldplayer.o_quickness;
	Newplayer.p_quksilver = (double) Oldplayer.o_quksilver;
	Newplayer.p_speed = 0.0;		/* game will calculate */
	Newplayer.p_magiclvl = Oldplayer.o_magiclvl;
	Newplayer.p_mana = Oldplayer.o_mana;
	Newplayer.p_brains = Oldplayer.o_brains;
	Newplayer.p_poison = Oldplayer.o_poison;
	Newplayer.p_gold = Oldplayer.o_gold;
	Newplayer.p_gems = Oldplayer.o_gems;
	Newplayer.p_sin = Oldplayer.o_sin;
	Newplayer.p_x = Oldplayer.o_x;
	Newplayer.p_y = Oldplayer.o_y;
	Newplayer.p_1scratch = Oldplayer.o_1scratch;
	Newplayer.p_2scratch = Oldplayer.o_2scratch;

	Newplayer.p_ring.ring_type = Oldplayer.o_ring.ring_type;
	Newplayer.p_ring.ring_duration = Oldplayer.o_ring.ring_duration;
	Newplayer.p_ring.ring_inuse = FALSE;

	Newplayer.p_age = (long) Oldplayer.o_degenerated * N_AGE;

	Newplayer.p_degenerated = Oldplayer.o_degenerated + 1;

	/* convert character type into character type and special type */

	if (Oldplayer.o_type < 0)
	    /* player with crown */
	    Oldplayer.o_type = -Oldplayer.o_type;

	if (Oldplayer.o_type == 99)
	    /* valar */
	    {
	    Newplayer.p_specialtype = SC_VALAR;
	    Newplayer.p_type = (short) ROLL(C_MAGIC, C_EXPER - C_MAGIC + 1);
	    Newplayer.p_lives = Oldplayer.o_ring.ring_duration;
	    }
	else if (Oldplayer.o_type == 90)
	    /* ex-valar */
	    {
	    Newplayer.p_specialtype = SC_EXVALAR;
	    Newplayer.p_type = (short) ROLL(C_MAGIC, C_EXPER - C_MAGIC + 1);
	    Newplayer.p_lives = 0;
	    }
	else if (Oldplayer.o_type > 20)
	    /* council of wise */
	    {
	    Newplayer.p_specialtype = SC_COUNCIL;
	    Newplayer.p_type = Oldplayer.o_type - 20;
	    Newplayer.p_lives = Oldplayer.o_ring.ring_duration;
	    }
	else if (Oldplayer.o_type > 10)
	    /* king */
	    {
	    Newplayer.p_specialtype = SC_KING;
	    Newplayer.p_type = Oldplayer.o_type - 10;
	    Newplayer.p_lives = 0;
	    }
	else
	    /* normal player */
	    {
	    Newplayer.p_specialtype = SC_NONE;
	    Newplayer.p_type = Oldplayer.o_type;
	    Newplayer.p_lives = 0;
	    }

	Newplayer.p_lives = 0;
	Newplayer.p_crowns = Oldplayer.o_crowns;
	Newplayer.p_charms = Oldplayer.o_charms;
	Newplayer.p_amulets = Oldplayer.o_amulets;
	Newplayer.p_holywater = Oldplayer.o_holywater;
	Newplayer.p_lastused = Oldplayer.o_lastused;

	/* convert status and name into status */

	Newplayer.p_status = Oldplayer.o_status + S_OFF;
	if (strcmp(Oldplayer.m_name, "<null>") == 0)
	    /* unused recored */
	    Newplayer.p_status = S_NOTUSED;
	if (Oldplayer.o_quickness < 0)
	    /* hung up player */
	    {
	    Newplayer.p_quickness = (double) Oldplayer.o_tampered;
	    Oldplayer.o_tampered = T_OFF;
	    Newplayer.p_status = S_HUNGUP;
	    }

	Newplayer.p_tampered = Oldplayer.o_tampered + T_OFF;
	Newplayer.p_istat = I_OFF;

	Newplayer.p_palantir = Oldplayer.o_palantir;
	Newplayer.p_blessing = Oldplayer.o_blessing;
	Newplayer.p_virgin = Oldplayer.o_virgin;
	Newplayer.p_blindness = Oldplayer.o_blindness;

	strcpy(Newplayer.p_name, Oldplayer.o_name);
	strcpy(Newplayer.p_password, Oldplayer.o_password);
	strcpy(Newplayer.p_login, Oldplayer.o_login);

	/* write new structure */
	fwrite((char *) &Newplayer, sizeof(Newplayer), 1, newcharac);
	}

    fclose(oldcharac);		/* close files */
    fclose(newcharac);

    exit(0);
}
/**/
/************************************************************************
/
/ FUNCTION NAME: drandom()
/
/ FUNCTION: return a random number between 0.0 < 1.0
/
/ AUTHOR: E. A. Estes, 2/7/86
/
/ ARGUMENTS: none
/
/ RETURN VALUE: random number
/
/ MODULES CALLED: random()
/
/ GLOBAL INPUTS: none
/
/ GLOBAL OUTPUTS: none
/
/ DESCRIPTION:
/	Return a random number.
/
/************************************************************************/

double
drandom()
{
    if (sizeof(int) != 2)
	return((double) (random() & 0x7fff) / 32768.0);
    else
	return((double) random() / 32768.0);
}
