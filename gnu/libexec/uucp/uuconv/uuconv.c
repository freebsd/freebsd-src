/* uuconv.c
   Convert one type of UUCP configuration file to another.

   Copyright (C) 1991, 1992 Ian Lance Taylor

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

#include "uucnfi.h"

#if USE_RCS_ID
const char uuconv_rcsid[] = "$Id: uuconv.c,v 1.1 1993/08/05 18:27:29 conklin Exp $";
#endif

#include "getopt.h"

/* Local functions.  */

static void uvusage P((void));
static void uvwrite_time P((FILE *e, struct uuconf_timespan *qtime));
static void uvwrite_string P((FILE *e, const char *zarg, const char *zcmd));
static void uvwrite_size P((FILE *e, struct uuconf_timespan *qsize,
			    const char *zcmd));
static void uvwrite_boolean P((FILE *e, int iarg, const char *zcmd));
static void uvwrite_string_array P((FILE *e, char **pz, const char *zcmd));
static void uvwrite_chat_script P((FILE *e, char **pz));
static void uvwrite_chat P((FILE *e, const struct uuconf_chat *qchat,
			    const struct uuconf_chat *qlast,
			    const char *zprefix, boolean fforce));
static void uvwrite_proto_params P((FILE *e,
				    const struct uuconf_proto_param *qparam,
				    const char *zprefix));
static void uvwrite_taylor_system P((FILE *e,
				     const struct uuconf_system *qsys));
static void uvwrite_v2_system P((FILE *e,
				 const struct uuconf_system *qsys));
static void uvwrite_hdb_system P((FILE *e,
				  const struct uuconf_system *qsys));
static boolean fvperm_string_cmp P((const char *z1, const char *z2));
static boolean fvperm_array_cmp P((const char **pz1, const char **pz2));
static void uvadd_perm P((struct shpermissions *qadd));
static void uvwrite_perms P((void));
static void uvwrite_perm_array P((FILE *e, const char **pz,
				  const char *zcmd, size_t *pccol));
static void uvwrite_perm_boolean P((FILE *e, int f, const char *zcmd,
				    size_t *pccol, boolean fsendfiles));
static void uvwrite_perm_rw_array P((FILE *e, const char **pz,
				     const char *zcmd, size_t *pccol));
static void uvwrite_perm_string P((FILE *e, const char *z, const char *zcmd,
				   size_t *pccol));
static int ivwrite_taylor_port P((struct uuconf_port *qport,
				  pointer pinfo));
static int ivwrite_v2_port P((struct uuconf_port *qport, pointer pinfo));
static int ivwrite_hdb_port P((struct uuconf_port *qport, pointer pinfo));
static void uvwrite_taylor_port P((FILE *e, struct uuconf_port *qport,
				   const char *zprefix));
static void uvwrite_taylor_dialer P((FILE *e, struct uuconf_dialer *qdialer,
				     const char *zprefix));
static void uvwrite_hdb_dialer P((FILE *e, struct uuconf_dialer *qdialer));
static void uvuuconf_error P((pointer puuconf, int iret));

/* A list of Permissions entries built when writing out HDB system
   information.  */
static struct shpermissions *qVperms;

/* Type of configuration file.  */
enum tconfig
{
  CONFIG_TAYLOR,
  CONFIG_V2,
  CONFIG_HDB
};

/* Long getopt options.  */
static const struct option asVlongopts[] = { { NULL, 0, NULL, 0 } };

