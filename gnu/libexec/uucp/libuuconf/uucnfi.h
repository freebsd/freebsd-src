/* uucnfi.h
   Internal header file for the uuconf package.

   Copyright (C) 1992, 1993, 1994 Ian Lance Taylor

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
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

/* This is the internal header file for the uuconf package.  It should
   not be included by anything other than the uuconf code itself.  */

/* Get all the general definitions.  */
#include "uucp.h"

/* Get the uuconf header file itself.  */
#include "uuconf.h"

/* We need the system dependent header file.  */
#include "syshdr.h"

/* This is the generic information structure.  This holds all the
   per-thread global information needed by the uuconf code.  The
   per-process global information is held in an sprocess structure,
   which this structure points to.  This permits the code to not have
   any global variables at all.  */

struct sglobal
{
  /* A pointer to the per-process global information.  */
  struct sprocess *qprocess;
  /* A memory block in which all the memory for these fields is
     allocated.  */
  pointer pblock;
  /* The value of errno after an error.  */
  int ierrno;
  /* The filename for which an error occurred.  */
  const char *zfilename;
  /* The line number at which an error occurred.  */
  int ilineno;
};

/* This is the per-process information structure.  This essentially
   holds all the global variables used by uuconf.  */

struct sprocess
{
  /* The name of the local machine.  This will be NULL if it is not
     specified in a configuration file.  */
  const char *zlocalname;
  /* The spool directory.  */
  const char *zspooldir;
  /* The default public directory.  */
  const char *zpubdir;
  /* The lock directory.  */
  const char *zlockdir;
  /* The log file.  */
  const char *zlogfile;
  /* The statistics file.  */
  const char *zstatsfile;
  /* The debugging file.  */
  const char *zdebugfile;
  /* The default debugging level.  */
  const char *zdebug;
  /* The maximum number of simultaneously executing uuxqts.  */
  int cmaxuuxqts;
  /* How often to spawn a uuxqt process.  */
  const char *zrunuuxqt;
  /* Whether we are reading the V2 configuration files.  */
  boolean fv2;
  /* Whether we are reading the HDB configuration files.  */
  boolean fhdb;
  /* The names of the dialcode files.  */
  char **pzdialcodefiles;
  /* Timetables.  These are in pairs.  The first element is the name,
     the second is the time string.  */
  char **pztimetables;

  /* Taylor UUCP config file name.  */
  char *zconfigfile;
  /* Taylor UUCP sys file names.  */
  char **pzsysfiles;
  /* Taylor UUCP port file names.  */
  char **pzportfiles;
  /* Taylor UUCP dial file names.  */
  char **pzdialfiles;
  /* Taylor UUCP passwd file names.  */
  char **pzpwdfiles;
  /* Taylor UUCP call file names.  */
  char **pzcallfiles;
  /* List of "unknown" commands from config file.  */
  struct sunknown *qunknown;
  /* Whether the Taylor UUCP system information locations have been
     read.  */
  boolean fread_syslocs;
  /* Taylor UUCP system information locations.  */
  struct stsysloc *qsyslocs;
  /* Taylor UUCP validation restrictions.  */
  struct svalidate *qvalidate;
  /* Whether the "myname" command is used in a Taylor UUCP file.  */
  boolean fuses_myname;

  /* V2 system file name (L.sys).  */
  char *zv2systems;
  /* V2 device file name (L-devices).  */
  char *zv2devices;
  /* V2 user permissions file name (USERFILE).  */
  char *zv2userfile;
  /* V2 user permitted commands file (L.cmds).  */
  char *zv2cmds;

  /* HDB system file names (Systems).  */
  char **pzhdb_systems;
  /* HDB device file names (Devices).  */
  char **pzhdb_devices;
  /* HDB dialer file names (Dialers).  */
  char **pzhdb_dialers;
  /* Whether the HDB Permissions file has been read.  */
  boolean fhdb_read_permissions;
  /* The HDB Permissions file entries.  */
  struct shpermissions *qhdb_permissions;
};

/* This structure is used to hold the "unknown" commands from the
   Taylor UUCP config file before they have been parsed.  */

struct sunknown
{
  /* Next element in linked list.  */
  struct sunknown *qnext;
  /* Line number in config file.  */
  int ilineno;
  /* Number of arguments.  */
  int cargs;
  /* Arguments.  */
  char **pzargs;
};     

