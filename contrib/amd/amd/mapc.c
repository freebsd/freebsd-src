/*
 * Copyright (c) 1997-2003 Erez Zadok
 * Copyright (c) 1989 Jan-Simon Pendry
 * Copyright (c) 1989 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      %W% (Berkeley) %G%
 *
 * $Id: mapc.c,v 1.7.2.6 2002/12/27 22:44:38 ezk Exp $
 *
 */

/*
 * Mount map cache
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/*
 * Make a duplicate reference to an existing map
 */
#define mapc_dup(m) ((m)->refc++, (m))

/*
 * Map cache types
 * default, none, incremental, all, regexp
 * MAPC_RE implies MAPC_ALL and must be numerically
 * greater.
 */
#define	MAPC_DFLT	0x000
#define	MAPC_NONE	0x001
#define	MAPC_INC	0x002
#define	MAPC_ROOT	0x004
#define	MAPC_ALL	0x010
#define	MAPC_CACHE_MASK	0x0ff
#define	MAPC_SYNC	0x100

#ifdef HAVE_REGEXEC
# define	MAPC_RE		0x020
# define	MAPC_ISRE(m)	((m)->alloc == MAPC_RE)
#else /* not HAVE_REGEXEC */
# define	MAPC_ISRE(m)	FALSE
#endif /* not HAVE_REGEXEC */

/*
 * Lookup recursion
 */
#define	MREC_FULL	2
#define	MREC_PART	1
#define	MREC_NONE	0

#define MAX_CHAIN	2048

static struct opt_tab mapc_opt[] =
{
  {"all", MAPC_ALL},
  {"default", MAPC_DFLT},
  {"inc", MAPC_INC},
  {"mapdefault", MAPC_DFLT},
  {"none", MAPC_NONE},
#ifdef HAVE_REGEXEC
  {"re", MAPC_RE},
  {"regexp", MAPC_RE},
#endif /* HAVE_REGEXEC */
  {"sync", MAPC_SYNC},
  {0, 0}
};

/*
 * Wildcard key
 */
static char wildcard[] = "*";

/*
 * Map type
 */
typedef struct map_type map_type;
struct map_type {
  char *name;			/* Name of this map type */
  init_fn *init;		/* Initialization */
  reload_fn *reload;		/* Reload or fill */
  isup_fn *isup;		/* Is service up or not? (1=up, 0=down) */
  search_fn *search;		/* Search for new entry */
  mtime_fn *mtime;		/* Find modify time */
  int def_alloc;		/* Default allocation mode */
};

/*
 * Map for root node
 */
static mnt_map *root_map;

/*
 * List of known maps
 */
qelem map_list_head = {&map_list_head, &map_list_head};

/*
 * Configuration
 */

/* forward definitions */
static const char *get_full_path(const char *map, const char *path, const char *type);
static int mapc_meta_search(mnt_map *, char *, char **, int);
static void mapc_sync(mnt_map *);
static void mapc_clear(mnt_map *);

/* ROOT MAP */
static int root_init(mnt_map *, char *, time_t *);

/* ERROR MAP */
static int error_init(mnt_map *, char *, time_t *);
static int error_reload(mnt_map *, char *, add_fn *);
static int error_search(mnt_map *, char *, char *, char **, time_t *);
static int error_mtime(mnt_map *, char *, time_t *);

/* PASSWD MAPS */
#ifdef HAVE_MAP_PASSWD
extern int passwd_init(mnt_map *, char *, time_t *);
extern int passwd_search(mnt_map *, char *, char *, char **, time_t *);
#endif /* HAVE_MAP_PASSWD */

/* HESIOD MAPS */
#ifdef HAVE_MAP_HESIOD
extern int amu_hesiod_init(mnt_map *, char *map, time_t *tp);
extern int hesiod_isup(mnt_map *, char *);
extern int hesiod_search(mnt_map *, char *, char *, char **, time_t *);
#endif /* HAVE_MAP_HESIOD */

/* LDAP MAPS */
#ifdef HAVE_MAP_LDAP
extern int amu_ldap_init(mnt_map *, char *map, time_t *tp);
extern int amu_ldap_search(mnt_map *, char *, char *, char **, time_t *);
extern int amu_ldap_mtime(mnt_map *, char *, time_t *);
#endif /* HAVE_MAP_LDAP */