int
main (argc, argv)
     int argc;
     char **argv;
{
  /* -I: The configuration file name.  */
  const char *zconfig = NULL;
  /* -i: Input type.  */
  const char *zinput = NULL;
  /* -o: Output type.  */
  const char *zoutput = NULL;
  /* -p: Program name.  */
  const char *zprogram = NULL;
  int iopt;
  enum tconfig tinput, toutput;
  int iret;
  pointer pinput;

  while ((iopt = getopt_long (argc, argv, "i:I:o:p:x:", asVlongopts,
			      (int *) NULL)) != EOF)
    {
      switch (iopt)
	{
	case 'i':
	  /* Input type.  */
	  zinput = optarg;
	  break;

	case 'o':
	  /* Output type.  */
	  zoutput = optarg;
	  break;

	case 'p':
	  /* Program name.  */
	  zprogram = optarg;
	  break;

	case 'I':
	  /* Set the configuration file name.  */
	  zconfig = optarg;
	  break;

	case 'x':
	  /* Set the debugging level.  */
	  break;

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  uvusage ();
	  break;
	}
    }

  if (optind != argc
      || zinput == NULL
      || zoutput == NULL)
    uvusage ();

  if (strcasecmp (zinput, "taylor") == 0)
    tinput = CONFIG_TAYLOR;
  else if (strcasecmp (zinput, "v2") == 0)
    tinput = CONFIG_V2;
  else if (strcasecmp (zinput, "hdb") == 0)
    tinput = CONFIG_HDB;
  else
    {
      uvusage ();
      tinput = CONFIG_TAYLOR;
    }

  if (strcasecmp (zoutput, "taylor") == 0)
    toutput = CONFIG_TAYLOR;
  else if (strcasecmp (zoutput, "v2") == 0)
    toutput = CONFIG_V2;
  else if (strcasecmp (zoutput, "hdb") == 0)
    toutput = CONFIG_HDB;
  else
    {
      uvusage ();
      toutput = CONFIG_TAYLOR;
    }

  if (tinput == toutput)
    uvusage ();

  iret = UUCONF_SUCCESS;

  /* Initialize the input.  */
  pinput = NULL;
  switch (tinput)
    {
    case CONFIG_TAYLOR:
      iret = uuconf_taylor_init (&pinput, zprogram, zconfig);
      break;
    case CONFIG_V2:
      iret = uuconf_v2_init (&pinput);
      break;
    case CONFIG_HDB:
      iret = uuconf_hdb_init (&pinput, zprogram);
      break;
    }
  if (iret != UUCONF_SUCCESS)
    {
      uvuuconf_error (pinput, iret);
      exit (EXIT_FAILURE);
    }

  {
    char **pzsystems;
    char *zsys;
    char abtaylor[sizeof ZCURDIR + sizeof SYSFILE - 1];
    char abv2[sizeof ZCURDIR + sizeof V2_SYSTEMS - 1];
    char abhdb[sizeof ZCURDIR + sizeof HDB_SYSTEMS - 1];
    FILE *esys;
    char **pz;

    /* Get the list of systems.  */
    switch (tinput)
      {
      case CONFIG_TAYLOR:
	iret = uuconf_taylor_system_names (pinput, &pzsystems, FALSE);
	break;
      case CONFIG_V2:
	iret = uuconf_v2_system_names (pinput, &pzsystems, FALSE);
	break;
      case CONFIG_HDB:
	iret = uuconf_hdb_system_names (pinput, &pzsystems, FALSE);
	break;
      }
    if (iret != UUCONF_SUCCESS)
      uvuuconf_error (pinput, iret);
    else
      {
	/* Open the sys file for the output type.  */
	switch (toutput)
	  {
	  default:
	  case CONFIG_TAYLOR:
	    sprintf (abtaylor, "%s%s", ZCURDIR, SYSFILE);
	    zsys = abtaylor;
	    break;
	  case CONFIG_V2:
	    sprintf (abv2, "%s%s", ZCURDIR, V2_SYSTEMS);
	    zsys = abv2;
	    break;
	  case CONFIG_HDB:
	    sprintf (abhdb, "%s%s", ZCURDIR, HDB_SYSTEMS);
	    zsys = abhdb;
	    break;
	  }
	esys = fopen (zsys, "w");
	if (esys == NULL)
	  {
	    fprintf (stderr, "uuchk:%s: ", zsys);
	    perror ("fopen");
	    exit (EXIT_FAILURE);
	  }

	fprintf (esys, "# %s file automatically generated by uuconv.\n",
		 zsys);

	/* Read and write each system.  We cheat and call the internal
	   routines, so that we can easily detect default information and
	   not write it out.  This isn't necessary, but it makes the output
	   smaller and simpler.  */
	for (pz = pzsystems; *pz != NULL; pz++)
	  {
	    struct uuconf_system ssys;

	    switch (tinput)
	      {
	      case CONFIG_TAYLOR:
		iret = _uuconf_itaylor_system_internal (pinput, *pz, &ssys);
		break;
	      case CONFIG_V2:
		iret = _uuconf_iv2_system_internal (pinput, *pz, &ssys);
		break;
	      case CONFIG_HDB:
		iret = _uuconf_ihdb_system_internal (pinput, *pz, &ssys);
		break;
	      }
	    if (iret != UUCONF_SUCCESS)
	      uvuuconf_error (pinput, iret);
	    else
	      {
		switch (toutput)
		  {
		  case CONFIG_TAYLOR:
		    uvwrite_taylor_system (esys, &ssys);
		    break;
		  case CONFIG_V2:
		    uvwrite_v2_system (esys, &ssys);
		    break;
		  case CONFIG_HDB:
		    uvwrite_hdb_system (esys, &ssys);
		    break;
		  }
		if (toutput != CONFIG_HDB)
		  (void) uuconf_system_free (pinput, &ssys);
	      }
	  }

	if (toutput == CONFIG_HDB)
	  uvwrite_perms ();

	if (ferror (esys)
	    || fclose (esys) == EOF)
	  {
	    fprintf (stderr, "uuchk:%s: error during output\n", zsys);
	    exit (EXIT_FAILURE);
	  }
      }
  }

  {
    /* Open the port file for the output type.  */
    char *zport;
    char abtaylor[sizeof ZCURDIR + sizeof PORTFILE - 1];
    char abv2[sizeof ZCURDIR + sizeof V2_DEVICES - 1];
    char abhdb[sizeof ZCURDIR + sizeof HDB_DEVICES - 1];
    FILE *eport;
    int (*piportfn) P((struct uuconf_port *, pointer));
    struct uuconf_port sport;

    switch (toutput)
      {
      default:
      case CONFIG_TAYLOR:
	sprintf (abtaylor, "%s%s", ZCURDIR, PORTFILE);
	zport = abtaylor;
	piportfn = ivwrite_taylor_port;
	break;
      case CONFIG_V2:
	sprintf (abv2, "%s%s", ZCURDIR, V2_DEVICES);
	zport = abv2;
	piportfn = ivwrite_v2_port;
	break;
      case CONFIG_HDB:
	sprintf (abhdb, "%s%s", ZCURDIR, HDB_DEVICES);
	zport = abhdb;
	piportfn = ivwrite_hdb_port;
	break;
      }
    eport = fopen (zport, "w");
    if (eport == NULL)
      {
	fprintf (stderr, "uuchk:%s: ", zport);
	perror ("fopen");
	exit (EXIT_FAILURE);
      }

    fprintf (eport, "# %s file automatically generated by uuconv.\n", zport);

    switch (tinput)
      {
      case CONFIG_TAYLOR:
	iret = uuconf_taylor_find_port (pinput, (const char *) NULL, 0L,
					0L, piportfn, (pointer) eport,
					&sport);
	break;
      case CONFIG_V2:
	iret = uuconf_v2_find_port (pinput, (const char *) NULL, 0L, 0L,
				    piportfn, (pointer) eport, &sport);
	break;
      case CONFIG_HDB:
	iret = uuconf_hdb_find_port (pinput, (const char *) NULL, 0L, 0L,
				     piportfn, (pointer) eport, &sport);
	break;
      }
	
    if (iret != UUCONF_NOT_FOUND)
      uvuuconf_error (pinput, iret);

    if (ferror (eport)
	|| fclose (eport) == EOF)
      {
	fprintf (stderr, "uuchk:%s: error during output\n", zport);
	exit (EXIT_FAILURE);
      }
  }

  /* V2 configuration files don't support dialers.  */
  if (tinput != CONFIG_V2 && toutput != CONFIG_V2)
    {
      char **pzdialers;
      char *zdialer;
      char abtaylor[sizeof ZCURDIR + sizeof DIALFILE - 1];
      char abhdb[sizeof ZCURDIR + sizeof HDB_DIALERS - 1];
      FILE *edialer;
      char **pz;

      /* Get the list of dialers.  */
      switch (tinput)
	{
	default:
	case CONFIG_TAYLOR:
	  iret = uuconf_taylor_dialer_names (pinput, &pzdialers);
	  break;
	case CONFIG_HDB:
	  iret = uuconf_hdb_dialer_names (pinput, &pzdialers);
	  break;
	}
      if (iret != UUCONF_SUCCESS)
	uvuuconf_error (pinput, iret);
      else
	{
	  /* Open the sys file for the output type.  */
	  switch (toutput)
	    {
	    default:
	    case CONFIG_TAYLOR:
	      sprintf (abtaylor, "%s%s", ZCURDIR, DIALFILE);
	      zdialer = abtaylor;
	      break;
	    case CONFIG_HDB:
	      sprintf (abhdb, "%s%s", ZCURDIR, HDB_DIALERS);
	      zdialer = abhdb;
	      break;
	    }
	  edialer = fopen (zdialer, "w");
	  if (edialer == NULL)
	    {
	      fprintf (stderr, "uuchk:%s: ", zdialer);
	      perror ("fopen");
	      exit (EXIT_FAILURE);
	    }

	  fprintf (edialer, "# %s file automatically generated by uuconv.\n",
		   zdialer);

	  /* Read and write each dialer.  */
	  for (pz = pzdialers; *pz != NULL; pz++)
	    {
	      struct uuconf_dialer sdialer;

	      switch (tinput)
		{
		default:
		case CONFIG_TAYLOR:
		  iret = uuconf_taylor_dialer_info (pinput, *pz, &sdialer);
		  break;
		case CONFIG_HDB:
		  iret = uuconf_hdb_dialer_info (pinput, *pz, &sdialer);
		  break;
		}
	      if (iret != UUCONF_SUCCESS)
		uvuuconf_error (pinput, iret);
	      else
		{
		  switch (toutput)
		    {
		    default:
		    case CONFIG_TAYLOR:
		      fprintf (edialer, "# Start of dialer %s\n",
			       sdialer.uuconf_zname);
		      fprintf (edialer, "dialer %s\n", sdialer.uuconf_zname);
		      uvwrite_taylor_dialer (edialer, &sdialer, "");
		      break;
		    case CONFIG_HDB:
		      uvwrite_hdb_dialer (edialer, &sdialer);
		      break;
		    }
		  (void) uuconf_dialer_free (pinput, &sdialer);
		}
	    }

	  if (ferror (edialer)
	      || fclose (edialer) == EOF)
	    {
	      fprintf (stderr, "uuchk:%s: error during output\n", zdialer);
	      exit (EXIT_FAILURE);
	    }
	}
    }

  exit (EXIT_SUCCESS);
}

/* Print out a usage message and die.  */

static void
uvusage ()
{
  fprintf (stderr,
	   "Taylor UUCP version %s, copyright (C) 1991, 1992 Ian Lance Taylor\n",
	   VERSION);
  fprintf (stderr,
	   "Usage: uuconv -i input -o output [-p program] [-I file]\n");
  fprintf (stderr,
	   " -i input: Set input type (one of taylor, v2, hdb)\n");
  fprintf (stderr,
	   " -o output: Set output type (one of taylor, v2, hdb)\n");
  fprintf (stderr,
	   " -p program: Program to convert (e.g., uucp or cu)\n");
  fprintf (stderr,
	   " -I file: Set Taylor UUCP configuration file to use\n");
  exit (EXIT_FAILURE);
}

/* Write out a timespan.  */

static void
uvwrite_time (e, qtime)
     FILE *e;
     struct uuconf_timespan *qtime;
{
  if (qtime == NULL)
    {
      fprintf (e, "Never");
      return;
    }

  if (qtime->uuconf_istart == 0 && qtime->uuconf_iend == 7 * 24 * 60)
    {
      fprintf (e, "Any");
      return;
    }

  for (; qtime != NULL; qtime = qtime->uuconf_qnext)
    {
      int idaystart, idayend;
      int ihourstart, ihourend;
      int iminutestart, iminuteend;
      const char * const zdays = "Su\0Mo\0Tu\0We\0Th\0Fr\0Sa";

      idaystart = qtime->uuconf_istart / (24 * 60);
      ihourstart = (qtime->uuconf_istart % (24 * 60)) / 60;
      iminutestart = qtime->uuconf_istart % 60;
      if (qtime->uuconf_iend >= 7 * 24 * 60)
	qtime->uuconf_iend = 7 * 24 * 60 - 1;
      idayend = qtime->uuconf_iend / (24 * 60);
      ihourend = (qtime->uuconf_iend % (24 * 60)) / 60;
      iminuteend = qtime->uuconf_iend % 60;
      if (ihourend == 0 && iminuteend == 0)
	--idayend;

      if (idaystart == idayend)
	fprintf (e, "%s%02d%02d-%02d%02d", zdays + idaystart * 3,
		 ihourstart, iminutestart, ihourend, iminuteend);
      else
	{
	  int i;

	  fprintf (e, "%s%02d%02d-0000", zdays + idaystart * 3,
		   ihourstart, iminutestart);
	  for (i = idaystart + 1; i < idayend; i++)
	    fprintf (e, ",%s", zdays + i * 3);
	  if (ihourend != 0 || iminuteend != 0)
	    fprintf (e, ",%s0000-%02d%02d", zdays + idayend * 3, ihourend,
		     iminuteend);
	}

      if (qtime->uuconf_qnext != NULL)
	fprintf (e, ",");
    }
}

/* Some subroutines used when writing out Taylor UUCP configuration
   files.  */

/* Write a command with a string argument.  */

