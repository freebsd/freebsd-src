/* tportc.c
   Handle a Taylor UUCP port command.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_tportc_rcsid[] = "$Id: tportc.c,v 1.1 1993/08/05 18:26:12 conklin Exp $";
#endif

#include <errno.h>

static int ipproto_param P((pointer pglobal, int argc, char **argv,
			    pointer pvar, pointer pinfo));
static int ipbaud_range P((pointer pglobal, int argc, char **argv,
			   pointer pvar, pointer pinfo));
static int ipdialer P((pointer pglobal, int argc, char **argv, pointer pvar,
		       pointer pinfo));
static int ipcunknown P((pointer pglobal, int argc, char **argv,
			 pointer pvar, pointer pinfo));

/* The string names of the port types.  This array corresponds to the
   uuconf_porttype enumeration.  */

static const char * const azPtype_names[] =
{
  NULL,
  "stdin",
  "modem",
  "direct",
  "tcp",
  "tli"
};

#define CPORT_TYPES (sizeof azPtype_names / sizeof azPtype_names[0])

/* The command table for generic port commands.  The "port" and "type"
   commands are handled specially.  */
static const struct cmdtab_offset asPort_cmds[] =
{
  { "protocol", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_port, uuconf_zprotocols), NULL },
  { "protocol-parameter", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct uuconf_port, uuconf_qproto_params), ipproto_param },
  { "seven-bit", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct uuconf_port, uuconf_ireliable), _uuconf_iseven_bit },
  { "reliable", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct uuconf_port, uuconf_ireliable), _uuconf_ireliable },
  { "half-duplex", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct uuconf_port, uuconf_ireliable),
      _uuconf_ihalf_duplex },
  { "lockname", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_port, uuconf_zlockname), NULL },
  { NULL, 0, 0, NULL }
};

#define CPORT_CMDS (sizeof asPort_cmds / sizeof asPort_cmds[0])

/* The stdin port command table.  */
static const struct cmdtab_offset asPstdin_cmds[] =
{
  { NULL, 0, 0, NULL }
};

#define CSTDIN_CMDS (sizeof asPstdin_cmds / sizeof asPstdin_cmds[0])

/* The modem port command table.  */
static const struct cmdtab_offset asPmodem_cmds[] =
{
  { "device", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_port, uuconf_u.uuconf_smodem.uuconf_zdevice),
      NULL },
  { "baud", UUCONF_CMDTABTYPE_LONG,
      offsetof (struct uuconf_port, uuconf_u.uuconf_smodem.uuconf_ibaud),
      NULL },
  { "speed", UUCONF_CMDTABTYPE_LONG,
      offsetof (struct uuconf_port, uuconf_u.uuconf_smodem.uuconf_ibaud),
      NULL },
  { "baud-range", UUCONF_CMDTABTYPE_FN | 3,
      offsetof (struct uuconf_port, uuconf_u.uuconf_smodem), ipbaud_range },
  { "speed-range", UUCONF_CMDTABTYPE_FN | 3,
      offsetof (struct uuconf_port, uuconf_u.uuconf_smodem), ipbaud_range },
  { "carrier", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct uuconf_port, uuconf_u.uuconf_smodem.uuconf_fcarrier),
      NULL },
  { "dial-device", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_port,
		uuconf_u.uuconf_smodem.uuconf_zdial_device),
      NULL },
  { "dialer", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct uuconf_port, uuconf_u.uuconf_smodem), ipdialer },
  { "dialer-sequence", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_port, uuconf_u.uuconf_smodem.uuconf_pzdialer),
      NULL },
  { NULL, 0, 0, NULL }
};

#define CMODEM_CMDS (sizeof asPmodem_cmds / sizeof asPmodem_cmds[0])

