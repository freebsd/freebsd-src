/* uuchk.c
   Display what we think the permissions of systems are.

   Copyright (C) 1991, 1992, 1993, 1994 Ian Lance Taylor

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
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucp.h"

#if USE_RCS_ID
const char uuchk_rcsid[] = "$Id: uuchk.c,v 1.2 1994/05/07 18:13:37 ache Exp $";
#endif

#include "getopt.h"

#include "uuconf.h"

/* Local functions.  */

static void ukusage P((void));
static void ukhelp P((void));
static void ukshow P((const struct uuconf_system *qsys,
		      pointer puuconf));
static int ikshow_port P((struct uuconf_port *qport, pointer pinfo));
static void ukshow_dialer P((struct uuconf_dialer *qdial));
static void ukshow_chat P((const struct uuconf_chat *qchat,
			   const char *zhdr));
static void ukshow_size P((struct uuconf_timespan *q, boolean fcall,
			   boolean flocal));
static void ukshow_reliable P ((int i, const char *zhdr));
static void ukshow_proto_params P((struct uuconf_proto_param *pas,
				   int cindent));
static void ukshow_time P((const struct uuconf_timespan *));
static struct uuconf_timespan *qcompress_span P((struct uuconf_timespan *));
static void ukuuconf_error P((pointer puuconf, int iret));

/* Structure used to pass uuconf pointer into ikshow_port and also let
   it record whether any ports were found.  */
struct sinfo
{
  /* The uuconf global pointer.  */
  pointer puuconf;
  /* The system.  */
  const struct uuconf_system *qsys;
  /* Whether any ports were seen.  */
  boolean fgot;
};

/* Program name.  */
static const char *zKprogram;

/* Long getopt options.  */
static const struct option asKlongopts[] =
{
  { "config", required_argument, NULL, 'I' },
  { "debug", required_argument, NULL, 'x' },
  { "version", no_argument, NULL, 'v' },
  { "help", no_argument, NULL, 1 },
  { NULL, 0, NULL, 0 }
};

int
main (argc, argv)
     int argc;
     char **argv;
{
  int iopt;
  /* The configuration file name.  */
  const char *zconfig = NULL;
  int iret;
  pointer puuconf;
  char **pzsystems;

  zKprogram = argv[0];

  while ((iopt = getopt_long (argc, argv, "I:vx:", asKlongopts,
			      (int *) NULL)) != EOF)
    {
      switch (iopt)
	{
	case 'I':
	  /* Set the configuration file name.  */
	  zconfig = optarg;
	  break;

	case 'x':
	  /* Set the debugging level.  There is actually no debugging
	     information for this program.  */
	  break;

	case 'v':
	  /* Print version and exit.  */
	  printf ("%s: Taylor UUCP %s, copyright (C) 1991, 1992, 1993, 1994 Ian Lance Taylor\n",
		  zKprogram, VERSION);
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/
	  
	case 1:
	  /* --help.  */
	  ukhelp ();
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  ukusage ();
	  /*NOTREACHED*/
	}
    }

  if (optind != argc)
    {
      fprintf (stderr, "%s: too many arguments", zKprogram);
      ukusage ();
    }

  iret = uuconf_init (&puuconf, (const char *) NULL, zconfig);
  if (iret != UUCONF_SUCCESS)
    ukuuconf_error (puuconf, iret);

  iret = uuconf_system_names (puuconf, &pzsystems, FALSE);
  if (iret != UUCONF_SUCCESS)
    ukuuconf_error (puuconf, iret);

  if (*pzsystems == NULL)
    {
      fprintf (stderr, "%s: no systems found\n", zKprogram);
      exit (EXIT_FAILURE);
    }

  while (*pzsystems != NULL)
    {
      struct uuconf_system ssys;

      iret = uuconf_system_info (puuconf, *pzsystems, &ssys);
      if (iret != UUCONF_SUCCESS)
	ukuuconf_error (puuconf, iret);
      else
	ukshow (&ssys, puuconf);
      (void) uuconf_system_free (puuconf, &ssys);
      ++pzsystems;
      if (*pzsystems != NULL)
	printf ("\n");
    }

  exit (EXIT_SUCCESS);

  /* Avoid errors about not returning a value.  */
  return 0;
}