static void
uvwrite_string (e, zarg, zcmd)
     FILE *e;
     const char *zarg;
     const char *zcmd;
{
  if (zarg != (const char *) &_uuconf_unset)
    fprintf (e, "%s %s\n", zcmd, zarg == NULL ? (const char *) "" : zarg);
}

/* Write out a size restriction command.  */

static void
uvwrite_size (e, qtime, zcmd)
     FILE *e;
     struct uuconf_timespan *qtime;
     const char *zcmd;
{
  if (qtime != (struct uuconf_timespan *) &_uuconf_unset)
    {
      for (; qtime != NULL; qtime = qtime->uuconf_qnext)
	{
	  fprintf (e, "%s %ld", zcmd, qtime->uuconf_ival);
	  uvwrite_time (e, qtime);
	  fprintf (e, "\n");
	}
    }
}

/* Write out a boolean argument with a string command.  If the value
   is less than zero, than it was uninitialized and we don't write
   anything.  */

static void
uvwrite_boolean (e, fval, zcmd)
     FILE *e;
     int fval;
     const char *zcmd;
{
  if (fval >= 0)
    fprintf (e, "%s %s\n", zcmd, fval > 0 ? "true" : "false");
}

/* Write out a string array as a single command.  */

static void
uvwrite_string_array (e, pz, zcmd)
     FILE *e;
     char **pz;
     const char *zcmd;
{
  if (pz != (char **) &_uuconf_unset)
    {
      fprintf (e, "%s", zcmd);
      if (pz != NULL)
	for (; *pz != NULL; pz++)
	  fprintf (e, " %s", *pz);
      fprintf (e, "\n");
    }
}

/* Write out a chat script.  Don't separate subsend/subexpect strings
   by spaces.  */

static void
uvwrite_chat_script (e, pzarg)
     FILE *e;
     char **pzarg;
{
  char **pz;

  if (pzarg == NULL || pzarg == (char **) &_uuconf_unset)
    return;

  for (pz = pzarg; *pz != NULL; pz++)
    {
      if ((*pz)[0] != '-' && pz != pzarg)
	fprintf (e, " ");
      fprintf (e, *pz);
    }
}

/* Write out chat information.  If the qlast argument is not NULL,
   then only values that are different from qlast should be written.
   The fforce argument is used to get around a peculiar problem: if
   the ``chat'' command is used with no arguments for a system, then
   uuconf_pzchat will be NULL (not &_uuconf_unset) and the default
   chat script will not be used.  We must distinguish this case from
   the ``chat'' command not appearing at all for a port or dialer, in
   which case the value will again be NULL.  In the former case we
   must output a ``chat'' command, in the latter case we would prefer
   not to.  */

static void
uvwrite_chat (e, q, qlast, zprefix, fforce)
     FILE *e;
     const struct uuconf_chat *q;
     const struct uuconf_chat *qlast;
     const char *zprefix;
     boolean fforce;
{
  char **pz;
  char ab[100];

  if (q->uuconf_pzchat != (char **) &_uuconf_unset
      && (qlast == NULL
	  ? (fforce || q->uuconf_pzchat != NULL)
	  : qlast->uuconf_pzchat != q->uuconf_pzchat))
    {
      fprintf (e, "%schat ", zprefix);
      uvwrite_chat_script (e, q->uuconf_pzchat);
      fprintf (e, "\n");
    }

  if (q->uuconf_pzprogram != (char **) &_uuconf_unset
      && (qlast == NULL
	  ? q->uuconf_pzprogram != NULL
	  : qlast->uuconf_pzprogram != q->uuconf_pzprogram))
    {
      sprintf (ab, "%schat-program", zprefix);
      uvwrite_string_array (e, q->uuconf_pzprogram, ab);
    }

  if (q->uuconf_ctimeout >= 0
      && (qlast == NULL
	  || qlast->uuconf_ctimeout != q->uuconf_ctimeout))
    fprintf (e, "%schat-timeout %d\n", zprefix, q->uuconf_ctimeout);

  if (q->uuconf_pzfail != NULL
      && q->uuconf_pzfail != (char **) &_uuconf_unset
      && (qlast == NULL
	  || qlast->uuconf_pzfail != q->uuconf_pzfail))
    for (pz = q->uuconf_pzfail; *pz != NULL; pz++)
      fprintf (e, "%schat-fail %s\n", zprefix, *pz);
      
  if (qlast == NULL || qlast->uuconf_fstrip != q->uuconf_fstrip)
    {
      sprintf (ab, "%schat-strip", zprefix);
      uvwrite_boolean (e, q->uuconf_fstrip, ab);
    }
}

/* Write out protocol parameters to a Taylor UUCP file.  */

static void
uvwrite_proto_params (e, qparams, zprefix)
     FILE *e;
     const struct uuconf_proto_param *qparams;
     const char *zprefix;
{
  const struct uuconf_proto_param *qp;

  if (qparams == NULL
      || qparams == (struct uuconf_proto_param *) &_uuconf_unset)
    return;

  for (qp = qparams; qp->uuconf_bproto != '\0'; qp++)
    {
      const struct uuconf_proto_param_entry *qe;

      for (qe = qp->uuconf_qentries; qe->uuconf_cargs > 0; qe++)
	{
	  int i;

	  fprintf (e, "%sprotocol-parameter %c", zprefix, qp->uuconf_bproto);
	  for (i = 0; i < qe->uuconf_cargs; i++)
	    fprintf (e, " %s", qe->uuconf_pzargs[i]);
	  fprintf (e, "\n");
	}
    }
}

/* Write out Taylor UUCP system information.  */