/* UNION MAPS */
#ifdef HAVE_MAP_UNION
extern int union_init(mnt_map *, char *, time_t *);
extern int union_search(mnt_map *, char *, char *, char **, time_t *);
extern int union_reload(mnt_map *, char *, add_fn *);
#endif /* HAVE_MAP_UNION */

/* Network Information Service PLUS (NIS+) */
#ifdef HAVE_MAP_NISPLUS
extern int nisplus_init(mnt_map *, char *, time_t *);
extern int nisplus_reload(mnt_map *, char *, add_fn *);
extern int nisplus_search(mnt_map *, char *, char *, char **, time_t *);
extern int nisplus_mtime(mnt_map *, char *, time_t *);
#endif /* HAVE_MAP_NISPLUS */

/* Network Information Service (YP, Yellow Pages) */
#ifdef HAVE_MAP_NIS
extern int nis_init(mnt_map *, char *, time_t *);
extern int nis_reload(mnt_map *, char *, add_fn *);
extern int nis_isup(mnt_map *, char *);
extern int nis_search(mnt_map *, char *, char *, char **, time_t *);
extern int nis_mtime(mnt_map *, char *, time_t *);
#endif /* HAVE_MAP_NIS */

/* NDBM MAPS */
#ifdef HAVE_MAP_NDBM
extern int ndbm_init(mnt_map *, char *, time_t *);
extern int ndbm_search(mnt_map *, char *, char *, char **, time_t *);
extern int ndbm_mtime(mnt_map *, char *, time_t *);
#endif /* HAVE_MAP_NDBM */

/* FILE MAPS */
#ifdef HAVE_MAP_FILE
extern int file_init(mnt_map *, char *, time_t *);
extern int file_reload(mnt_map *, char *, add_fn *);
extern int file_search(mnt_map *, char *, char *, char **, time_t *);
extern int file_mtime(mnt_map *, char *, time_t *);
#endif /* HAVE_MAP_FILE */


/* note that the choice of MAPC_{INC,ALL} will affect browsable_dirs */
static map_type maptypes[] =
{
  {
    "root",
    root_init,
    error_reload,
    NULL,			/* isup function */
    error_search,
    error_mtime,
    MAPC_ROOT
  },
#ifdef HAVE_MAP_PASSWD
  {
    "passwd",
    passwd_init,
    error_reload,
    NULL,			/* isup function */
    passwd_search,
    error_mtime,
    MAPC_INC
  },
#endif /* HAVE_MAP_PASSWD */
#ifdef HAVE_MAP_HESIOD
  {
    "hesiod",
    amu_hesiod_init,
    error_reload,
    hesiod_isup,		/* is Hesiod up or not? */
    hesiod_search,
    error_mtime,
    MAPC_INC
  },
#endif /* HAVE_MAP_HESIOD */
#ifdef HAVE_MAP_LDAP
  {
    "ldap",
    amu_ldap_init,
    error_reload,
    NULL,			/* isup function */
    amu_ldap_search,
    amu_ldap_mtime,
    MAPC_INC
  },
#endif /* HAVE_MAP_LDAP */
#ifdef HAVE_MAP_UNION
  {
    "union",
    union_init,
    union_reload,
    NULL,			/* isup function */
    union_search,
    error_mtime,
    MAPC_ALL
  },
#endif /* HAVE_MAP_UNION */
#ifdef HAVE_MAP_NISPLUS
  {
    "nisplus",
    nisplus_init,
    nisplus_reload,
    NULL,			/* isup function */
    nisplus_search,
    nisplus_mtime,
    MAPC_INC
  },
#endif /* HAVE_MAP_NISPLUS */
#ifdef HAVE_MAP_NIS
  {
    "nis",
    nis_init,
    nis_reload,
    nis_isup,			/* is NIS up or not? */
    nis_search,
    nis_mtime,
    MAPC_ALL
  },
#endif /* HAVE_MAP_NIS */
#ifdef HAVE_MAP_NDBM
  {
    "ndbm",
    ndbm_init,
    error_reload,
    NULL,			/* isup function */
    ndbm_search,
    ndbm_mtime,
    MAPC_INC
  },
#endif /* HAVE_MAP_NDBM */
#ifdef HAVE_MAP_FILE
  {
    "file",
    file_init,
    file_reload,
    NULL,			/* isup function */
    file_search,
    file_mtime,
    MAPC_ALL
  },
#endif /* HAVE_MAP_FILE */
  {
    "error",
    error_init,
    error_reload,
    NULL,			/* isup function */
    error_search,
    error_mtime,
    MAPC_NONE
  },
};