/* The direct port command table.  */
static const struct cmdtab_offset asPdirect_cmds[] =
{
  { "device", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_port, uuconf_u.uuconf_sdirect.uuconf_zdevice),
      NULL },
  { "baud", UUCONF_CMDTABTYPE_LONG,
      offsetof (struct uuconf_port, uuconf_u.uuconf_sdirect.uuconf_ibaud),
      NULL },
  { "speed", UUCONF_CMDTABTYPE_LONG,
      offsetof (struct uuconf_port, uuconf_u.uuconf_sdirect.uuconf_ibaud),
      NULL },
  { NULL, 0, 0, NULL }
};

#define CDIRECT_CMDS (sizeof asPdirect_cmds / sizeof asPdirect_cmds[0])

/* The TCP port command table.  */
static const struct cmdtab_offset asPtcp_cmds[] =
{
  { "service", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_port, uuconf_u.uuconf_stcp.uuconf_zport),
      NULL },
  { NULL, 0, 0, NULL }
};

#define CTCP_CMDS (sizeof asPtcp_cmds / sizeof asPtcp_cmds[0])

/* The TLI port command table.  */
static const struct cmdtab_offset asPtli_cmds[] =
{
  { "device", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_port, uuconf_u.uuconf_stli.uuconf_zdevice),
      NULL },
  { "stream", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct uuconf_port, uuconf_u.uuconf_stli.uuconf_fstream),
      NULL },
  { "push", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_port, uuconf_u.uuconf_stli.uuconf_pzpush),
      NULL },
  { "dialer-sequence", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_port, uuconf_u.uuconf_stli.uuconf_pzdialer),
      NULL },
  { "server-address", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_port, uuconf_u.uuconf_stli.uuconf_zservaddr),
      NULL },
  { NULL, 0, 0, NULL }
};

#define CTLI_CMDS (sizeof asPtli_cmds / sizeof asPtli_cmds[0])

#undef max
#define max(i1, i2) ((i1) > (i2) ? (i1) : (i2))
#define CCMDS \
  max (max (max (CPORT_CMDS, CSTDIN_CMDS), CMODEM_CMDS), \
       max (max (CDIRECT_CMDS, CTCP_CMDS), CTLI_CMDS))

/* Handle a command passed to a port from a Taylor UUCP configuration
   file.  This can be called when reading either the port file or the
   sys file.  The return value may have UUCONF_CMDTABRET_KEEP set, but
   not UUCONF_CMDTABRET_EXIT.  It assigns values to the elements of
   qport.  The first time this is called, qport->uuconf_zname and
   qport->uuconf_palloc should be set and qport->uuconf_ttype should
   be UUCONF_PORTTYPE_UNKNOWN.  */

int
_uuconf_iport_cmd (qglobal, argc, argv, qport)
     struct sglobal *qglobal;
     int argc;
     char **argv;
     struct uuconf_port *qport;
{
  boolean fgottype;
  const struct cmdtab_offset *qcmds;
  size_t ccmds;
  struct uuconf_cmdtab as[CCMDS];
  int i;
  int iret;

  fgottype = strcasecmp (argv[0], "type") == 0;