static void
uvwrite_taylor_system (e, q)
     FILE *e;
     const struct uuconf_system *q;
{
  char **pz;
  const struct uuconf_system *qlast;

  fprintf (e, "# Start of system %s\n", q->uuconf_zname);

  fprintf (e, "system %s\n", q->uuconf_zname);
  if (q->uuconf_pzalias != NULL
      && q->uuconf_pzalias != (char **) &_uuconf_unset)
    for (pz = q->uuconf_pzalias; *pz != NULL; pz++)
      uvwrite_string (e, *pz, "alias");

  for (qlast = NULL; q != NULL; qlast = q, q = q->uuconf_qalternate)
    {
      struct uuconf_timespan *qtime;

      if (qlast != NULL)
	{
	  fprintf (e, "alternate");
	  if (q->uuconf_zalternate != (char *) &_uuconf_unset
	      && q->uuconf_zalternate != NULL)
	    fprintf (e, " %s", q->uuconf_zalternate);
	  fprintf (e, "\n");
	}

#define CHANGED(x) (qlast == NULL || qlast->x != q->x)

      if (CHANGED (uuconf_qtimegrade)
	  && (q->uuconf_qtimegrade
	      != (struct uuconf_timespan *) &_uuconf_unset))
	{
	  if (q->uuconf_qtimegrade == NULL)
	    fprintf (e, "time never\n");
	  else
	    {
	      for (qtime = q->uuconf_qtimegrade;
		   qtime != NULL;
		   qtime = qtime->uuconf_qnext)
		{
		  if ((char) qtime->uuconf_ival == UUCONF_GRADE_LOW)
		    fprintf (e, "time ");
		  else
		    fprintf (e, "timegrade %c ", (char) qtime->uuconf_ival);
		  uvwrite_time (e, qtime);
		  if (qtime->uuconf_cretry != 0)
		    fprintf (e, " %d", qtime->uuconf_cretry);
		  fprintf (e, "\n");
		}
	    }
	}

      if (CHANGED (uuconf_qcalltimegrade)
	  && (q->uuconf_qcalltimegrade
	      != (struct uuconf_timespan *) &_uuconf_unset))
	{
	  for (qtime = q->uuconf_qcalltimegrade;
	       qtime != NULL;
	       qtime = qtime->uuconf_qnext)
	    {
	      fprintf (e, "call-timegrade %c ", (char) qtime->uuconf_ival);
	      uvwrite_time (e, qtime);
	      fprintf (e, "\n");
	    }
	}

      if (CHANGED (uuconf_qcall_local_size))
	uvwrite_size (e, q->uuconf_qcall_local_size, "call-local-size");

      if (CHANGED (uuconf_qcall_remote_size))
	uvwrite_size (e, q->uuconf_qcall_remote_size, "call-remote-size");

      if (CHANGED (uuconf_qcalled_local_size))
	uvwrite_size (e, q->uuconf_qcalled_local_size, "called-local-size");

      if (CHANGED (uuconf_qcalled_remote_size))
	uvwrite_size (e, q->uuconf_qcalled_remote_size, "called-remote-size");

      if (CHANGED (uuconf_ibaud) || CHANGED (uuconf_ihighbaud))
	{
	  if (q->uuconf_ibaud >= 0)
	    {
	      if (q->uuconf_ihighbaud > 0)
		fprintf (e, "baud-range %ld %ld\n", q->uuconf_ibaud,
			 q->uuconf_ihighbaud);
	      else
		fprintf (e, "baud %ld\n", q->uuconf_ibaud);
	    }
	}

      if (CHANGED (uuconf_zport) || CHANGED (uuconf_qport))
	{
	  if (q->uuconf_zport != NULL
	      && q->uuconf_zport != (char *) &_uuconf_unset)
	    uvwrite_string (e, q->uuconf_zport, "port");
	  else if (q->uuconf_qport != NULL
		   && (q->uuconf_qport
		       != (struct uuconf_port *) &_uuconf_unset))
	    uvwrite_taylor_port (e, q->uuconf_qport, "port ");
	}

      if (CHANGED (uuconf_zphone))
	{
	  const char *zcmd;

	  if (q->uuconf_qport != NULL
	      && q->uuconf_qport != (struct uuconf_port *) &_uuconf_unset
	      && (q->uuconf_qport->uuconf_ttype == UUCONF_PORTTYPE_TCP
		  || q->uuconf_qport->uuconf_ttype == UUCONF_PORTTYPE_TLI))
	    zcmd = "address";
	  else
	    zcmd = "phone";
	  uvwrite_string (e, q->uuconf_zphone, zcmd);
	}

      uvwrite_chat (e, &q->uuconf_schat,
		    (qlast == NULL
		     ? (struct uuconf_chat *) NULL
		     : &qlast->uuconf_schat),
		    "", TRUE);

      if (CHANGED (uuconf_zcall_login))
	uvwrite_string (e, q->uuconf_zcall_login, "call-login");

      if (CHANGED (uuconf_zcall_password))
	uvwrite_string (e, q->uuconf_zcall_password, "call-password");

      if (CHANGED (uuconf_zcalled_login))
	uvwrite_string (e, q->uuconf_zcalled_login, "called-login");

      if (CHANGED (uuconf_fcallback))
	uvwrite_boolean (e, q->uuconf_fcallback, "callback");

      if (CHANGED (uuconf_fsequence))
	uvwrite_boolean (e, q->uuconf_fsequence, "sequence");

      if (CHANGED (uuconf_zprotocols))
	uvwrite_string (e, q->uuconf_zprotocols, "protocol");

      if (CHANGED (uuconf_qproto_params))
	uvwrite_proto_params (e, q->uuconf_qproto_params, "");
      
      uvwrite_chat (e, &q->uuconf_scalled_chat,
		    (qlast == NULL
		     ? (struct uuconf_chat *) NULL
		     : &qlast->uuconf_scalled_chat),
		    "called-", FALSE);

      if (CHANGED (uuconf_zdebug))
	uvwrite_string (e, q->uuconf_zdebug, "debug");

      if (CHANGED (uuconf_zmax_remote_debug))
	uvwrite_string (e, q->uuconf_zmax_remote_debug, "max-remote-debug");

      if ((CHANGED (uuconf_fsend_request)
	   || CHANGED (uuconf_frec_request))
	  && (q->uuconf_fsend_request >= 0
	      || q->uuconf_frec_request >= 0))
	{
	  if (q->uuconf_fsend_request >= 0
	      && (q->uuconf_fsend_request > 0
		  ? q->uuconf_frec_request > 0
		  : q->uuconf_frec_request == 0))
	    uvwrite_boolean (e, q->uuconf_fsend_request, "request");
	  else
	    {
	      uvwrite_boolean (e, q->uuconf_fsend_request, "send-request");
	      uvwrite_boolean (e, q->uuconf_frec_request,
			       "receive-request");
	    }
	}

      if ((CHANGED (uuconf_fcall_transfer)
	   || CHANGED (uuconf_fcalled_transfer))
	  && (q->uuconf_fcall_transfer >= 0
	      || q->uuconf_fcalled_transfer >= 0))
	{
	  if (q->uuconf_fcall_transfer >= 0
	      && (q->uuconf_fcall_transfer > 0
		  ? q->uuconf_fcalled_transfer > 0
		  : q->uuconf_fcalled_transfer == 0))
	    uvwrite_boolean (e, q->uuconf_fcall_transfer, "transfer");
	  else
	    {
	      uvwrite_boolean (e, q->uuconf_fcall_transfer, "call-transfer");
	      uvwrite_boolean (e, q->uuconf_fcalled_transfer,
			       "called-transfer");
	    }
	}

      if (CHANGED (uuconf_pzlocal_send))
	uvwrite_string_array (e, q->uuconf_pzlocal_send, "local-send");

      if (CHANGED (uuconf_pzremote_send))
	uvwrite_string_array (e, q->uuconf_pzremote_send, "remote-send");

      if (CHANGED (uuconf_pzlocal_receive))
	uvwrite_string_array (e, q->uuconf_pzlocal_receive, "local-receive");

      if (CHANGED (uuconf_pzremote_receive))
	uvwrite_string_array (e, q->uuconf_pzremote_receive,
			      "remote-receive");

      if (CHANGED (uuconf_pzpath))
	uvwrite_string_array (e, q->uuconf_pzpath, "command-path");

      if (CHANGED (uuconf_pzcmds))
	uvwrite_string_array (e, q->uuconf_pzcmds, "commands");

      if (CHANGED (uuconf_cfree_space)
	  && q->uuconf_cfree_space >= 0)
	fprintf (e, "free-space %ld\n", q->uuconf_cfree_space);

      if (CHANGED (uuconf_pzforward_from))
	uvwrite_string_array (e, q->uuconf_pzforward_from, "forward-from");

      if (CHANGED (uuconf_pzforward_to))
	uvwrite_string_array (e, q->uuconf_pzforward_to, "forward-to");

      if (CHANGED (uuconf_zpubdir))
	uvwrite_string (e, q->uuconf_zpubdir, "pubdir");

      if (CHANGED (uuconf_zlocalname))
	uvwrite_string (e, q->uuconf_zlocalname, "myname");
    }
}

/* Write out V2 system information.  */

static void
uvwrite_v2_system (e, q)
     FILE *e;
     const struct uuconf_system *q;
{
  for (; q != NULL; q = q->uuconf_qalternate)
    {
      fprintf (e, "%s", q->uuconf_zname);

      if (q->uuconf_qtimegrade != (struct uuconf_timespan *) &_uuconf_unset)
	{
	  fprintf (e, " ");
	  uvwrite_time (e, q->uuconf_qtimegrade);

	  if (q->uuconf_zport != (char *) &_uuconf_unset
	      || q->uuconf_qport != (struct uuconf_port *) &_uuconf_unset)
	    {
	      struct uuconf_port *qp;
	      boolean ftcp;

	      qp = q->uuconf_qport;
	      ftcp = (qp != (struct uuconf_port *) &_uuconf_unset
		      && qp != NULL
		      && qp->uuconf_ttype == UUCONF_PORTTYPE_TCP);
	      if (ftcp
		  || (q->uuconf_zport != NULL
		      && q->uuconf_zport != (char *) &_uuconf_unset))
		{
		  if (ftcp)
		    fprintf (e, " TCP");
		  else
		    fprintf (e, " %s", q->uuconf_zport);

		  if (ftcp || q->uuconf_ibaud >= 0)
		    {
		      fprintf (e, " ");
		      if (ftcp)
			{
			  const char *zport;

			  zport = qp->uuconf_u.uuconf_stcp.uuconf_zport;
			  if (zport == NULL)
			    zport = "uucp";
			  fprintf (e, "%s", zport);
			}
		      else
			fprintf (e, "%ld", q->uuconf_ibaud);

		      if (q->uuconf_zphone != (char *) &_uuconf_unset
			  && q->uuconf_zphone != NULL)
			{
			  char **pzc;
			  
			  fprintf (e, " %s", q->uuconf_zphone);
			  pzc = q->uuconf_schat.uuconf_pzchat;
			  if (pzc != (char **) &_uuconf_unset
			      && pzc != NULL)
			    {
			      fprintf (e, " ");
			      uvwrite_chat_script (e, pzc);
			    }
			}
		    }
		}
	    }
	}

      fprintf (e, "\n");

      /* Here we should gather information to write out to USERFILE
	 and L.cmds, and perhaps some day we will.  It's much more
	 likely to happen if somebody else does it, though.  */
    }
}

/* Write out HDB system information.  */