/* Print a usage message and die.  */

static void ukusage ()
{
  fprintf (stderr, "Usage: %s [{-I,--config} file]\n", zKprogram);
  fprintf (stderr, "Use %s --help for help\n", zKprogram);
  exit (EXIT_FAILURE);
}

/* Print a help message.  */

static void
ukhelp ()
{
  printf ("Taylor UUCP %s, copyright (C) 1991, 1992, 1993, 1994 Ian Lance Taylor\n",
	  VERSION);
  printf ("Usage: %s [{-I,--config} file] [-v] [--version] [--help]\n",
	  zKprogram);
  printf (" -I,--config file: Set configuration file to use\n");
  printf (" -v,--version: Print version and exit\n");
  printf (" --help: Print help and exit\n");
}

/* Dump out the information for a system.  */

static void
ukshow (qsys, puuconf)
     const struct uuconf_system *qsys;
     pointer puuconf;
{
  char **pz;
  int i;
  int iret;

  printf ("System: %s", qsys->uuconf_zname);
  if (qsys->uuconf_pzalias != NULL)
    {
      printf (" (");
      for (pz = qsys->uuconf_pzalias; *pz != NULL; pz++)
	{
	  printf ("%s", *pz);
	  if (pz[1] != NULL)
	    printf (" ");
	}
      printf (")");
    }
  printf ("\n");

  for (i = 0; qsys != NULL; qsys = qsys->uuconf_qalternate, i++)
    {
      boolean fcall, fcalled;
      struct uuconf_timespan *qtime, *qspan;

      if (i != 0 || qsys->uuconf_qalternate != NULL)
	{
	  printf ("Alternate %d", i);
	  if (qsys->uuconf_zalternate != NULL)
	    printf (" (%s)", qsys->uuconf_zalternate);
	  printf ("\n");
	}

      /* See if this alternate could be used when calling out.  */
      fcall = qsys->uuconf_fcall;
      if (qsys->uuconf_qtimegrade == NULL)
	fcall = FALSE;

      /* See if this alternate could be used when calling in.  */
      fcalled = qsys->uuconf_fcalled;

      if (! fcall && ! fcalled)
	{
	  printf (" This alternate is never used\n");
	  continue;
	}

      if (fcalled)
	{
	  if (qsys->uuconf_zcalled_login != NULL
	      && strcmp (qsys->uuconf_zcalled_login, "ANY") != 0)
	    {
	      if (i == 0 && qsys->uuconf_qalternate == NULL)
		printf (" Caller must log in as %s\n",
			qsys->uuconf_zcalled_login);
	      else
		printf (" When called using login name %s\n",
			qsys->uuconf_zcalled_login);
	    }
	  else
	    printf (" When called using any login name\n");

	  if (qsys->uuconf_zlocalname != NULL)
	    printf (" Will use %s as name of local system\n",
		    qsys->uuconf_zlocalname);
	}

      if (fcalled && qsys->uuconf_fcallback)
	{
	  printf (" If called, will call back\n");
	  fcalled = FALSE;
	}

      if (fcall)
	{
	  struct sinfo si;

	  if (i == 0 && qsys->uuconf_qalternate == NULL)
	    printf (" Call out");
	  else
	    printf (" This alternate applies when calling");
	  
	  if (qsys->uuconf_zport != NULL || qsys->uuconf_qport != NULL)
	    {
	      printf (" using ");
	      if (qsys->uuconf_zport != NULL)
		printf ("port %s", qsys->uuconf_zport);
	      else
		printf ("a specially defined port");
	      if (qsys->uuconf_ibaud != 0)
		{
		  printf (" at speed %ld", qsys->uuconf_ibaud);
		  if (qsys->uuconf_ihighbaud != 0)
		    printf (" to %ld", qsys->uuconf_ihighbaud);
		}
	      printf ("\n");
	    }
	  else if (qsys->uuconf_ibaud != 0)
	    {
	      printf (" at speed %ld", qsys->uuconf_ibaud);
	      if (qsys->uuconf_ihighbaud != 0)
		printf (" to %ld", qsys->uuconf_ihighbaud);
	      printf ("\n");
	    }
	  else
	    printf (" using any port\n");

	  si.puuconf = puuconf;
	  si.qsys = qsys;
	  si.fgot = FALSE;

	  if (qsys->uuconf_qport != NULL)
	    {
	      printf (" The port is defined as:\n");
	      (void) ikshow_port (qsys->uuconf_qport, (pointer) &si);
	    }
	  else
	    {
	      struct uuconf_port sdummy;

	      printf (" The possible ports are:\n");
	      iret = uuconf_find_port (puuconf, qsys->uuconf_zport,
				       qsys->uuconf_ibaud,
				       qsys->uuconf_ihighbaud,
				       ikshow_port, (pointer) &si,
				       &sdummy);
	      if (iret != UUCONF_NOT_FOUND)
		ukuuconf_error (puuconf, iret);
	      if (! si.fgot)
		printf (" *** There are no matching ports\n");
	    }

	  if (qsys->uuconf_zphone != NULL)
	    {
	      if ((qsys->uuconf_zport != NULL
		   && strcmp (qsys->uuconf_zport, "TCP") == 0)
		  || (qsys->uuconf_qport != NULL
		      && (qsys->uuconf_qport->uuconf_ttype
			  == UUCONF_PORTTYPE_TCP
			  || qsys->uuconf_qport->uuconf_ttype
			  == UUCONF_PORTTYPE_TLI)))
		printf (" Remote address %s\n", qsys->uuconf_zphone);
	      else
		printf (" Phone number %s\n", qsys->uuconf_zphone);
	    }

	  ukshow_chat (&qsys->uuconf_schat, " Chat");

	  if (qsys->uuconf_zcall_login != NULL
	      || qsys->uuconf_zcall_password != NULL)
	    {
	      char *zlogin, *zpass;

	      iret = uuconf_callout (puuconf, qsys, &zlogin, &zpass);
	      if (iret == UUCONF_NOT_FOUND)
		printf (" Can not determine login name or password\n");
	      else if (UUCONF_ERROR_VALUE (iret) == UUCONF_FOPEN_FAILED)
		printf (" Can not read call out file\n");
	      else if (iret != UUCONF_SUCCESS)
		ukuuconf_error (puuconf, iret);
	      else
		{
		  if (zlogin != NULL)
		    {
		      printf (" Login name %s\n", zlogin);
		      free ((pointer) zlogin);
		    }
		  if (zpass != NULL)
		    {
		      printf (" Password %s\n", zpass);
		      free ((pointer) zpass);
		    }
		}
	    }

	  qtime = qcompress_span (qsys->uuconf_qtimegrade);

	  for (qspan = qtime; qspan != NULL; qspan = qspan->uuconf_qnext)
	    {
	      printf (" ");
	      ukshow_time (qspan);
	      printf (" may call if ");
	      if ((char) qspan->uuconf_ival == UUCONF_GRADE_LOW)
		printf ("any work");
	      else
		printf ("work grade %c or higher", (char) qspan->uuconf_ival);
	      if (qspan->uuconf_cretry != 0)
		printf (" (retry %d)", qspan->uuconf_cretry);
	      printf ("\n");
	    }

	  if (qsys->uuconf_qcalltimegrade != NULL)
	    {
	      boolean fprint, fother;

	      qtime = qcompress_span (qsys->uuconf_qcalltimegrade);
	      fprint = FALSE;
	      fother = FALSE;
	      if (qtime->uuconf_istart != 0)
		fother = TRUE;
	      for (qspan = qtime; qspan != NULL; qspan = qspan->uuconf_qnext)
		{
		  if ((char) qspan->uuconf_ival == UUCONF_GRADE_LOW)
		    {
		      fother = TRUE;
		      continue;
		    }
		  fprint = TRUE;
		  printf (" ");
		  ukshow_time (qspan);
		  printf (" may accept work grade %c or higher\n",
			  (char) qspan->uuconf_ival);
		  if (qspan->uuconf_qnext == NULL)
		    {
		      if (qspan->uuconf_iend != 7 * 24 * 60)
			fother = TRUE;
		    }
		  else
		    {
		      if (qspan->uuconf_iend
			  != qspan->uuconf_qnext->uuconf_istart)
			fother = TRUE;
		    }
		}
	      if (fprint && fother)
		printf (" (At other times may accept any work)\n");
	    }
	}

      if (qsys->uuconf_fsequence)
	printf (" Sequence numbers are used\n");

      if (fcalled)
	ukshow_chat (&qsys->uuconf_scalled_chat, " When called, chat");

      if (qsys->uuconf_zdebug != NULL)
	printf (" Debugging level %s\n", qsys->uuconf_zdebug);
      if (qsys->uuconf_zmax_remote_debug != NULL)
	printf (" Max remote debugging level %s\n",
		qsys->uuconf_zmax_remote_debug);

      if (fcall)
	{
	  ukshow_size (qsys->uuconf_qcall_local_size, TRUE, TRUE);
	  ukshow_size (qsys->uuconf_qcall_remote_size, TRUE, FALSE);
	}
      if (fcalled)
	{
	  ukshow_size (qsys->uuconf_qcalled_local_size, FALSE, TRUE);
	  ukshow_size (qsys->uuconf_qcalled_remote_size, FALSE, FALSE);
	}

      if (fcall)
	printf (" May %smake local requests when calling\n",
		qsys->uuconf_fcall_transfer ? "" : "not ");

      if (fcalled)
	printf (" May %smake local requests when called\n",
		qsys->uuconf_fcalled_transfer ? "" : "not ");

      if (qsys->uuconf_fcall_transfer || qsys->uuconf_fcalled_transfer)
	{
	  printf (" May send by local request:");
	  for (pz = qsys->uuconf_pzlocal_send; *pz != NULL; pz++)
	    printf (" %s", *pz);
	  printf ("\n");
	}
      if (! qsys->uuconf_fsend_request)
	printf (" May not send files by remote request\n");
      else
	{
	  printf (" May send by remote request:");
	  for (pz = qsys->uuconf_pzremote_send; *pz != NULL; pz++)
	    printf (" %s", *pz);
	  printf ("\n");
	}
      if (qsys->uuconf_fcall_transfer || qsys->uuconf_fcalled_transfer)
	{
	  printf (" May accept by local request:");
	  for (pz = qsys->uuconf_pzlocal_receive; *pz != NULL; pz++)
	    printf (" %s", *pz);
	  printf ("\n");
	}
      if (! qsys->uuconf_frec_request)
	printf (" May not receive files by remote request\n");
      else
	{
	  printf (" May receive by remote request:");
	  for (pz = qsys->uuconf_pzremote_receive; *pz != NULL; pz++)
	    printf (" %s", *pz);
	  printf ("\n");
	}

      printf (" May execute");
      for (pz = qsys->uuconf_pzcmds; *pz != NULL; pz++)
	printf (" %s", *pz);
      printf ("\n");

      printf (" Execution path");
      for (pz = qsys->uuconf_pzpath; *pz != NULL; pz++)
	printf (" %s" , *pz);
      printf ("\n");

      if (qsys->uuconf_cfree_space != 0)
	printf (" Will leave %ld bytes available\n", qsys->uuconf_cfree_space);

      if (qsys->uuconf_zpubdir != NULL)
	printf (" Public directory is %s\n", qsys->uuconf_zpubdir);

      if (qsys->uuconf_pzforward_from != NULL)
	{
	  printf (" May forward from");
	  for (pz = qsys->uuconf_pzforward_from; *pz != NULL; pz++)
	    printf (" %s", *pz);
	  printf ("\n");
	}
	  
      if (qsys->uuconf_pzforward_to != NULL)
	{
	  printf (" May forward to");
	  for (pz = qsys->uuconf_pzforward_to; *pz != NULL; pz++)
	    printf (" %s", *pz);
	  printf ("\n");
	}
	  
      if (qsys->uuconf_zprotocols != NULL)
	printf (" Will use protocols %s\n", qsys->uuconf_zprotocols);
      else
	printf (" Will use any known protocol\n");

      if (qsys->uuconf_qproto_params != NULL)
	ukshow_proto_params (qsys->uuconf_qproto_params, 1);
    }
}