/*
 * Hash function
 */
static u_int
kvhash_of(char *key)
{
  u_int i, j;

  for (i = 0; (j = *key++); i += j) ;

  return i % NKVHASH;
}


void
mapc_showtypes(char *buf)
{
  map_type *mt;
  char *sep = "";

  buf[0] = '\0';
  for (mt = maptypes; mt < maptypes + sizeof(maptypes) / sizeof(maptypes[0]); mt++) {
    strcat(buf, sep);
    strcat(buf, mt->name);
    sep = ", ";
  }
}


/*
 * Check if a map of a certain type exists.
 * Return 1 (true) if exists, 0 (false) if not.
 */
int
mapc_type_exists(const char *type)
{
  map_type *mt;

  if (!type)
    return 0;
  for (mt = maptypes;
       mt < maptypes + sizeof(maptypes) / sizeof(maptypes[0]);
       mt++) {
    if (STREQ(type, mt->name))
      return 1;
  }
  return 0;			/* not found anywhere */
}


/*
 * Add key and val to the map m.
 * key and val are assumed to be safe copies
 */
void
mapc_add_kv(mnt_map *m, char *key, char *val)
{
  kv **h;
  kv *n;
  int hash = kvhash_of(key);
#ifdef HAVE_REGEXEC
  regex_t re;
#endif /* HAVE_REGEXEC */

#ifdef DEBUG
  dlog("add_kv: %s -> %s", key, val);
#endif /* DEBUG */

#ifdef HAVE_REGEXEC
  if (MAPC_ISRE(m)) {
    char pattern[MAXPATHLEN];
    int retval;

    /*
     * Make sure the string is bound to the start and end
     */
    sprintf(pattern, "^%s$", key);
    retval = regcomp(&re, pattern, REG_ICASE);
    if (retval != 0) {
      char errstr[256];

      /* XXX: this code was recently ported, and must be tested -Erez */
      errstr[0] = '\0';
      regerror(retval, &re, errstr, 256);
      plog(XLOG_USER, "error compiling RE \"%s\": %s", pattern, errstr);
      return;
    }
  }
#endif /* HAVE_REGEXEC */

  h = &m->kvhash[hash];
  n = ALLOC(struct kv);
  n->key = key;
#ifdef HAVE_REGEXEC
  memcpy(&n->re, &re, sizeof(regex_t));
#endif /* HAVE_REGEXEC */
  n->val = val;
  n->next = *h;
  *h = n;
}


static void
mapc_repl_kv(mnt_map *m, char *key, char *val)
{
  kv *k;

  /*
   * Compute the hash table offset
   */
  k = m->kvhash[kvhash_of(key)];

  /*
   * Scan the linked list for the key
   */
  while (k && !FSTREQ(k->key, key))
    k = k->next;

  if (k) {
    XFREE(k->val);
    k->val = val;
  } else {
    mapc_add_kv(m, key, val);
  }
}


/*
 * Search a map for a key.
 * Calls map specific search routine.
 * While map is out of date, keep re-syncing.
 */
static int
search_map(mnt_map *m, char *key, char **valp)
{
  int rc;

  do {
    rc = (*m->search) (m, m->map_name, key, valp, &m->modify);
    if (rc < 0) {
      plog(XLOG_MAP, "Re-synchronizing cache for map %s", m->map_name);
      mapc_sync(m);
    }
  } while (rc < 0);

  return rc;
}


/*
 * Do a wildcard lookup in the map and
 * save the result.
 */
static void
mapc_find_wildcard(mnt_map *m)
{
  /*
   * Attempt to find the wildcard entry
   */
  int rc = search_map(m, wildcard, &m->wildcard);

  if (rc != 0)
    m->wildcard = 0;
}