static void
uvwrite_hdb_system (e, qsys)
     FILE *e;
     const struct uuconf_system *qsys;
{
  const struct uuconf_system *q;
  struct shpermissions sperm;
  char *azmachine[2];
  char *azlogname[2];

  for (q = qsys; q != NULL; q = q->uuconf_qalternate)
    {
      if (q->uuconf_fcall)
	{
	  fprintf (e, "%s", q->uuconf_zname);

	  if (q->uuconf_qtimegrade
	      != (struct uuconf_timespan *) &_uuconf_unset)
	    {
	      const char *zport;

	      fprintf (e, " ");
	      uvwrite_time (e, q->uuconf_qtimegrade);

	      zport = q->uuconf_zport;
	      if (q->uuconf_qport != NULL
		  && q->uuconf_qport != (struct uuconf_port *) &_uuconf_unset
		  && q->uuconf_qport->uuconf_ttype == UUCONF_PORTTYPE_TCP)
		zport = "TCP";
	      if (zport != NULL && zport != (char *) &_uuconf_unset)
		{
		  fprintf (e, " %s", zport);
		  if (q->uuconf_zprotocols != (char *) &_uuconf_unset
		      && q->uuconf_zprotocols != NULL)
		    fprintf (e, ",%s", q->uuconf_zprotocols);

		  if (q->uuconf_ibaud >= 0
		      || q->uuconf_zphone != (char *) &_uuconf_unset)
		    {
		      fprintf (e, " ");
		      if (q->uuconf_ibaud < 0)
			fprintf (e, "Any");
		      else
			{
			  fprintf (e, "%ld", q->uuconf_ibaud);
			  if (q->uuconf_ihighbaud >= 0)
			    fprintf (e, "-%ld", q->uuconf_ihighbaud);
			}

		      if (q->uuconf_zphone != (char *) &_uuconf_unset
			  && q->uuconf_zphone != NULL)
			{
			  char **pzc;
			  
			  fprintf (e, " %s", q->uuconf_zphone);
			  pzc = q->uuconf_schat.uuconf_pzchat;
			  if (pzc != (char **) &_uuconf_unset
			      && pzc != NULL)
			    {
			      fprintf (e, " ");
			      uvwrite_chat_script (e, pzc);
			    }
			}
		    }
		}
	    }

	  fprintf (e, "\n");
	}
    }

  /* Build a Permissions entry for this system.  There will be only
     one MACHINE entry for a given system.  */

  for (q = qsys; q != NULL; q = q->uuconf_qalternate)
    if (q->uuconf_fcall)
      break;

  if (q != NULL)
    {
      sperm.qnext = NULL;
      sperm.pzlogname = NULL;
      sperm.pzmachine = NULL;
      sperm.frequest = -1;
      sperm.fsendfiles = -1;
      sperm.pzread = NULL;
      sperm.pzwrite = NULL;
      sperm.fcallback = -1;
      sperm.pzcommands = NULL;
      sperm.pzvalidate = NULL;
      sperm.zmyname = NULL;
      sperm.zpubdir = NULL;
      sperm.pzalias = NULL;

      azmachine[0] = q->uuconf_zname;
      azmachine[1] = NULL;
      sperm.pzmachine = azmachine;
      if (q->uuconf_fsend_request >= 0)
	sperm.frequest = q->uuconf_fsend_request;
      if (q->uuconf_pzremote_send != (char **) &_uuconf_unset
	  && q->uuconf_pzremote_send != NULL)
	sperm.pzread = q->uuconf_pzremote_send;
      if (q->uuconf_pzremote_receive != (char **) &_uuconf_unset
	  && q->uuconf_pzremote_receive != NULL)
	sperm.pzwrite = q->uuconf_pzremote_receive;
      if (q->uuconf_pzcmds != (char **) &_uuconf_unset
	  && q->uuconf_pzcmds != NULL)
	sperm.pzcommands = q->uuconf_pzcmds;
      if (q->uuconf_zlocalname != (char *) &_uuconf_unset
	  && q->uuconf_zlocalname != NULL)
	sperm.zmyname = q->uuconf_zlocalname;
      if (q->uuconf_zpubdir != (char *) &_uuconf_unset
	  && q->uuconf_zpubdir != NULL)
	sperm.zpubdir = q->uuconf_zpubdir;
      if (q->uuconf_pzalias != (char **) &_uuconf_unset
	  && q->uuconf_pzalias != NULL)
	sperm.pzalias = q->uuconf_pzalias;

      if (q->uuconf_fcalled
	  && q->uuconf_zcalled_login != (char *) &_uuconf_unset
	  && q->uuconf_zcalled_login != NULL)
	{
	  azlogname[0] = q->uuconf_zcalled_login;
	  azlogname[1] = NULL;
	  sperm.pzlogname = azlogname;
	  if (q->uuconf_fcalled_transfer >= 0)
	    sperm.fsendfiles = q->uuconf_fcalled_transfer;
	  if (q->uuconf_fcallback >= 0)
	    sperm.fcallback = q->uuconf_fcallback;
	  sperm.pzvalidate = azmachine;
	}

      uvadd_perm (&sperm);
    }

  /* Now add a Permissions entry for each alternative that is not used
     for calling out.  */
  for (q = qsys; q != NULL; q = q->uuconf_qalternate)
    {
      if (! q->uuconf_fcalled || q->uuconf_fcall)
	continue;

      sperm.qnext = NULL;
      sperm.pzlogname = NULL;
      sperm.pzmachine = NULL;
      sperm.frequest = -1;
      sperm.fsendfiles = -1;
      sperm.pzread = NULL;
      sperm.pzwrite = NULL;
      sperm.fcallback = -1;
      sperm.pzcommands = NULL;
      sperm.pzvalidate = NULL;
      sperm.zmyname = NULL;
      sperm.zpubdir = NULL;
      sperm.pzalias = NULL;

      if (q->uuconf_zcalled_login != (char *) &_uuconf_unset
	  && q->uuconf_zcalled_login != NULL)
	azlogname[0] = q->uuconf_zcalled_login;
      else
	azlogname[0] = (char *) "OTHER";
      azlogname[1] = NULL;
      sperm.pzlogname = azlogname;

      if (q->uuconf_fsend_request >= 0)
	sperm.frequest = q->uuconf_fsend_request;
      if (q->uuconf_fcalled_transfer >= 0)
	sperm.fsendfiles = q->uuconf_fcalled_transfer;
      if (q->uuconf_pzremote_send != (char **) &_uuconf_unset
	  && q->uuconf_pzremote_send != NULL)
	sperm.pzread = q->uuconf_pzremote_send;
      if (q->uuconf_pzremote_receive != (char **) &_uuconf_unset
	  && q->uuconf_pzremote_receive != NULL)
	sperm.pzwrite = q->uuconf_pzremote_receive;
      if (q->uuconf_fcallback >= 0)
	sperm.fcallback = q->uuconf_fcallback;
      if (q->uuconf_zlocalname != (char *) &_uuconf_unset
	  && q->uuconf_zlocalname != NULL)
	sperm.zmyname = q->uuconf_zlocalname;
      if (q->uuconf_zpubdir != (char *) &_uuconf_unset
	  && q->uuconf_zpubdir != NULL)
	sperm.zpubdir = q->uuconf_zpubdir;

      uvadd_perm (&sperm);
    }
}

/* Compare two strings from a Permissions entry, returning TRUE if
   they are the same.  */
static boolean
fvperm_string_cmp (z1, z2)
     const char *z1;
     const char *z2;
{
  if (z1 == NULL
      ? z2 != NULL
      : z2 == NULL)
    return FALSE;

  if (z1 == NULL)
    return TRUE;

  return strcmp (z1, z2) == 0;
}

/* Compare two arrays of strings from a Permissions entry, returning
   TRUE if they are the same.  */

static boolean
fvperm_array_cmp (pz1, pz2)
     const char **pz1;
     const char **pz2;
{
  if (pz1 == NULL
      ? pz2 != NULL
      : pz2 == NULL)
    return FALSE;

  if (pz1 == NULL)
    return TRUE;

  for (; *pz1 != NULL && *pz2 != NULL; pz1++, pz2++)
    if (strcmp (*pz1, *pz2) != 0)
      break;

  return *pz1 == NULL && *pz2 == NULL;
}      

/* Add a Permissions entry to a global list, combining entries where
   possible.  */

static void
uvadd_perm (qadd)
     struct shpermissions *qadd;
{
  struct shpermissions *qlook;
  struct shpermissions *qnew;
  int iret;

  /* If there's no information, don't bother to add this entry.  */
  if (qadd->pzlogname == NULL
      && qadd->frequest < 0
      && qadd->fsendfiles < 0
      && qadd->pzread == NULL
      && qadd->pzwrite == NULL
      && qadd->fcallback < 0
      && qadd->pzcommands == NULL
      && qadd->pzvalidate == NULL
      && qadd->zmyname == NULL
      && qadd->zpubdir == NULL
      && qadd->pzalias == NULL)
    return;