/* Show information about a port.  */

/*ARGSUSED*/
static int
ikshow_port (qport, pinfo)
     struct uuconf_port *qport;
     pointer pinfo;
{
  struct sinfo *qi = (struct sinfo *) pinfo;
  char **pz;
  struct uuconf_modem_port *qmodem;
  struct uuconf_tcp_port *qtcp;
  struct uuconf_tli_port *qtli;
  struct uuconf_pipe_port *qpipe;

  qi->fgot = TRUE;

  printf ("  Port name %s\n", qport->uuconf_zname);
  switch (qport->uuconf_ttype)
    {
    case UUCONF_PORTTYPE_STDIN:
      printf ("   Port type stdin\n");
      break;
    case UUCONF_PORTTYPE_DIRECT:
      printf ("   Port type direct\n");
      if (qport->uuconf_u.uuconf_sdirect.uuconf_zdevice != NULL)
	printf ("   Device %s\n",
		qport->uuconf_u.uuconf_sdirect.uuconf_zdevice);
      else
	printf ("   Using port name as device name\n");
      printf ("   Speed %ld\n", qport->uuconf_u.uuconf_sdirect.uuconf_ibaud);
      printf ("   Carrier %savailable\n",
	      qport->uuconf_u.uuconf_sdirect.uuconf_fcarrier ? "" : "not ");
      printf ("   Hardware flow control %savailable\n",
	      qport->uuconf_u.uuconf_sdirect.uuconf_fhardflow ? "" : "not ");
      break;
    case UUCONF_PORTTYPE_MODEM:
      qmodem = &qport->uuconf_u.uuconf_smodem;
      printf ("   Port type modem\n");
      if (qmodem->uuconf_zdevice != NULL)
	printf ("   Device %s\n", qmodem->uuconf_zdevice);
      else
	printf ("   Using port name as device name\n");
      if (qmodem->uuconf_zdial_device != NULL)
	printf ("   Dial device %s\n", qmodem->uuconf_zdial_device);
      printf ("   Speed %ld\n", qmodem->uuconf_ibaud);
      if (qmodem->uuconf_ilowbaud != qmodem->uuconf_ihighbaud)
	printf ("   Speed range %ld to %ld\n", qmodem->uuconf_ilowbaud,
		qmodem->uuconf_ihighbaud);
      printf ("   Carrier %savailable\n",
	      qmodem->uuconf_fcarrier ? "" : "not ");
      printf ("   Hardware flow control %savailable\n",
	      qmodem->uuconf_fhardflow ? "" : "not ");
      if (qmodem->uuconf_qdialer != NULL)
	{
	  printf ("   Specially defined dialer\n");
	  ukshow_dialer (qmodem->uuconf_qdialer);
	}
      else if (qmodem->uuconf_pzdialer != NULL
	       && qmodem->uuconf_pzdialer[0] != NULL)
	{
	  struct uuconf_dialer sdial;
	  int iret;

	  /* This might be a single dialer name, or it might be a
	     sequence of dialer/token pairs.  */

	  if (qmodem->uuconf_pzdialer[1] == NULL
	      || qmodem->uuconf_pzdialer[2] == NULL)
	    {
	      iret = uuconf_dialer_info (qi->puuconf,
					 qmodem->uuconf_pzdialer[0],
					 &sdial);
	      if (iret == UUCONF_NOT_FOUND)
		printf ("   *** No dialer %s\n", qmodem->uuconf_pzdialer[0]);
	      else if (iret != UUCONF_SUCCESS)
		ukuuconf_error (qi->puuconf, iret);
	      else
		{
		  printf ("   Dialer %s\n", qmodem->uuconf_pzdialer[0]);
		  ukshow_dialer (&sdial);
		  if (qmodem->uuconf_pzdialer[1] != NULL)
		    printf ("   Token %s\n", qmodem->uuconf_pzdialer[1]);
		}
	    }
	  else
	    {
	      pz = qmodem->uuconf_pzdialer;
	      while (*pz != NULL)
		{
		  iret = uuconf_dialer_info (qi->puuconf, *pz, &sdial);
		  if (iret == UUCONF_NOT_FOUND)
		    printf ("   *** No dialer %s\n", *pz);
		  else if (iret != UUCONF_SUCCESS)
		    ukuuconf_error (qi->puuconf, iret);
		  else
		    {
		      printf ("   Dialer %s\n", *pz);
		      ukshow_dialer (&sdial);
		    }

		  ++pz;
		  if (*pz != NULL)
		    {
		      printf ("   Token %s\n", *pz);
		      ++pz;
		    }
		}
	    }
	}
      else
	printf ("   *** No dialer information\n");
      break;
    case UUCONF_PORTTYPE_TCP:
      qtcp = &qport->uuconf_u.uuconf_stcp;
      printf ("   Port type tcp\n");
      printf ("   TCP service %s\n", qtcp->uuconf_zport);
      if (qtcp->uuconf_pzdialer != NULL
	  && qtcp->uuconf_pzdialer[0] != NULL)
	{
	  printf ("   Dialer sequence");
	  for (pz = qtcp->uuconf_pzdialer; *pz != NULL; pz++)
	    printf (" %s", *pz);
	  printf ("\n");
	}
      break;
    case UUCONF_PORTTYPE_TLI:
      qtli = &qport->uuconf_u.uuconf_stli;
      printf ("   Port type TLI%s\n",
	      qtli->uuconf_fstream ? "S" : "");
      if (qtli->uuconf_zdevice != NULL)
	printf ("   Device %s\n", qtli->uuconf_zdevice);
      else
	printf ("   Using port name as device name\n");
      if (qtli->uuconf_pzpush != NULL)
	{
	  printf ("   Push");
	  for (pz = qtli->uuconf_pzpush; *pz != NULL; pz++)
	    printf (" %s", *pz);
	  printf ("\n");
	}
      if (qtli->uuconf_pzdialer != NULL
	  && qtli->uuconf_pzdialer[0] != NULL)
	{
	  printf ("   Dialer sequence");
	  for (pz = qtli->uuconf_pzdialer; *pz != NULL; pz++)
	    printf (" %s", *pz);
	  printf ("\n");
	}
      if (qtli->uuconf_zservaddr != NULL)
	printf ("   Server address %s\n", qtli->uuconf_zservaddr);
      break;
    case UUCONF_PORTTYPE_PIPE:
      qpipe = &qport->uuconf_u.uuconf_spipe;
      printf ("   Port type pipe\n");
      if (qpipe->uuconf_pzcmd != NULL)
	{
	  printf ("   Command");
	  for (pz = qpipe->uuconf_pzcmd; *pz != NULL; pz++)
	    printf (" %s", *pz);
	  printf ("\n");
	}
      break;
    default:
      fprintf (stderr, "   CAN'T HAPPEN\n");
      break;
    }

  if (qport->uuconf_zprotocols != NULL)
    printf ("   Will use protocols %s\n", qport->uuconf_zprotocols);

  if (qport->uuconf_zlockname != NULL)
    printf ("   Will use lockname %s\n", qport->uuconf_zlockname);

  if ((qport->uuconf_ireliable & UUCONF_RELIABLE_SPECIFIED) != 0)
    ukshow_reliable (qport->uuconf_ireliable, "   ");

  if (qport->uuconf_qproto_params != NULL)
    ukshow_proto_params (qport->uuconf_qproto_params, 3);

  /* Return NOT_FOUND to force find_port to continue searching.  */
  return UUCONF_NOT_FOUND;
}