/*
 * Do a map reload.
 * Attempt to reload without losing current data by switching the hashes
 * round.
 */
static void
mapc_reload_map(mnt_map *m)
{
  int error;
  kv *maphash[NKVHASH], *tmphash[NKVHASH];
  time_t t;

  error = (*m->mtime) (m, m->map_name, &t);
  if (error) {
    t = m->modify;
  }

  /*
   * skip reloading maps that have not been modified, unless
   * amq -f was used (do_mapc_reload is 0)
   */
  if (m->reloads != 0 && do_mapc_reload != 0) {
    if (t <= m->modify) {
      plog(XLOG_INFO, "reload of map %s is not needed (in sync)", m->map_name);
#ifdef DEBUG
      dlog("map %s last load time is %d, last modify time is %d",
	   m->map_name, (int) m->modify, (int) t);
#endif /* DEBUG */
      return;
    }
  }

  /* copy the old hash and zero the map */
  memcpy((voidp) maphash, (voidp) m->kvhash, sizeof(m->kvhash));
  memset((voidp) m->kvhash, 0, sizeof(m->kvhash));

#ifdef DEBUG
  dlog("calling map reload on %s", m->map_name);
#endif /* DEBUG */
  error = (*m->reload) (m, m->map_name, mapc_add_kv);
  if (error) {
    if (m->reloads == 0)
      plog(XLOG_FATAL, "first time load of map %s failed!", m->map_name);
    else
      plog(XLOG_ERROR, "reload of map %s failed - using old values",
	   m->map_name);
    mapc_clear(m);
    memcpy((voidp) m->kvhash, (voidp) maphash, sizeof(m->kvhash));
  } else {
    if (m->reloads++ == 0)
      plog(XLOG_INFO, "first time load of map %s succeeded", m->map_name);
    else
      plog(XLOG_INFO, "reload #%d of map %s succeeded",
	   m->reloads, m->map_name);
    memcpy((voidp) tmphash, (voidp) m->kvhash, sizeof(m->kvhash));
    memcpy((voidp) m->kvhash, (voidp) maphash, sizeof(m->kvhash));
    mapc_clear(m);
    memcpy((voidp) m->kvhash, (voidp) tmphash, sizeof(m->kvhash));
    m->modify = t;
  }
  m->wildcard = 0;

#ifdef DEBUG
  dlog("calling mapc_search for wildcard");
#endif /* DEBUG */
  error = mapc_search(m, wildcard, &m->wildcard);
  if (error)
    m->wildcard = 0;
}


/*
 * Create a new map
 */