  for (qlook = qVperms; qlook != NULL; qlook = qlook->qnext)
    {
      /* See if we can merge qadd into qlook.  */
      if (qadd->pzlogname == NULL
	  ? qlook->pzlogname != NULL
	  : qlook->pzlogname == NULL)
	continue;
      if (qadd->pzmachine == NULL
	  ? qlook->pzmachine != NULL
	  : qlook->pzmachine == NULL)
	continue;
      if (qadd->frequest != qlook->frequest
	  || qadd->fsendfiles != qlook->fsendfiles
	  || qadd->fcallback != qlook->fcallback)
	continue;
      if (! fvperm_string_cmp (qadd->zmyname, qlook->zmyname)
	  || ! fvperm_string_cmp (qadd->zpubdir, qlook->zpubdir))
	continue;
      if (! fvperm_array_cmp ((const char **) qadd->pzread,
			      (const char **) qlook->pzread)
	  || ! fvperm_array_cmp ((const char **)  qadd->pzwrite,
				 (const char **) qlook->pzwrite)
	  || ! fvperm_array_cmp ((const char **) qadd->pzcommands,
				 (const char **) qlook->pzcommands))
	continue;

      /* Merge qadd into qlook.  */
      if (qadd->pzmachine != NULL)
	{
	  iret = _uuconf_iadd_string ((struct sglobal *) NULL,
				      qadd->pzmachine[0], FALSE,
				      TRUE, &qlook->pzmachine,
				      (pointer) NULL);
	  if (iret != UUCONF_SUCCESS)
	    uvuuconf_error ((pointer) NULL, iret);
	}
      if (qadd->pzlogname != NULL)
	{
	  iret = _uuconf_iadd_string ((struct sglobal *) NULL,
				      qadd->pzlogname[0], FALSE,
				      TRUE, &qlook->pzlogname,
				      (pointer) NULL);
	  if (iret != UUCONF_SUCCESS)
	    uvuuconf_error ((pointer) NULL, iret);
	}
      if (qadd->pzalias != NULL)
	{
	  char **pz;

	  for (pz = qadd->pzalias; *pz != NULL; pz++)
	    {
	      iret = _uuconf_iadd_string ((struct sglobal *) NULL,
					  *pz, FALSE, TRUE,
					  &qlook->pzalias, (pointer) NULL);
	      if (iret != UUCONF_SUCCESS)
		uvuuconf_error ((pointer) NULL, iret);
	    }
	}

      return;
    }

  /* We must add qadd as a new entry on the list, which means we must
     copy it into the heap.  */

  qnew = (struct shpermissions *) malloc (sizeof (struct shpermissions));
  if (qnew == NULL)
    uvuuconf_error ((pointer) NULL, UUCONF_MALLOC_FAILED);
  *qnew = *qadd;
  if (qadd->pzmachine != NULL)
    {
      qnew->pzmachine = NULL;
      iret = _uuconf_iadd_string ((struct sglobal *) NULL,
				  qadd->pzmachine[0], FALSE,
				  FALSE, &qnew->pzmachine,
				  (pointer) NULL);
      if (iret != UUCONF_SUCCESS)
	uvuuconf_error ((pointer) NULL, iret);
    }
  if (qadd->pzlogname != NULL)
    {
      qnew->pzlogname = NULL;
      iret = _uuconf_iadd_string ((struct sglobal *) NULL,
				  qadd->pzlogname[0], FALSE,
				  FALSE, &qnew->pzlogname,
				  (pointer) NULL);
      if (iret != UUCONF_SUCCESS)
	uvuuconf_error ((pointer) NULL, iret);
    }
  if (qadd->pzvalidate != NULL)
    qnew->pzvalidate = qnew->pzmachine;

  qnew->qnext = qVperms;
  qVperms = qnew;
}

/* Write out the Permissions entries.  */

static void
uvwrite_perms ()
{
  char ab[sizeof ZCURDIR + sizeof HDB_PERMISSIONS - 1];
  FILE *e;
  struct shpermissions *q;

  sprintf (ab, "%s%s", ZCURDIR, HDB_PERMISSIONS);
  e = fopen (ab, "w");
  if (e == NULL)
    {
      fprintf (stderr, "uuchk:%s: ", ab);
      perror ("fopen");
      exit (EXIT_FAILURE);
    }

  fprintf (e, "# Permissions file automatically generated by uuconv.\n");

  for (q = qVperms; q != NULL; q = q->qnext)
    {
      size_t ccol;

      ccol = 0;
      uvwrite_perm_array (e, (const char **) q->pzlogname, "LOGNAME", &ccol);
      uvwrite_perm_array (e, (const char **) q->pzmachine, "MACHINE", &ccol);
      uvwrite_perm_boolean (e, q->frequest, "REQUEST", &ccol, FALSE);
      uvwrite_perm_boolean (e, q->fsendfiles, "SENDFILES", &ccol, TRUE);
      uvwrite_perm_rw_array (e, (const char **) q->pzread, "READ", &ccol);
      uvwrite_perm_rw_array (e, (const char **) q->pzwrite, "WRITE", &ccol);
      uvwrite_perm_boolean (e, q->fcallback, "CALLBACK", &ccol, FALSE);
      uvwrite_perm_array (e, (const char **) q->pzcommands, "COMMANDS",
			  &ccol);
      uvwrite_perm_array (e, (const char **) q->pzvalidate, "VALIDATE",
			  &ccol);
      uvwrite_perm_string (e, q->zmyname, "MYNAME", &ccol);
      uvwrite_perm_string (e, q->zpubdir, "PUBDIR", &ccol);
      uvwrite_perm_array (e, (const char **) q->pzalias, "ALIAS", &ccol);

      fprintf (e, "\n");
    }

  if (ferror (e)
      || fclose (e) == EOF)
    {
      fprintf (stderr, "uuchk:%s: error during output\n", HDB_PERMISSIONS);
      exit (EXIT_FAILURE);
    }
}

/* Write an array out to the Permissions file.  */

static void
uvwrite_perm_array (e, pzarg, zcmd, pccol)
     FILE *e;
     const char **pzarg;
     const char *zcmd;
     size_t *pccol;
{
  size_t c;
  const char **pz;

  if (pzarg == NULL)
    return;

  c = strlen (zcmd) + 1;
  
  for (pz = pzarg; *pz != NULL; pz++)
    c += strlen (*pz) + 1;

  if (*pccol > 20 && c + *pccol > 75)
    {
      fprintf (e, " \\\n");
      *pccol = c - 1;
    }
  else
    {
      if (*pccol != 0)
	fprintf (e, " ");
      *pccol += c;
    }

  fprintf (e, "%s=", zcmd);
  for (pz = pzarg; *pz != NULL; pz++)
    {
      if (pz != pzarg)
	fprintf (e, ":");
      fprintf (e, "%s", *pz);
    }
}

/* Write a boolean value out to the Permissions file.  This may be
   either a yes/no boolean or a yes/call boolean (the latter is for
   SENDFILES).  */

static void
uvwrite_perm_boolean (e, f, zcmd, pccol, fsendfiles)
     FILE *e;
     int f;
     const char *zcmd;
     size_t *pccol;
     boolean fsendfiles;
{
  const char *az[2];

  if (f < 0)
    return;

  if (f)
    az[0] = "yes";
  else
    az[0] = fsendfiles ? "call" : "no";
  az[1] = NULL;

  uvwrite_perm_array (e, az, zcmd, pccol);
}

/* Write a set of READ or WRITE entries to the Permissions file.  We
   have to separate out all entries that start with '!'.  */

static void
uvwrite_perm_rw_array (e, pzarg, zcmd, pccol)
     FILE *e;
     const char **pzarg;
     const char *zcmd;
     size_t *pccol;
{
  size_t c;
  const char **pz, **pzcopy, **pzset;

  if (pzarg == NULL)
    return;

  c = 0;
  for (pz = pzarg; *pz != NULL; pz++)
    c++;

  pzcopy = (const char **) malloc ((c + 1) * sizeof (char *));
  if (pzcopy == NULL)
    uvuuconf_error ((pointer) NULL, UUCONF_MALLOC_FAILED);

  pzset = pzcopy;
  for (pz = pzarg; *pz != NULL; pz++)
    if ((*pz)[0] != '!')
      *pzset++ = *pz;
  *pzset = NULL;

  if (pzset != pzcopy)
    uvwrite_perm_array (e, (const char **) pzcopy, zcmd, pccol);

  pzset = pzcopy;
  for (pz = pzarg; *pz != NULL; pz++)
    if ((*pz)[0] == '!')
      *pzset++ = *pz;
  *pzset = NULL;

  if (pzset != pzcopy)
    {
      char ab[20];

      sprintf (ab, "NO%s", zcmd);
      uvwrite_perm_array (e, (const char **) pzcopy, ab, pccol);
    }
}

/* Write a string out to the Permissions file.  */

static void
uvwrite_perm_string (e, z, zcmd, pccol)
     FILE *e;
     const char *z;
     const char *zcmd;
     size_t *pccol;
{
  const char *az[2];

  if (z == NULL)
    return;

  az[0] = z;
  az[1] = NULL;

  uvwrite_perm_array (e, az, zcmd, pccol);
}

/* Write out a Taylor UUCP port.  This is called via uuconf_find_port;
   the pinfo argument is the port file.  */

static int
ivwrite_taylor_port (qport, pinfo)
     struct uuconf_port *qport;
     pointer pinfo;
{
  FILE *e = (FILE *) pinfo;

  fprintf (e, "port %s\n", qport->uuconf_zname);

  uvwrite_taylor_port (e, qport, "");

  /* Return UUCONF_NOT_FOUND to force uuconf_find_port to keep looking
     for ports.  */
  return UUCONF_NOT_FOUND;
}