/* Show information about a dialer.  */

static void
ukshow_dialer (q)
     struct uuconf_dialer *q;
{
  ukshow_chat (&q->uuconf_schat, "    Chat");
  printf ("    Wait for dialtone %s\n", q->uuconf_zdialtone);
  printf ("    Pause while dialing %s\n", q->uuconf_zpause);
  printf ("    Carrier %savailable\n", q->uuconf_fcarrier ? "" : "not ");
  if (q->uuconf_fcarrier)
    printf ("    Wait %d seconds for carrier\n", q->uuconf_ccarrier_wait);
  if (q->uuconf_fdtr_toggle)
    {
      printf ("    Toggle DTR");
      if (q->uuconf_fdtr_toggle_wait)
	printf (" and wait");
      printf ("\n");
    }
  ukshow_chat (&q->uuconf_scomplete, "    When complete chat");
  ukshow_chat (&q->uuconf_sabort, "    When aborting chat");
  if ((q->uuconf_ireliable & UUCONF_RELIABLE_SPECIFIED) != 0)
    ukshow_reliable (q->uuconf_ireliable, "   ");
  if (q->uuconf_qproto_params != NULL)
    ukshow_proto_params (q->uuconf_qproto_params, 4);
}

/* Show a chat script.  */

static void
ukshow_chat (qchat, zhdr)
     const struct uuconf_chat *qchat;
     const char *zhdr;
{
  char **pz;

  if (qchat->uuconf_pzprogram != NULL)
    {
      printf ("%s program", zhdr);
      for (pz = qchat->uuconf_pzprogram; *pz != NULL; pz++)
	printf (" %s", *pz);
      printf ("\n");
    }

  if (qchat->uuconf_pzchat != NULL)
    {

      printf ("%s script", zhdr);
      for (pz = qchat->uuconf_pzchat; *pz != NULL; pz++)
	{
	  if ((*pz)[0] != '-' || pz == qchat->uuconf_pzchat)
	    printf (" ");
	  printf ("%s", *pz);
	}
      printf ("\n");
      printf ("%s script timeout %d\n", zhdr, qchat->uuconf_ctimeout);
      if (qchat->uuconf_pzfail != NULL)
	{
	  printf ("%s failure strings", zhdr);
	  for (pz = qchat->uuconf_pzfail; *pz != NULL; pz++)
	    printf (" %s", *pz);
	  printf ("\n");
	}
      if (qchat->uuconf_fstrip)
	printf ("%s script incoming bytes stripped to seven bits\n", zhdr);
    }
}