static mnt_map *
mapc_create(char *map, char *opt, const char *type)
{
  mnt_map *m = ALLOC(struct mnt_map);
  map_type *mt;
  time_t modify;
  int alloc = 0;

  cmdoption(opt, mapc_opt, &alloc);

  /*
   * If using a configuration file, and the map_type is defined, then look
   * for it, in the maptypes array.  If found, initialize the map using that
   * map_type.  If not found, return error.  If no map_type was defined,
   * default to cycling through all maptypes.
   */
  if (use_conf_file && type) {
    /* find what type of map this one is */
    for (mt = maptypes;
	 mt < maptypes + sizeof(maptypes) / sizeof(maptypes[0]);
	 mt++) {
      if (STREQ(type, mt->name)) {
	plog(XLOG_INFO, "initializing amd.conf map %s of type %s", map, type);
	if ((*mt->init) (m, map, &modify) == 0) {
	  break;
	} else {
	  plog(XLOG_ERROR, "failed to initialize map %s", map);
	  error_init(m, map, &modify);
	  break;
	}
      }
    } /* end of "for (mt =" loop */

  } else {			/* cycle through all known maptypes */

    /*
     * not using amd conf file or using it by w/o specifying map type
     */
    for (mt = maptypes;
	 mt < maptypes + sizeof(maptypes) / sizeof(maptypes[0]);
	 mt++) {
#ifdef DEBUG
      dlog("trying to initialize map %s of type %s ...", map, mt->name);
#endif /* DEBUG */
      if ((*mt->init) (m, map, &modify) == 0) {
	break;
      }
    }
  } /* end of "if (use_conf_file && (colpos = strchr ..." statement */

  /* assert: mt in maptypes */

  m->flags = alloc & ~MAPC_CACHE_MASK;
  alloc &= MAPC_CACHE_MASK;

  if (alloc == MAPC_DFLT)
    alloc = mt->def_alloc;

  switch (alloc) {
  default:
    plog(XLOG_USER, "Ambiguous map cache type \"%s\"; using \"inc\"", opt);
    alloc = MAPC_INC;
    /* fall-through... */
  case MAPC_NONE:
  case MAPC_INC:
  case MAPC_ROOT:
    break;

  case MAPC_ALL:
    /*
     * If there is no support for reload and it was requested
     * then back off to incremental instead.
     */
    if (mt->reload == error_reload) {
      plog(XLOG_WARNING, "Map type \"%s\" does not support cache type \"all\"; using \"inc\"", mt->name);
      alloc = MAPC_INC;
    }
    break;

#ifdef HAVE_REGEXEC
  case MAPC_RE:
    if (mt->reload == error_reload) {
      plog(XLOG_WARNING, "Map type \"%s\" does not support cache type \"re\"", mt->name);
      mt = &maptypes[sizeof(maptypes) / sizeof(maptypes[0]) - 1];
      /* assert: mt->name == "error" */
    }
    break;
#endif /* HAVE_REGEXEC */
  }

#ifdef DEBUG
  dlog("Map for %s coming from maptype %s", map, mt->name);
#endif /* DEBUG */

  m->alloc = alloc;
  m->reload = mt->reload;
  m->isup = mt->isup;
  m->modify = modify;
  m->search = alloc >= MAPC_ALL ? error_search : mt->search;
  m->mtime = mt->mtime;
  memset((voidp) m->kvhash, 0, sizeof(m->kvhash));
  m->map_name = strdup(map);
  m->refc = 1;
  m->wildcard = 0;
  m->reloads = 0;

  /*
   * synchronize cache with reality
   */
  mapc_sync(m);

  return m;
}


/*
 * Free the cached data in a map
 */
static void
mapc_clear(mnt_map *m)
{
  int i;

  /*
   * For each of the hash slots, chain
   * along free'ing the data.
   */
  for (i = 0; i < NKVHASH; i++) {
    kv *k = m->kvhash[i];
    while (k) {
      kv *n = k->next;
      XFREE(k->key);
      if (k->val)
	XFREE(k->val);
      XFREE(k);
      k = n;
    }
  }

  /*
   * Zero the hash slots
   */
  memset((voidp) m->kvhash, 0, sizeof(m->kvhash));

  /*
   * Free the wildcard if it exists
   */
  if (m->wildcard) {
    XFREE(m->wildcard);
    m->wildcard = 0;
  }
}


/*
 * Find a map, or create one if it does not exist
 */
mnt_map *
mapc_find(char *map, char *opt, const char *maptype)
{
  mnt_map *m;

  /*
   * Search the list of known maps to see if
   * it has already been loaded.  If it is found
   * then return a duplicate reference to it.
   * Otherwise make a new map as required and
   * add it to the list of maps
   */
  ITER(m, mnt_map, &map_list_head)
  if (STREQ(m->map_name, map))
      return mapc_dup(m);
  m = mapc_create(map, opt, maptype);
  ins_que(&m->hdr, &map_list_head);

  return m;
}


/*
 * Free a map.
 */
void
mapc_free(voidp v)
{
  mnt_map *m = v;

  /*
   * Decrement the reference count.
   * If the reference count hits zero
   * then throw the map away.
   */
  if (m && --m->refc == 0) {
    mapc_clear(m);
    XFREE(m->map_name);
    rem_que(&m->hdr);
    XFREE(m);
  }
}


/*
 * Search the map for the key.  Put a safe (malloc'ed) copy in *pval or
 * return an error code
 */