/* This structure is used to hold the locations of systems within the
   Taylor UUCP sys files.  */

struct stsysloc
{
  /* Next element in linked list.  */
  struct stsysloc *qnext;
  /* System name.  */
  const char *zname;
  /* Whether system is an alias or a real system.  If this is an
     alias, the real system is the next entry in the linked list which
     is not an alias.  */
  boolean falias;
  /* File name (one of the sys files).  */
  const char *zfile;
  /* Open file.  */
  FILE *e;
  /* Location within file (from ftell).  */
  long iloc;
  /* Line number within file.  */
  int ilineno;
};

/* This structure is used to hold validation restrictions.  This is a
   list of machines which are permitted to use a particular login
   name.  If a machine logs in, and there is no called login entry for
   it, the login name and machine name must be passed to
   uuconf_validate to confirm that either there is no entry for this
   login name or that the machine name appears on the entry.  */

struct svalidate
{
  /* Next element in linked list.  */
  struct svalidate *qnext;
  /* Login name.  */
  const char *zlogname;
  /* NULL terminated list of machine names.  */
  char **pzmachines;
};

/* This structure is used to hold a linked list of HDB Permissions
   file entries.  */

struct shpermissions
{
  /* Next entry in linked list.  */
  struct shpermissions *qnext;
  /* NULL terminated array of LOGNAME values.   */
  char **pzlogname;
  /* NULL terminated array of MACHINE values.  */
  char **pzmachine;
  /* Boolean REQUEST value.  */
  int frequest;
  /* Boolean SENDFILES value ("call" is taken as "no").  */
  int fsendfiles;
  /* NULL terminated array of READ values.  */
  char **pzread;
  /* NULL terminated array of WRITE values.  */
  char **pzwrite;
  /* Boolean CALLBACK value.  */
  int fcallback;
  /* NULL terminated array of COMMANDS values.  */
  char **pzcommands;
  /* NULL terminated array of VALIDATE values.  */
  char **pzvalidate;
  /* String MYNAME value.  */
  char *zmyname;
  /* String PUBDIR value.  */
  const char *zpubdir;
  /* NULL terminated array of ALIAS values.  */
  char **pzalias;
};

/* This structure is used to build reentrant uuconf_cmdtab tables.
   The ioff field is either (size_t) -1 or an offsetof macro.  The
   table is then copied into a uuconf_cmdtab, except that offsets of
   (size_t) -1 are converted to pvar elements of NULL, and other
   offsets are converted to an offset off some base address.  */

struct cmdtab_offset
{
  const char *zcmd;
  int itype;
  size_t ioff;
  uuconf_cmdtabfn pifn;
};

/* A value in a uuconf_system structure which holds the address of
   this special variable is known to be uninitialized.  */
extern char *_uuconf_unset;

/* Internal function to read a system from the Taylor UUCP
   configuration files.  This does not apply the basic defaults.  */
extern int _uuconf_itaylor_system_internal P((struct sglobal *qglobal,
					      const char *zsystem,
					      struct uuconf_system *qsys));

/* Read the system locations and validation information from the
   Taylor UUCP configuration files.  This sets the qsyslocs,
   qvalidate, and fread_syslocs elements of the global structure.  */
extern int _uuconf_iread_locations P((struct sglobal *qglobal));

/* Process a command for a port from a Taylor UUCP file.  */
extern int _uuconf_iport_cmd P((struct sglobal *qglobal, int argc,
				char **argv, struct uuconf_port *qport));

/* Process a command for a dialer from a Taylor UUCP file.  */
extern int _uuconf_idialer_cmd P((struct sglobal *qglobal, int argc,
				  char **argv,
				  struct uuconf_dialer *qdialer));

/* Process a command for a chat script from a Taylor UUCP file; this
   is also called for HDB or V2 files, with a made up command.  */
extern int _uuconf_ichat_cmd P((struct sglobal *qglobal, int argc,
				char **argv, struct uuconf_chat *qchat,
				pointer pblock));

/* Process a protocol-parameter command from a Taylor UUCP file.  */
extern int _uuconf_iadd_proto_param P((struct sglobal *qglobal,
				       int argc, char **argv,
				       struct uuconf_proto_param **pq,
				       pointer pblock));