  if (fgottype || qport->uuconf_ttype == UUCONF_PORTTYPE_UNKNOWN)
    {
      enum uuconf_porttype ttype;

      /* We either just got a "type" command, or this is an
	 uninitialized port.  If the first command to a port is not
	 "type", it is assumed to be a modem port.  This
	 implementation will actually permit "type" at any point, but
	 will effectively discard any type specific information that
	 appears before the "type" command.  This supports defaults,
	 in that the default may be of a specific type while future
	 ports in the same file may be of other types.  */
      if (! fgottype)
	ttype = UUCONF_PORTTYPE_MODEM;
      else
	{
	  if (argc != 2)
	    return UUCONF_SYNTAX_ERROR;

	  for (i = 0; i < CPORT_TYPES; i++)
	    if (azPtype_names[i] != NULL
		&& strcasecmp (argv[1], azPtype_names[i]) == 0)
	      break;

	  if (i >= CPORT_TYPES)
	    return UUCONF_SYNTAX_ERROR;
	  
	  ttype = (enum uuconf_porttype) i;
	}

      qport->uuconf_ttype = ttype;

      switch (ttype)
	{
	default:
	case UUCONF_PORTTYPE_STDIN:
	  break;
	case UUCONF_PORTTYPE_MODEM:
	  qport->uuconf_u.uuconf_smodem.uuconf_zdevice = NULL;
	  qport->uuconf_u.uuconf_smodem.uuconf_zdial_device = NULL;
	  qport->uuconf_u.uuconf_smodem.uuconf_ibaud = 0L;
	  qport->uuconf_u.uuconf_smodem.uuconf_ilowbaud = 0L;
	  qport->uuconf_u.uuconf_smodem.uuconf_ihighbaud = 0L;
	  qport->uuconf_u.uuconf_smodem.uuconf_fcarrier = TRUE;
	  qport->uuconf_u.uuconf_smodem.uuconf_pzdialer = NULL;
	  qport->uuconf_u.uuconf_smodem.uuconf_qdialer = NULL;
	  break;
	case UUCONF_PORTTYPE_DIRECT:
	  qport->uuconf_u.uuconf_sdirect.uuconf_zdevice = NULL;
	  qport->uuconf_u.uuconf_sdirect.uuconf_ibaud = -1;
	  break;
	case UUCONF_PORTTYPE_TCP:
	  qport->uuconf_u.uuconf_stcp.uuconf_zport = (char *) "uucp";
	  qport->uuconf_ireliable = (UUCONF_RELIABLE_SPECIFIED
				     | UUCONF_RELIABLE_ENDTOEND
				     | UUCONF_RELIABLE_RELIABLE
				     | UUCONF_RELIABLE_EIGHT
				     | UUCONF_RELIABLE_FULLDUPLEX);
	  break;
	case UUCONF_PORTTYPE_TLI:
	  qport->uuconf_u.uuconf_stli.uuconf_zdevice = NULL;
	  qport->uuconf_u.uuconf_stli.uuconf_fstream = FALSE;
	  qport->uuconf_u.uuconf_stli.uuconf_pzpush = NULL;
	  qport->uuconf_u.uuconf_stli.uuconf_pzdialer = NULL;
	  qport->uuconf_u.uuconf_stli.uuconf_zservaddr = NULL;
	  qport->uuconf_ireliable = (UUCONF_RELIABLE_SPECIFIED
				     | UUCONF_RELIABLE_ENDTOEND
				     | UUCONF_RELIABLE_RELIABLE
				     | UUCONF_RELIABLE_EIGHT
				     | UUCONF_RELIABLE_FULLDUPLEX);
	  break;
	}

      if (fgottype)
	return UUCONF_CMDTABRET_CONTINUE;
    }

  /* See if this command is one of the generic ones.  */
  qcmds = asPort_cmds;
  ccmds = CPORT_CMDS;

  for (i = 0; i < CPORT_CMDS - 1; i++)
    if (strcasecmp (argv[0], asPort_cmds[i].zcmd) == 0)
      break;

  if (i >= CPORT_CMDS - 1)
    {
      /* It's not a generic command, so we must check the type
	 specific commands.  */
      switch (qport->uuconf_ttype)
	{
	case UUCONF_PORTTYPE_STDIN:
	  qcmds = asPstdin_cmds;
	  ccmds = CSTDIN_CMDS;
	  break;
	case UUCONF_PORTTYPE_MODEM:
	  qcmds = asPmodem_cmds;
	  ccmds = CMODEM_CMDS;
	  break;
	case UUCONF_PORTTYPE_DIRECT:
	  qcmds = asPdirect_cmds;
	  ccmds = CDIRECT_CMDS;
	  break;
	case UUCONF_PORTTYPE_TCP:
	  qcmds = asPtcp_cmds;
	  ccmds = CTCP_CMDS;
	  break;
	case UUCONF_PORTTYPE_TLI:
	  qcmds = asPtli_cmds;
	  ccmds = CTLI_CMDS;
	  break;
	default:
	  return UUCONF_SYNTAX_ERROR;
	}
    }

  /* Copy the command table onto the stack and modify it to point to
     qport.  */
  _uuconf_ucmdtab_base (qcmds, ccmds, (char *) qport, as);