/* Show a size/time restriction.  */

static void
ukshow_size (qspan, fcall, flocal)
     struct uuconf_timespan *qspan;
     boolean fcall;
     boolean flocal;
{
  struct uuconf_timespan *q;
  boolean fother;

  qspan = qcompress_span (qspan);
  if (qspan == NULL)
    return;

  printf (" If call%s the following applies to a %s request:\n",
	  fcall ? "ing" : "ed", flocal ? "local" : "remote");

  fother = FALSE;
  if (qspan->uuconf_istart >= 60)
    fother = TRUE;

  for (q = qspan; q != NULL; q = q->uuconf_qnext)
    {
      printf ("  ");
      ukshow_time (q);
      printf (" may transfer files %ld bytes or smaller\n", q->uuconf_ival);
      if (q->uuconf_qnext == NULL)
	{
	  if (q->uuconf_iend <= 6 * 24 * 60 + 23 * 60)
	    fother = TRUE;
	}
      else
	{
	  if (q->uuconf_iend + 60 <= q->uuconf_qnext->uuconf_istart)
	    fother = TRUE;
	}
    }

  if (fother)
    printf ("  (At other times may send files of any size)\n");
}

/* Show reliability information.  */

static void
ukshow_reliable (i, zhdr)
     int i;
     const char *zhdr;
{
  printf ("%sCharacteristics:", zhdr);
  if ((i & UUCONF_RELIABLE_EIGHT) != 0)
    printf (" eight-bit-clean");
  else
    printf (" not-eight-bit-clean");
  if ((i & UUCONF_RELIABLE_RELIABLE) != 0)
    printf (" reliable");
  if ((i & UUCONF_RELIABLE_ENDTOEND) != 0)
    printf (" end-to-end");
  if ((i & UUCONF_RELIABLE_FULLDUPLEX) != 0)
    printf (" fullduplex");
  else
    printf (" halfduplex");
  printf ("\n");
}