/* Write a port out to a Taylor UUCP configuration file.  This doesn't
   output the name, since it is called to output a specially defined
   port in the sys file.  */

static void
uvwrite_taylor_port (e, qport, zprefix)
     FILE *e;
     struct uuconf_port *qport;
     const char *zprefix;
{
  const char *ztype;
  char ab[100];

  switch (qport->uuconf_ttype)
    {
    default:
    case UUCONF_PORTTYPE_UNKNOWN:
      fprintf (stderr, "uuconv: Bad port type\n");
      exit (EXIT_FAILURE);
      break;
    case UUCONF_PORTTYPE_STDIN:
      ztype = "stdin";
      break;
    case UUCONF_PORTTYPE_MODEM:
      ztype = "modem";
      break;
    case UUCONF_PORTTYPE_DIRECT:
      ztype = "direct";
      break;
    case UUCONF_PORTTYPE_TCP:
      ztype = "tcp";
      break;
    case UUCONF_PORTTYPE_TLI:
      ztype = "tli";
      break;
    }

  fprintf (e, "%stype %s\n", zprefix, ztype);

  if (qport->uuconf_zprotocols != NULL)
    fprintf (e, "%sprotocol %s\n", zprefix, qport->uuconf_zprotocols);

  if (qport->uuconf_qproto_params != NULL)
    uvwrite_proto_params (e, qport->uuconf_qproto_params, zprefix);

  if ((qport->uuconf_ireliable & UUCONF_RELIABLE_SPECIFIED) != 0)
    {
      sprintf (ab, "%sseven-bit", zprefix);
      uvwrite_boolean (e,
		       ((qport->uuconf_ireliable & UUCONF_RELIABLE_EIGHT)
			== 0),
		       ab);
      sprintf (ab, "%sreliable", zprefix);
      uvwrite_boolean (e,
		       ((qport->uuconf_ireliable & UUCONF_RELIABLE_RELIABLE)
			!= 0),
		       ab);
      sprintf (ab, "%shalf-duplex", zprefix);
      uvwrite_boolean (e,
		       ((qport->uuconf_ireliable & UUCONF_RELIABLE_FULLDUPLEX)
			== 0),
		       ab);
    }

  if (qport->uuconf_zlockname != NULL)
    fprintf (e, "%slockname %s\n", zprefix, qport->uuconf_zlockname);

  switch (qport->uuconf_ttype)
    {
    default:
      break;
    case UUCONF_PORTTYPE_MODEM:
      {
	struct uuconf_modem_port *qm;

	qm = &qport->uuconf_u.uuconf_smodem;
	if (qm->uuconf_zdevice != NULL)
	  fprintf (e, "%sdevice %s\n", zprefix, qm->uuconf_zdevice);
	if (qm->uuconf_zdial_device != NULL)
	  fprintf (e, "%sdial-device %s\n", zprefix, qm->uuconf_zdial_device);
	if (qm->uuconf_ibaud != 0)
	  fprintf (e, "%sbaud %ld\n", zprefix, qm->uuconf_ibaud);
	if (qm->uuconf_ilowbaud != 0)
	  fprintf (e, "%sbaud-range %ld %ld\n", zprefix, qm->uuconf_ilowbaud,
		   qm->uuconf_ihighbaud);
	if (! qm->uuconf_fcarrier)
	  fprintf (e, "%scarrier false\n", zprefix);
	if (qm->uuconf_pzdialer != NULL)
	  {
	    if (qm->uuconf_pzdialer[1] == NULL)
	      fprintf (e, "%sdialer %s\n", zprefix, qm->uuconf_pzdialer[0]);
	    else
	      {
		sprintf (ab, "%sdialer-sequence", zprefix);
		uvwrite_string_array (e, qm->uuconf_pzdialer, zprefix);
	      }
	  }
	if (qm->uuconf_qdialer != NULL)
	  {
	    sprintf (ab, "%sdialer ", zprefix);
	    uvwrite_taylor_dialer (e, qm->uuconf_qdialer, ab);
	  }
      }
      break;
    case UUCONF_PORTTYPE_DIRECT:
      {
	struct uuconf_direct_port *qd;

	qd = &qport->uuconf_u.uuconf_sdirect;
	if (qd->uuconf_zdevice != NULL)
	  fprintf (e, "%sdevice %s\n", zprefix, qd->uuconf_zdevice);
	if (qd->uuconf_ibaud != 0)
	  fprintf (e, "%sbaud %ld\n", zprefix, qd->uuconf_ibaud);
      }
      break;
    case UUCONF_PORTTYPE_TCP:
      if (qport->uuconf_u.uuconf_stcp.uuconf_zport != NULL)
	fprintf (e, "%sservice %s\n", zprefix,
		 qport->uuconf_u.uuconf_stcp.uuconf_zport);
      break;
    case UUCONF_PORTTYPE_TLI:
      {
	struct uuconf_tli_port *qt;

	qt = &qport->uuconf_u.uuconf_stli;
	if (qt->uuconf_zdevice != NULL)
	  fprintf (e, "%sdevice %s\n", zprefix, qt->uuconf_zdevice);
	sprintf (ab, "%sstream", zprefix);
	uvwrite_boolean (e, qt->uuconf_fstream, ab);
	if (qt->uuconf_pzpush != NULL)
	  {
	    sprintf (ab, "%spush", zprefix);
	    uvwrite_string_array (e, qt->uuconf_pzpush, ab);
	  }
	if (qt->uuconf_pzdialer != NULL)
	  {
	    sprintf (ab, "%sdialer-sequence", zprefix);
	    uvwrite_string_array (e, qt->uuconf_pzdialer, ab);
	  }
	if (qt->uuconf_zservaddr != NULL)
	  fprintf (e, "%sserver-address %s\n", zprefix,
		   qt->uuconf_zservaddr);
      }
      break;
    }
}

/* Write out a port to the V2 L-devices file.  This is called via
   uuconf_find_port.  */

static int
ivwrite_v2_port (qport, pinfo)
     struct uuconf_port *qport;
     pointer pinfo;
{
  FILE *e = (FILE *) pinfo;

  if (qport->uuconf_ttype == UUCONF_PORTTYPE_DIRECT)
    {
      fprintf (e, "DIR %s - %ld direct",
	       qport->uuconf_u.uuconf_sdirect.uuconf_zdevice,
	       qport->uuconf_u.uuconf_sdirect.uuconf_ibaud);
    }
  else if (qport->uuconf_ttype == UUCONF_PORTTYPE_MODEM)
    {
      fprintf (e, "%s %s ", qport->uuconf_zname,
	       qport->uuconf_u.uuconf_smodem.uuconf_zdevice);
      if (qport->uuconf_u.uuconf_smodem.uuconf_zdial_device != NULL)
	fprintf (e, "%s", qport->uuconf_u.uuconf_smodem.uuconf_zdial_device);
      else
	fprintf (e, "-");
      fprintf (e, " ");
      if (qport->uuconf_u.uuconf_smodem.uuconf_ilowbaud != 0L)
	fprintf (e, "%ld-%ld",
		 qport->uuconf_u.uuconf_smodem.uuconf_ilowbaud,
		 qport->uuconf_u.uuconf_smodem.uuconf_ihighbaud);
      else if (qport->uuconf_u.uuconf_smodem.uuconf_ibaud != 0L)
	fprintf (e, "%ld", qport->uuconf_u.uuconf_smodem.uuconf_ibaud);
      else
	fprintf (e, "Any");
      if (qport->uuconf_u.uuconf_smodem.uuconf_pzdialer != NULL)
	fprintf (e, " %s",
		 qport->uuconf_u.uuconf_smodem.uuconf_pzdialer[0]);
    }
  else
    {
      fprintf (e, "# Ignoring port %s with unsupported type",
	       qport->uuconf_zname);
    }

  fprintf (e, "\n");

  /* Return UUCONF_NOT_FOUND to force uuconf_find_port to keep looking
     for a port.  */
  return UUCONF_NOT_FOUND;
}

/* Write out a port to the HDB Devices file.  This is called via
   uuconf_find_port.  */