  iret = uuconf_cmd_args ((pointer) qglobal, argc, argv, as,
			  (pointer) qport, ipcunknown, 0,
			  qport->uuconf_palloc);

  return iret &~ UUCONF_CMDTABRET_EXIT;
}

/* Handle the "protocol-parameter" command.  */

static int
ipproto_param (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_proto_param **pqparam = (struct uuconf_proto_param **) pvar;
  struct uuconf_port *qport = (struct uuconf_port *) pinfo;

  return _uuconf_iadd_proto_param (qglobal, argc - 1, argv + 1, pqparam,
				   qport->uuconf_palloc);
}

/* Handle the "baud-range" command.  */

/*ARGSUSED*/
static int
ipbaud_range (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_modem_port *qmodem = (struct uuconf_modem_port *) pvar;
  int iret;

  iret = _uuconf_iint (qglobal, argv[1],
		       (pointer) &qmodem->uuconf_ilowbaud, FALSE);
  if ((iret &~ UUCONF_CMDTABRET_KEEP) != UUCONF_SUCCESS)
    return iret;

  iret |= _uuconf_iint (qglobal, argv[2],
			(pointer) &qmodem->uuconf_ihighbaud, FALSE);

  return iret;
}

/* Handle the "dialer" command.  If there is one argument, this names
   a dialer.  Otherwise, the remaining arguments form a command
   describing the dialer.  */

static int
ipdialer (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_modem_port *qmodem = (struct uuconf_modem_port *) pvar;
  struct uuconf_port *qport = (struct uuconf_port *) pinfo;
  int iret;

  if (argc < 2)
    return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;

  if (argc > 2)
    {
      if (qmodem->uuconf_qdialer == NULL)
	{
	  struct uuconf_dialer *qnew;
	  size_t clen;

	  qnew = ((struct uuconf_dialer *)
		  uuconf_malloc (qport->uuconf_palloc,
				  sizeof (struct uuconf_dialer)));
	  if (qnew == NULL)
	    {
	      qglobal->ierrno = errno;
	      return (UUCONF_MALLOC_FAILED
		      | UUCONF_ERROR_ERRNO
		      | UUCONF_CMDTABRET_EXIT);
	    }

	  _uuconf_uclear_dialer (qnew);

	  clen = strlen (qport->uuconf_zname);
	  qnew->uuconf_zname = (char *) uuconf_malloc (qport->uuconf_palloc,
						       (clen
							+ sizeof " dialer"));
	  if (qnew->uuconf_zname == NULL)
	    {
	      qglobal->ierrno = errno;
	      return (UUCONF_MALLOC_FAILED
		      | UUCONF_ERROR_ERRNO
		      | UUCONF_CMDTABRET_EXIT);
	    }

	  memcpy ((pointer) qnew->uuconf_zname,
		  (pointer) qport->uuconf_zname, clen);
	  memcpy ((pointer) (qnew->uuconf_zname + clen), (pointer) " dialer",
		  sizeof " dialer");

	  qnew->uuconf_palloc = qport->uuconf_palloc;

	  qmodem->uuconf_qdialer = qnew;
	}

      iret = _uuconf_idialer_cmd (qglobal, argc - 1, argv + 1,
				  qmodem->uuconf_qdialer);
      if ((iret &~ UUCONF_CMDTABRET_KEEP) != UUCONF_SUCCESS)
	iret |= UUCONF_CMDTABRET_EXIT;
      return iret;
    }
  else
    {
      qmodem->uuconf_pzdialer = NULL;
      iret = _uuconf_iadd_string (qglobal, argv[1], TRUE, FALSE,
				  &qmodem->uuconf_pzdialer,
				  qport->uuconf_palloc);
      if (iret != UUCONF_SUCCESS)
	iret |= UUCONF_CMDTABRET_EXIT;
      return iret;
    }
}

/* Give an error for an unknown port command.  */

/*ARGSUSED*/
static int
ipcunknown (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;
}