/* Show protocol parameters.  */

static void
ukshow_proto_params (pas, cindent)
     struct uuconf_proto_param *pas;
     int cindent;
{
  struct uuconf_proto_param *q;

  for (q = pas; q->uuconf_bproto != '\0'; q++)
    {
      int i;
      struct uuconf_proto_param_entry *qe;

      for (i = 0; i < cindent; i++)
	printf (" ");
      printf ("For protocol %c will use the following parameters\n",
	      q->uuconf_bproto);
      for (qe = q->uuconf_qentries; qe->uuconf_cargs > 0; qe++)
	{
	  int ia;

	  for (i = 0; i < cindent; i++)
	    printf (" ");
	  for (ia = 0; ia < qe->uuconf_cargs; ia++)
	    printf (" %s", qe->uuconf_pzargs[ia]);
	  printf ("\n");
	}
    }
}

/* Display a time span.  */

static void
ukshow_time (q)
     const struct uuconf_timespan *q;
{
  int idaystart, idayend;
  int ihourstart, ihourend;
  int iminutestart, iminuteend;
  const char * const zdays = "Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat\0Sun";

  if (q->uuconf_istart == 0 && q->uuconf_iend == 7 * 24 * 60)
    {
      printf ("At any time");
      return;
    }

  idaystart = q->uuconf_istart / (24 * 60);
  ihourstart = (q->uuconf_istart % (24 * 60)) / 60;
  iminutestart = q->uuconf_istart % 60;
  idayend = q->uuconf_iend / (24 * 60);
  ihourend = (q->uuconf_iend % (24 * 60)) / 60;
  iminuteend = q->uuconf_iend % 60;

  if (idaystart == idayend)
    printf ("%s from %02d:%02d to %02d:%02d", zdays + idaystart * 4,
	    ihourstart, iminutestart, ihourend, iminuteend);
  else
    printf ("From %s %02d:%02d to %s %02d:%02d",
	    zdays + idaystart * 4, ihourstart, iminutestart,
	    zdays + idayend * 4, ihourend, iminuteend);
}