static int
mapc_meta_search(mnt_map *m, char *key, char **pval, int recurse)
{
  int error = 0;
  kv *k = 0;

  /*
   * Firewall
   */
  if (!m) {
    plog(XLOG_ERROR, "Null map request for %s", key);
    return ENOENT;
  }

  if (m->flags & MAPC_SYNC) {
    /*
     * Get modify time...
     */
    time_t t;
    error = (*m->mtime) (m, m->map_name, &t);
    if (error || t > m->modify) {
      plog(XLOG_INFO, "Map %s is out of date", m->map_name);
      mapc_sync(m);
    }
  }

  if (!MAPC_ISRE(m)) {
    /*
     * Compute the hash table offset
     */
    k = m->kvhash[kvhash_of(key)];

    /*
     * Scan the linked list for the key
     */
    while (k && !FSTREQ(k->key, key))
      k = k->next;

  }

#ifdef HAVE_REGEXEC
  else if (recurse == MREC_FULL) {
    /*
     * Try for an RE match against the entire map.
     * Note that this will be done in a "random"
     * order.
     */
    int i;

    for (i = 0; i < NKVHASH; i++) {
      k = m->kvhash[i];
      while (k) {
	int retval;

	/* XXX: this code was recently ported, and must be tested -Erez */
	retval = regexec(&k->re, key, 0, 0, 0);
	if (retval == 0) {	/* succeeded */
	  break;
	} else {		/* failed to match, log error */
	  char errstr[256];

	  errstr[0] = '\0';
	  regerror(retval, &k->re, errstr, 256);
	  plog(XLOG_USER, "error matching RE \"%s\" against \"%s\": %s",
	       key, k->key, errstr);
	}
	k = k->next;
      }
      if (k)
	break;
    }
  }
#endif /* HAVE_REGEXEC */

  /*
   * If found then take a copy
   */
  if (k) {
    if (k->val)
      *pval = strdup(k->val);
    else
      error = ENOENT;
  } else if (m->alloc >= MAPC_ALL) {
    /*
     * If the entire map is cached then this
     * key does not exist.
     */
    error = ENOENT;
  } else {
    /*
     * Otherwise search the map.  If we are
     * in incremental mode then add the key
     * to the cache.
     */
    error = search_map(m, key, pval);
    if (!error && m->alloc == MAPC_INC)
      mapc_add_kv(m, strdup(key), strdup(*pval));
  }

  /*
   * If an error, and a wildcard exists,
   * and the key is not internal then
   * return a copy of the wildcard.
   */
  if (error > 0) {
    if (recurse == MREC_FULL && !MAPC_ISRE(m)) {
      char wildname[MAXPATHLEN];
      char *subp;
      if (*key == '/')
	return error;
      /*
       * Keep chopping sub-directories from the RHS
       * and replacing with "/ *" and repeat the lookup.
       * For example:
       * "src/gnu/gcc" -> "src / gnu / *" -> "src / *"
       */
      strcpy(wildname, key);
      while (error && (subp = strrchr(wildname, '/'))) {
	strcpy(subp, "/*");
#ifdef DEBUG
	dlog("mapc recurses on %s", wildname);
#endif /* DEBUG */
	error = mapc_meta_search(m, wildname, pval, MREC_PART);
	if (error)
	  *subp = 0;
      }

      if (error > 0 && m->wildcard) {
	*pval = strdup(m->wildcard);
	error = 0;
      }
    }
  }
  return error;
}


int
mapc_search(mnt_map *m, char *key, char **pval)
{
  return mapc_meta_search(m, key, pval, MREC_FULL);
}


/*
 * Get map cache in sync with physical representation
 */
static void
mapc_sync(mnt_map *m)
{
  if (m->alloc != MAPC_ROOT) {

    /* do not clear map if map service is down */
    if (m->isup) {
      if (!((*m->isup)(m, m->map_name))) {
	plog(XLOG_ERROR, "mapc_sync: map %s is down: not clearing map", m->map_name);
	return;
      }
    }

    if (m->alloc >= MAPC_ALL) {
      /* mapc_reload_map() always works */
      mapc_reload_map(m);
    } else {
      mapc_clear(m);
      /*
       * Attempt to find the wildcard entry
       */
      mapc_find_wildcard(m);
    }
  }
}


/*
 * Reload all the maps
 * Called when Amd gets hit by a SIGHUP.
 */
void
mapc_reload(void)
{
  mnt_map *m;

  /*
   * For all the maps,
   * Throw away the existing information.
   * Do a reload
   * Find the wildcard
   */
  ITER(m, mnt_map, &map_list_head)
    mapc_sync(m);
}