static int
ivwrite_hdb_port (qport, pinfo)
     struct uuconf_port *qport;
     pointer pinfo;
{
  FILE *e = (FILE *) pinfo;

  if (qport->uuconf_ttype == UUCONF_PORTTYPE_DIRECT)
    {
      fprintf (e, "Direct");
      if (qport->uuconf_zprotocols != NULL)
	fprintf (e, ",%s", qport->uuconf_zprotocols);
      fprintf (e, " ");
      if (qport->uuconf_u.uuconf_sdirect.uuconf_zdevice != NULL)
	fprintf (e, "%s", qport->uuconf_u.uuconf_sdirect.uuconf_zdevice);
      else
	fprintf (e, "%s", qport->uuconf_zname);
      fprintf (e, " - %ld", qport->uuconf_u.uuconf_sdirect.uuconf_ibaud);
    }
  else if (qport->uuconf_ttype == UUCONF_PORTTYPE_MODEM)
    {
      fprintf (e, "%s", qport->uuconf_zname);
      if (qport->uuconf_zprotocols != NULL)
	fprintf (e, ",%s", qport->uuconf_zprotocols);
      fprintf (e, " ");
      if (qport->uuconf_u.uuconf_smodem.uuconf_zdevice != NULL)
	fprintf (e, "%s", qport->uuconf_u.uuconf_smodem.uuconf_zdevice);
      else
	fprintf (e, "%s", qport->uuconf_zname);
      fprintf (e, " ");
      if (qport->uuconf_u.uuconf_smodem.uuconf_zdial_device != NULL)
	fprintf (e, "%s", qport->uuconf_u.uuconf_smodem.uuconf_zdial_device);
      else
	fprintf (e, "-");
      fprintf (e, " ");
      if (qport->uuconf_u.uuconf_smodem.uuconf_ilowbaud != 0L)
	fprintf (e, "%ld-%ld",
		 qport->uuconf_u.uuconf_smodem.uuconf_ilowbaud,
		 qport->uuconf_u.uuconf_smodem.uuconf_ihighbaud);
      else if (qport->uuconf_u.uuconf_smodem.uuconf_ibaud != 0L)
	fprintf (e, "%ld", qport->uuconf_u.uuconf_smodem.uuconf_ibaud);
      else
	fprintf (e, "Any");
      if (qport->uuconf_u.uuconf_smodem.uuconf_pzdialer != NULL)
	{
	  char **pz;

	  for (pz = qport->uuconf_u.uuconf_smodem.uuconf_pzdialer;
	       *pz != NULL;
	       pz++)
	    fprintf (e, " %s", *pz);
	}
    }
  else if (qport->uuconf_ttype == UUCONF_PORTTYPE_TCP)
    {
      fprintf (e, "TCP");
      if (qport->uuconf_zprotocols != NULL)
	fprintf (e, ",%s", qport->uuconf_zprotocols);
      fprintf (e, " ");
      if (qport->uuconf_u.uuconf_stcp.uuconf_zport == NULL)
	fprintf (e, "uucp");
      else
	fprintf (e, "%s", qport->uuconf_u.uuconf_stcp.uuconf_zport);
      fprintf (e, " - -");
    }
  else if (qport->uuconf_ttype == UUCONF_PORTTYPE_TLI)
    {
      char **pz;

      fprintf (e, "%s", qport->uuconf_zname);
      if (qport->uuconf_zprotocols != NULL)
	fprintf (e, ",%s", qport->uuconf_zprotocols);
      fprintf (e, " ");
      if (qport->uuconf_u.uuconf_stli.uuconf_zdevice != NULL)
	fprintf (e, "%s", qport->uuconf_u.uuconf_smodem.uuconf_zdevice);
      else
	fprintf (e, "-");
      fprintf (e, " - -");
      pz = qport->uuconf_u.uuconf_stli.uuconf_pzdialer;
      if (pz == NULL
	  || *pz == NULL
	  || (strcmp (*pz, "TLI") != 0
	      && strcmp (*pz, "TLIS") != 0))
	fprintf (e, " TLI%s \\D",
		 qport->uuconf_u.uuconf_stli.uuconf_fstream ? "S" : "");
      if (pz != NULL)
	for (; *pz != NULL; pz++)
	  fprintf (e, " %s", *pz);
    }
  else
    {
      fprintf (e, "# Ignoring port %s with unsupported type",
	       qport->uuconf_zname);
    }

  fprintf (e, "\n");

  /* Return UUCONF_NOT_FOUND to force uuconf_find_port to keep looking
     for a port.  */
  return UUCONF_NOT_FOUND;
}

/* Write a dialer out to a Taylor UUCP configuration file.  This
   doesn't output the name, since it is called to output a specially
   defined dialer in the sys or port file.  */

static void
uvwrite_taylor_dialer (e, qdialer, zprefix)
     FILE *e;
     struct uuconf_dialer *qdialer;
     const char *zprefix;
{
  char ab[100];

  /* Reset default values, so we don't output them unnecessarily.  */
  if (qdialer->uuconf_schat.uuconf_ctimeout == 60)
    qdialer->uuconf_schat.uuconf_ctimeout = -1;
  if (qdialer->uuconf_schat.uuconf_fstrip)
    qdialer->uuconf_schat.uuconf_fstrip = -1;
  if (qdialer->uuconf_scomplete.uuconf_ctimeout == 60)
    qdialer->uuconf_scomplete.uuconf_ctimeout = -1;
  if (qdialer->uuconf_scomplete.uuconf_fstrip)
    qdialer->uuconf_scomplete.uuconf_fstrip = -1;
  if (qdialer->uuconf_sabort.uuconf_ctimeout == 60)
    qdialer->uuconf_sabort.uuconf_ctimeout = -1;
  if (qdialer->uuconf_sabort.uuconf_fstrip)
    qdialer->uuconf_sabort.uuconf_fstrip = -1;
  
  uvwrite_chat (e, &qdialer->uuconf_schat, (struct uuconf_chat *) NULL,
		zprefix, FALSE);
  if (qdialer->uuconf_zdialtone != NULL
      && strcmp (qdialer->uuconf_zdialtone, ",") != 0)
    fprintf (e, "%sdialtone %s\n", zprefix, qdialer->uuconf_zdialtone);
  if (qdialer->uuconf_zpause != NULL
      && strcmp (qdialer->uuconf_zpause, ",") != 0)
    fprintf (e, "%spause %s\n", zprefix, qdialer->uuconf_zpause);
  if (! qdialer->uuconf_fcarrier)
    fprintf (e, "%scarrier false\n", zprefix);
  if (qdialer->uuconf_ccarrier_wait != 60)
    fprintf (e, "%scarrier-wait %d\n", zprefix,
	     qdialer->uuconf_ccarrier_wait);
  if (qdialer->uuconf_fdtr_toggle)
    fprintf (e, "%sdtr-toggle %s %s\n", zprefix,
	     qdialer->uuconf_fdtr_toggle ? "true" : "false",
	     qdialer->uuconf_fdtr_toggle_wait ? "true" : "false");
  sprintf (ab, "%scomplete-", zprefix);
  uvwrite_chat (e, &qdialer->uuconf_scomplete, (struct uuconf_chat *) NULL,
		ab, FALSE);
  sprintf (ab, "%sabort-", zprefix);
  uvwrite_chat (e, &qdialer->uuconf_sabort, (struct uuconf_chat *) NULL,
		ab, FALSE);
  if (qdialer->uuconf_qproto_params != NULL)
    uvwrite_proto_params (e, qdialer->uuconf_qproto_params, zprefix);
  if ((qdialer->uuconf_ireliable & UUCONF_RELIABLE_SPECIFIED) != 0)
    {
      sprintf (ab, "%sseven-bit", zprefix);
      uvwrite_boolean (e,
		       ((qdialer->uuconf_ireliable & UUCONF_RELIABLE_EIGHT)
			== 0),
		       ab);
      sprintf (ab, "%sreliable", zprefix);
      uvwrite_boolean (e,
		       ((qdialer->uuconf_ireliable & UUCONF_RELIABLE_RELIABLE)
			!= 0),
		       ab);
      sprintf (ab, "%shalf-duplex", zprefix);
      uvwrite_boolean (e,
		       ((qdialer->uuconf_ireliable
			 & UUCONF_RELIABLE_FULLDUPLEX) == 0),
		       ab);
    }
}

/* Write a dialer out to an HDB configuration file.  */

static void
uvwrite_hdb_dialer (e, qdialer)
     FILE *e;
     struct uuconf_dialer *qdialer;
{
  fprintf (e, "%s ", qdialer->uuconf_zname);

  if (qdialer->uuconf_zdialtone != NULL)
    fprintf (e, "=%c", qdialer->uuconf_zdialtone[0]);
  if (qdialer->uuconf_zpause != NULL)
    fprintf (e, "-%c", qdialer->uuconf_zpause[0]);

  if (qdialer->uuconf_schat.uuconf_pzchat != NULL)
    {
      if (qdialer->uuconf_zdialtone == NULL
	  && qdialer->uuconf_zpause == NULL)
	fprintf (e, "\"\"");
      fprintf (e, " ");
      uvwrite_chat_script (e, qdialer->uuconf_schat.uuconf_pzchat);
    }

  fprintf (e, "\n");
}

/* Display a uuconf error and exit.  */

static void
uvuuconf_error (puuconf, iret)
     pointer puuconf;
     int iret;
{
  char ab[512];

  (void) uuconf_error_string (puuconf, iret, ab, sizeof ab);
  if ((iret & UUCONF_ERROR_FILENAME) == 0)
    fprintf (stderr, "uuconv: %s\n", ab);
  else
    fprintf (stderr, "uuconv:%s\n", ab);
  if (UUCONF_ERROR_VALUE (iret) != UUCONF_FOPEN_FAILED)
    exit (EXIT_FAILURE);
}