/* Compress a time span by merging any two adjacent spans with
   identical values.  This isn't necessary for uucico, but it looks
   nicer when printed out.  */

static struct uuconf_timespan *
qcompress_span (qlist)
     struct uuconf_timespan *qlist;
{
  struct uuconf_timespan **pq;

  pq = &qlist;
  while (*pq != NULL)
    {
      if ((*pq)->uuconf_qnext != NULL
	  && (*pq)->uuconf_iend == (*pq)->uuconf_qnext->uuconf_istart
	  && (*pq)->uuconf_ival == (*pq)->uuconf_qnext->uuconf_ival)
	{
	  struct uuconf_timespan *qnext;

	  qnext = (*pq)->uuconf_qnext;
	  (*pq)->uuconf_qnext = qnext->uuconf_qnext;
	  (*pq)->uuconf_iend = qnext->uuconf_iend;
	}
      else
	pq = &(*pq)->uuconf_qnext;
    }

  return qlist;
}

/* Display a uuconf error and exit.  */

static void
ukuuconf_error (puuconf, iret)
     pointer puuconf;
     int iret;
{
  char ab[512];

  (void) uuconf_error_string (puuconf, iret, ab, sizeof ab);
  if ((iret & UUCONF_ERROR_FILENAME) == 0)
    fprintf (stderr, "%s: %s\n", zKprogram, ab);
  else
    fprintf (stderr, "%s:%s\n", zKprogram, ab);
  exit (EXIT_FAILURE);
}