/*
 * Root map.
 * The root map is used to bootstrap amd.
 * All the require top-level mounts are added
 * into the root map and then the map is iterated
 * and a lookup is done on all the mount points.
 * This causes the top level mounts to be automounted.
 */
static int
root_init(mnt_map *m, char *map, time_t *tp)
{
  *tp = clocktime();
  return STREQ(map, ROOT_MAP) ? 0 : ENOENT;
}


/*
 * Add a new entry to the root map
 *
 * dir - directory (key)
 * opts - mount options
 * map - map name
 * cfm - optional amd configuration file map section structure
 */
void
root_newmap(const char *dir, const char *opts, const char *map, const cf_map_t *cfm)
{
  char str[MAXPATHLEN];

  /*
   * First make sure we have a root map to talk about...
   */
  if (!root_map)
    root_map = mapc_find(ROOT_MAP, "mapdefault", NULL);

  /*
   * Then add the entry...
   */

  /*
   * Here I plug in the code to process other amd.conf options like
   * map_type, search_path, and flags (browsable_dirs, mount_type).
   */

  if (cfm) {
    if (map) {
      sprintf(str, "cache:=mapdefault;type:=toplvl;fs:=\"%s\"",
	      get_full_path(map, cfm->cfm_search_path, cfm->cfm_type));
      if (opts && opts[0] != '\0') {
	strcat(str, ";");
	strcat(str, opts);
      }
      if (cfm->cfm_flags & CFM_BROWSABLE_DIRS_FULL)
	strcat(str, ";opts:=rw,fullybrowsable");
      if (cfm->cfm_flags & CFM_BROWSABLE_DIRS)
	strcat(str, ";opts:=rw,browsable");
      if (cfm->cfm_type) {
	strcat(str, ";maptype:=");
	strcat(str, cfm->cfm_type);
      }
    } else {
      strcpy(str, opts);
    }
  } else {
    if (map)
      sprintf(str, "cache:=mapdefault;type:=toplvl;fs:=\"%s\";%s",
	      map, opts ? opts : "");
    else
      strcpy(str, opts);
  }
  mapc_repl_kv(root_map, strdup((char *)dir), strdup(str));
}


int
mapc_keyiter(mnt_map *m, void (*fn) (char *, voidp), voidp arg)
{
  int i;
  int c = 0;

  for (i = 0; i < NKVHASH; i++) {
    kv *k = m->kvhash[i];
    while (k) {
      (*fn) (k->key, arg);
      k = k->next;
      c++;
    }
  }

  return c;
}


/*
 * Iterate on the root map and call (*fn)() on the key of all the nodes.
 * Finally throw away the root map.
 */
int
root_keyiter(void (*fn)(char *, voidp), voidp arg)
{
  if (root_map) {
    int c = mapc_keyiter(root_map, fn, arg);
    return c;
  }

  return 0;
}


/*
 * Was: NEW_TOPLVL_READDIR
 * Search a chain for an entry with some name.
 * -Erez Zadok <ezk@cs.columbia.edu>
 */
static int
key_already_in_chain(char *keyname, const nfsentry *chain)
{
  const nfsentry *tmpchain = chain;

  while (tmpchain) {
    if (keyname && tmpchain->ne_name && STREQ(keyname, tmpchain->ne_name))
        return 1;
    tmpchain = tmpchain->ne_nextentry;
  }

  return 0;
}


/*
 * Create a chain of entries which are not linked.
 * -Erez Zadok <ezk@cs.columbia.edu>
 */