/* Handle a "seven-bit", "reliable", or "half-duplex" command from a
   Taylor UUCP port or dialer file.  The pvar field should point to
   the ireliable element of the structure.  */
extern int _uuconf_iseven_bit P((pointer pglobal, int argc, char **argv,
				 pointer pvar, pointer pinfo));
extern int _uuconf_ireliable P((pointer pglobal, int argc, char **argv,
				pointer pvar, pointer pinfo));
extern int _uuconf_ihalf_duplex P((pointer pglobal, int argc, char **argv,
				   pointer pvar, pointer pinfo));

/* Internal function to read a system from the V2 configuration files.
   This does not apply the basic defaults.  */
extern int _uuconf_iv2_system_internal P((struct sglobal *qglobal,
					  const char *zsystem,
					  struct uuconf_system *qsys));

/* Internal function to read a system from the HDB configuration
   files.  This does not apply the basic defaults.  */
extern int _uuconf_ihdb_system_internal P((struct sglobal *qglobal,
					   const char *zsystem,
					   struct uuconf_system *qsys));

/* Read the HDB Permissions file.  */
extern int _uuconf_ihread_permissions P((struct sglobal *qglobal));

/* Initialize the global information structure.  */
extern int _uuconf_iinit_global P((struct sglobal **pqglobal));

/* Clear system information.  */
extern void _uuconf_uclear_system P((struct uuconf_system *qsys));

/* Default unset aspects of one system to the contents of another.  */
extern int _uuconf_isystem_default P((struct sglobal *qglobal,
				      struct uuconf_system *q,
				      struct uuconf_system *qdefault,
				      boolean faddalternates));

/* Put in the basic system defaults.  */
extern int _uuconf_isystem_basic_default P((struct sglobal *qglobal,
					    struct uuconf_system *qsys));

/* Clear port information.  */
extern void _uuconf_uclear_port P((struct uuconf_port *qport));

/* Clear dialer information.  */
extern void _uuconf_uclear_dialer P((struct uuconf_dialer *qdialer));

/* Add a timetable.  */
extern int _uuconf_itimetable P((pointer pglobal, int argc, char **argv,
				 pointer pvar, pointer pinfo));

/* Parse a time string.  */
extern int _uuconf_itime_parse P((struct sglobal *qglobal, char *ztime,
				  long ival, int cretry,
				  int (*picmp) P((long, long)),
				  struct uuconf_timespan **pqspan,
				  pointer pblock));

/* A grade comparison function to pass to _uuconf_itime_parse.  */
extern int _uuconf_itime_grade_cmp P((long, long));

/* Parse a debugging string.  */

extern int _uuconf_idebug_cmd P((struct sglobal *qglobal,
				 char **pzdebug, int argc,
				 char **argv, pointer pblock));

/* Add a string to a NULL terminated list of strings.  */
extern int _uuconf_iadd_string P((struct sglobal *qglobal,
				  char *zadd, boolean fcopy,
				  boolean fdupcheck, char ***ppzstrings,
				  pointer pblock));

/* Parse a string into a boolean value.  */
extern int _uuconf_iboolean P((struct sglobal *qglobal, const char *zval,
			       int *pi));

/* Parse a string into an integer value.  The argument p is either an
   int * or a long *, according to the argument fint.  */
extern int _uuconf_iint P((struct sglobal *qglobal, const char *zval,
			   pointer p, boolean fint));

/* Turn a cmdtab_offset table into a uuconf_cmdtab table.  */
extern void _uuconf_ucmdtab_base P((const struct cmdtab_offset *qoff,
				    size_t celes, char *pbase,
				    struct uuconf_cmdtab *qset));

/* Merge two memory blocks into one.  This cannot fail.  */
extern pointer _uuconf_pmalloc_block_merge P((pointer, pointer));

/* A wrapper for getline that continues lines if they end in a
   backslash.  It needs qglobal so that it can increment ilineno
   correctly.  */
extern int _uuconf_getline P((struct sglobal *qglobal,
			      char **, size_t *, FILE *));

/* Split a string into tokens.  */
extern int _uuconf_istrsplit P((char *zline, int bsep,
				char ***ppzsplit, size_t *csplit));