nfsentry *
make_entry_chain(am_node *mp, const nfsentry *current_chain, int fully_browsable)
{
  static u_int last_cookie = (u_int) 2;	/* monotonically increasing */
  static nfsentry chain[MAX_CHAIN];
  static int max_entries = MAX_CHAIN;
  char *key;
  int num_entries = 0, preflen = 0, i;
  nfsentry *retval = (nfsentry *) NULL;
  mntfs *mf;
  mnt_map *mmp;

  if (!mp) {
    plog(XLOG_DEBUG, "make_entry_chain: mp is (NULL)");
    return retval;
  }
  mf = mp->am_mnt;
  if (!mf) {
    plog(XLOG_DEBUG, "make_entry_chain: mp->am_mnt is (NULL)");
    return retval;
  }
  mmp = (mnt_map *) mf->mf_private;
  if (!mmp) {
    plog(XLOG_DEBUG, "make_entry_chain: mp->am_mnt->mf_private is (NULL)");
    return retval;
  }

  if (mp->am_pref)
    preflen = strlen(mp->am_pref);

  /* iterate over keys */
  for (i = 0; i < NKVHASH; i++) {
    kv *k;
    for (k = mmp->kvhash[i]; k ; k = k->next) {

      /*
       * Skip unwanted entries which are either not real entries or
       * very difficult to interpret (wildcards...)  This test needs
       * lots of improvement.  Any takers?
       */
      key = k->key;
      if (!key)
	continue;

      /* Skip '*' */
      if (!fully_browsable && strchr(key, '*'))
	continue;

      /*
       * If the map has a prefix-string then check if the key starts with
       * this string, and if it does, skip over this prefix.  If it has a
       * prefix and it doesn't match the start of the key, skip it.
       */
      if (preflen && (preflen <= (strlen(key)))) {
	if (!NSTREQ(key, mp->am_pref, preflen))
	  continue;
	key += preflen;
      } else if (preflen) {
	  continue;
      }

      /* no more '/' are allowed, unless browsable_dirs=full was used */
      if (!fully_browsable && strchr(key, '/'))
	continue;

      /* no duplicates allowed */
      if (key_already_in_chain(key, current_chain))
	continue;

      /* fill in a cell and link the entry */
      if (num_entries >= max_entries) {
	/* out of space */
	plog(XLOG_DEBUG, "make_entry_chain: no more space in chain");
	if (num_entries > 0) {
	  chain[num_entries - 1].ne_nextentry = 0;
	  retval = &chain[0];
	}
	return retval;
      }

      /* we have space.  put entry in next cell */
      ++last_cookie;
      chain[num_entries].ne_fileid = (u_int) last_cookie;
      *(u_int *) chain[num_entries].ne_cookie = (u_int) last_cookie;
      chain[num_entries].ne_name = key;
      if (num_entries < max_entries - 1) {	/* link to next one */
	chain[num_entries].ne_nextentry = &chain[num_entries + 1];
      }
      ++num_entries;
    } /* end of "while (k)" */
  } /* end of "for (i ... NKVHASH ..." */

  /* terminate chain */
  if (num_entries > 0) {
    chain[num_entries - 1].ne_nextentry = 0;
    retval = &chain[0];
  }

  return retval;
}


/*
 * Error map
 */
static int
error_init(mnt_map *m, char *map, time_t *tp)
{
  plog(XLOG_USER, "No source data for map %s", map);
  *tp = 0;

  return 0;
}


static int
error_search(mnt_map *m, char *map, char *key, char **pval, time_t *tp)
{
  return ENOENT;
}


static int
error_reload(mnt_map *m, char *map, add_fn *fn)
{
  return ENOENT;
}


static int
error_mtime(mnt_map *m, char *map, time_t *tp)
{
  *tp = 0;

  return 0;
}


/*
 * Return absolute path of map, searched in a type-specific path.
 * Note: uses a static buffer for returned data.
 */
static const char *
get_full_path(const char *map, const char *path, const char *type)
{
  char component[MAXPATHLEN], *str;
  static char full_path[MAXPATHLEN];
  int len;

  /* for now, only file-type search paths are implemented */
  if (type && !STREQ(type, "file"))
    return map;

  /* if null map, return it */
  if (!map)
    return map;

  /* if map includes a '/', return it (absolute or relative path) */
  if (strchr(map, '/'))
    return map;

  /* if path is empty, return map */
  if (!path)
    return map;

  /* now break path into components, and search in each */
  strcpy(component, path);

  str = strtok(component, ":");
  do {
    strcpy(full_path, str);
    len = strlen(full_path);
    if (full_path[len - 1] != '/') /* add trailing "/" if needed */
      strcat(full_path, "/");
    strcat(full_path, map);
    if (access(full_path, R_OK) == 0)
      return full_path;
    str = strtok(NULL, ":");
  } while (str);

  return map;			/* if found nothing, return map */
}
