#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_init.c	4.38 (Berkeley) 3/21/91";
static char rcsid[] = "$Id: ns_init.c,v 1.4 1995/10/23 11:11:46 peter Exp $";
#endif /* not lint */

/*
 * ++Copyright++ 1986, 1990
 * -
 * Copyright (c) 1986, 1990
 *    The Regents of the University of California.  All rights reserved.
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
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <resolv.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#include "named.h"

#undef nsaddr

enum limit { Datasize };

static void		zoneinit __P((struct zoneinfo *)),
			get_forwarders __P((FILE *)),
			boot_read __P((const char *filename, int includefile)),
#ifdef DEBUG
			content_zone __P((int)),
#endif
			free_forwarders __P((void)),
			ns_limit __P((const char *name, int value)),
			ns_rlimit __P((const char *name, enum limit limit,
				       long value)),
			ns_option __P((const char *name));

static struct zoneinfo	*find_zone __P((char *, int, int));

/*
 * Set new refresh time for zone.  Use a random number in the last half of
 * the refresh limit; we want it to be substantially correct while still
 * preventing slave synchronization.
 */
void
ns_refreshtime(zp, timebase)
	struct zoneinfo	*zp;
	time_t		timebase;
{
	u_long refresh = (zp->z_refresh > 0) ? zp->z_refresh : INIT_REFRESH;
	time_t half = (refresh + 1) / 2;

	zp->z_time = timebase + half + (rand() % half);
}

/*
 * Set new retry time for zone.
 */
void
ns_retrytime(zp, timebase)
	struct zoneinfo	*zp;
	time_t		timebase;
{
	zp->z_time = timebase + zp->z_retry;
}

/*
 * Read boot file for configuration info.
 */
void
ns_init(bootfile)
	char *bootfile;
{
	register struct zoneinfo *zp;
	static int loads = 0;			/* number of times loaded */

	dprintf(1, (ddt, "\nns_init(%s)\n", bootfile));
	gettime(&tt);

        if (loads == 0) {
		if ((zones =
			(struct zoneinfo *)calloc(64, sizeof(struct zoneinfo)))
					    == NULL) {
		    syslog(LOG_ERR,
			"Not enough memory to allocate initial zones array");
		    exit(1);
		}
		nzones = 1;		/* zone zero is cache data */
		/* allocate cache hash table, formerly the root hash table. */
		hashtab = savehash((struct hashbuf *)NULL);

		/* allocate root-hints/file-cache hash table */
		fcachetab = savehash((struct hashbuf *)NULL);
		/* init zone data */
		zones[0].z_type = Z_CACHE;
		zones[0].z_origin = "";
        } else {
		/* Mark previous zones as not yet found in boot file. */
		for (zp = &zones[1]; zp < &zones[nzones]; zp++)
			zp->z_flags &= ~Z_FOUND;
#ifdef LOCALDOM
		if (localdomain) {
			free(localdomain);
			localdomain = NULL;
		}
#endif
		free_forwarders();
		free_netlist(enettab);
#ifdef XFRNETS
		free_netlist(&xfrnets);
#endif
#ifdef BOGUSNS
		free_netlist(&boglist);
#endif
		forward_only = 0;
	}

	dprintf(3, (ddt, "\n content of zones before loading \n"));
#ifdef DEBUG
        if (debug >= 3) {
        	content_zone(nzones - 1);
        }
#endif
	boot_read(bootfile, 0);

        /* erase all old zones that were not found */
        for (zp = &zones[1]; zp < &zones[nzones]; zp++) {
            if (zp->z_type && (zp->z_flags & Z_FOUND) == 0) {
#ifdef CLEANCACHE
		remove_zone(hashtab, zp - zones, 1);
#else
		remove_zone(hashtab, zp - zones);
#endif
#ifdef SECURE_ZONES
		free_netlist(&zp->secure_nets);
#endif
		syslog(LOG_NOTICE, "Zone \"%s\" was removed", zp->z_origin);
		free(zp->z_origin);
		free(zp->z_source);
		bzero((char *) zp, sizeof(*zp));
	    }
        }
	dprintf(2, (ddt,"\n content of zones after loading\n"));

#ifdef DEBUG
	if (debug >= 2) {
        	content_zone(nzones-1);
        }
#endif

	/*
	 * Schedule calls to ns_maint().
	 */
	if (!needmaint)
		sched_maint();
	dprintf(1, (ddt, "exit ns_init()%s\n",
		    needmaint ? ", need maintenance immediately" : ""));
	loads++;
}

/*
 * Read the actual boot file.
 * Set up to recurse.
 */
static void
boot_read(filename, includefile)
	const char *filename;
	int includefile;
{
	register struct zoneinfo *zp;
	char buf[BUFSIZ], obuf[BUFSIZ], *source;
	FILE *fp;
	int type;
	int class;
#ifdef GEN_AXFR
	char *class_p;
#endif
	struct stat f_time;
	static int tmpnum = 0;		/* unique number for tmp zone files */
#ifdef ALLOW_UPDATES
	char *flag;
#endif
	int slineno;			/* Saved global line number. */
	int i;

	if ((fp = fopen(filename, "r")) == NULL) {
		syslog(LOG_ERR, "%s: %m", filename);
		if (includefile)
			return;
		exit(1);
	}

	slineno = lineno;
	lineno = 1;

	while (!feof(fp) && !ferror(fp)) {
		/* read named.boot keyword and process args */
		if (!getword(buf, sizeof(buf), fp, 0)) {
			/*
			 * This is a blank line, a commented line, or the
			 * '\n' of the previous line.
			 */
			continue;
		}
		if (strcasecmp(buf, "directory") == 0) {
			(void) getword(buf, sizeof(buf), fp, 0);
			if (chdir(buf) < 0) {
				syslog(LOG_CRIT, "directory %s: %m\n",
					buf);
				exit(1);
			}
			continue;
		} else if (strcasecmp(buf, "sortlist") == 0) {
			get_netlist(fp, enettab, ALLOW_NETS, buf);
			continue;
		} else if (strcasecmp(buf, "max-fetch") == 0) {
			max_xfers_running = getnum(fp, filename, GETNUM_NONE);
			continue;
		} else if (strcasecmp(buf, "limit") == 0) {
			(void) getword(buf, sizeof(buf), fp, 0);
			ns_limit(buf, getnum(fp, filename, GETNUM_SCALED));
			continue;
		} else if (strcasecmp(buf, "options") == 0) {
			while (getword(buf, sizeof(buf), fp, 0))
				ns_option(buf);
			continue;
		} else if (strcasecmp(buf, "forwarders") == 0) {
			get_forwarders(fp);
			continue;
		} else if (strcasecmp(buf, "slave") == 0) {
			forward_only++;
			continue;
#ifdef BOGUSNS
		} else if (strcasecmp(buf, "bogusns") == 0) {
			get_netlist(fp, &boglist, ALLOW_HOSTS, buf);
			continue;
#endif
#ifdef XFRNETS
		} else if ((strcasecmp(buf, "tcplist") == 0) ||
			   (strcasecmp(buf, "xfrnets") == 0)) {
			get_netlist(fp, &xfrnets, ALLOW_NETS, buf);
			continue;
#endif
#ifdef LOCALDOM
		} else if (strcasecmp(buf, "domain") == 0) {
			if (getword(buf, sizeof(buf), fp, 1))
				localdomain = savestr(buf);
			continue;
#endif
		} else if (strcasecmp(buf, "include") == 0) {
			if (getword(buf, sizeof(buf), fp, 0))
				boot_read(buf, 1);
			continue;
		} else if (strncasecmp(buf, "cache", 5) == 0) {
			type = Z_CACHE;
			class = C_IN;
#ifdef GEN_AXFR
			if (class_p = strchr(buf, '/')) {
				class = get_class(class_p+1);

				if (class != C_IN) {
					syslog(LOG_NOTICE,
		   "cache directive with non-IN class is not supported (yet)");
					endline(fp);
					continue;
				}
			}
#endif
		} else if (strncasecmp(buf, "primary", 7) == 0) {
			type = Z_PRIMARY;
			class = C_IN;
#ifdef GEN_AXFR
			if (class_p = strchr(buf, '/'))
				class = get_class(class_p+1);
#endif
		} else if (strncasecmp(buf, "secondary", 9) == 0) {
			type = Z_SECONDARY;
			class = C_IN;
#ifdef GEN_AXFR
			if (class_p = strchr(buf, '/'))
				class = get_class(class_p+1);
#endif
#ifdef STUBS
		} else if (strncasecmp(buf, "stub", 4) == 0) {
			type = Z_STUB;
			class = C_IN;
#ifdef GEN_AXFR
			if (class_p = strchr(buf, '/'))
				class = get_class(class_p+1);
#endif
#endif
		} else {
			syslog(LOG_NOTICE,
			       "%s: line %d: unknown directive '%s'\n",
			       filename, lineno, buf);
			endline(fp);
			continue;
		}

		/*
		 * read zone origin
		 */
		if (!getword(obuf, sizeof(obuf), fp, 1)) {
			syslog(LOG_NOTICE, "%s: line %d: missing origin\n",
			    filename, lineno);
			continue;
		}
		i = strlen(obuf);
		if ((obuf[i-1] == '.') && (i != 1))
			syslog(LOG_INFO,
			       "%s: line %d: zone \"%s\" has trailing dot\n",
			       filename, lineno, obuf);
		while ((--i >= 0) && (obuf[i] == '.'))
			obuf[i] = '\0';
		dprintf(1, (ddt, "zone origin %s", obuf[0]?obuf:"."));
		/*
		 * Read source file or host address.
		 */
		if (!getword(buf, sizeof(buf), fp, 0)) {
			syslog(LOG_NOTICE, "%s: line %d: missing %s\n",
				filename, lineno, 
#ifdef STUBS
			   (type == Z_SECONDARY || type == Z_STUB)
#else
			   (type == Z_SECONDARY)
#endif
			       ?"host address"
			       :"source file");
			continue;
		}

		/*
		 * Check for previous instance of this zone (reload).
		 */
		if (!(zp = find_zone(obuf, type, class))) {
			if (type == Z_CACHE) {
				zp = &zones[0];
				goto gotcache;
			}
			for (zp = &zones[1]; zp < &zones[nzones]; zp++)
				if (zp->z_type == Z_NIL)
					goto gotzone;
			/*
			 * This code assumes that nzones never decreases.
			 */
			if (nzones % 64 == 0) {
			    dprintf(1, (ddt,
					"Reallocating zones structure\n"));
			    /*
			     * Realloc() not used since it might damage zones
			     * if an error occurs.
			     */
			    zp = (struct zoneinfo *)
				malloc((64 + nzones)
				       * sizeof(struct zoneinfo));
			    if (!zp) {
				    syslog(LOG_NOTICE,
					   "no memory for more zones");
				    endline(fp);
				    continue;
			    }
			    bcopy((char *)zones, (char *)zp,
				  nzones * sizeof(struct zoneinfo));
			    bzero((char *)&zp[nzones],
				  64 * sizeof(struct zoneinfo));
			    free(zones);
			    zones = zp;
			}
			zp = &zones[nzones++];
	gotzone:
			zp->z_origin = savestr(obuf);
	gotcache:
			zp->z_type = type;
			zp->z_class = class;
		}
		zp->z_addrcnt = 0;

		switch (type) {
		case Z_CACHE:
			source = savestr(buf);
			dprintf(1, (ddt, ", source = %s\n", source));
			zp->z_refresh = 0;	/* by default, no dumping */
			if (getword(buf, sizeof(buf), fp, 0)) {
#ifdef notyet
				zp->z_refresh = atoi(buf);
				if (zp->z_refresh <= 0) {
					syslog(LOG_NOTICE,
				"%s: line %d: bad refresh time '%s', ignored\n",
						filename, lineno, buf);
					zp->z_refresh = 0;
				} else if (cache_file == NULL)
					cache_file = source;
#else
				syslog(LOG_NOTICE,
				       "%s: line %d: cache refresh ignored\n",
				       filename, lineno);
#endif
				endline(fp);
			}
			/*
			 * If we've loaded this file, and the file has
			 * not been modified and contains no $include,
			 * then there's no need to reload.
			 */
			if (zp->z_source &&
			    !strcmp(source, zp->z_source) &&
			    !(zp->z_flags & Z_INCLUDE) &&
			    stat(zp->z_source, &f_time) != -1 &&
			    zp->z_ftime == f_time.st_mtime) {
				dprintf(1, (ddt, "cache is up to date\n"));
				if (source != cache_file)
					free(source);
				break; /* zone is already up to date */
			}

			/* file has changed, or hasn't been loaded yet */
			if (zp->z_source) {
				free(zp->z_source);
#ifdef CLEANCACHE
				remove_zone(fcachetab, 0, 1);
#else
				remove_zone(fcachetab, 0);
#endif
			}
			zp->z_source = source;
			dprintf(1, (ddt, "reloading zone\n"));
			(void) db_load(zp->z_source, zp->z_origin, zp, NULL);
			break;

		case Z_PRIMARY:
			source = savestr(buf);
#ifdef ALLOW_UPDATES
			if (getword(buf, sizeof(buf), fp, 0)) {
				endline(fp);
				flag = buf;
				while (flag) {
				    char *cp = strchr(flag, ',');
				    if (cp)
					*cp++ = 0;
				    if (strcasecmp(flag, "dynamic") == 0)
					zp->z_flags |= Z_DYNAMIC;
				    else if (strcasecmp(flag, "addonly") == 0)
					zp->z_flags |= Z_DYNADDONLY;
				    else {
					syslog(LOG_NOTICE,
					       "%s: line %d: bad flag '%s'\n",
					       filename, lineno, flag);
				    }
				    flag = cp;
				}
			}
#else /*ALLOW_UPDATES*/
    		        endline(fp);
#endif

			dprintf(1, (ddt, ", source = %s\n", source));
			/*
			 * If we've loaded this file, and the file has
			 * not been modified and contains no $include,
			 * then there's no need to reload.
			 */
			if (zp->z_source &&
			    !strcmp(source, zp->z_source) &&
			    !(zp->z_flags & Z_INCLUDE) &&
			    stat(zp->z_source, &f_time) != -1 &&
			    zp->z_ftime == f_time.st_mtime) {
				dprintf(1, (ddt, "zone is up to date\n"));
				free(source);
				break; /* zone is already up to date */
			}
			if (zp->z_source) {
				free(zp->z_source);
#ifdef CLEANCACHE
				remove_zone(hashtab, zp - zones, 1);
#else
				remove_zone(hashtab, zp - zones);
#endif
			}
                        zp->z_source = source;
			zp->z_flags &= ~Z_AUTH;
#ifdef PURGE_ZONE
			purge_zone(zp->z_origin, hashtab, zp->z_class);
#endif
			dprintf(1, (ddt, "reloading zone\n"));
			if (!db_load(zp->z_source, zp->z_origin, zp, NULL))
				zp->z_flags |= Z_AUTH;
#ifdef ALLOW_UPDATES
			/* Guarantee calls to ns_maint() */
			zp->z_refresh = maint_interval;
#else
			zp->z_refresh = 0;	/* no maintenance needed */
			zp->z_time = 0;
#endif
			break;

		case Z_SECONDARY:
#ifdef STUBS
		case Z_STUB:
#endif
			source = NULL;
			dprintf(1, (ddt, "\n\taddrs: "));
			do {
				if (!inet_aton(buf,
					       &zp->z_addr[zp->z_addrcnt])
				    ) {
					source = savestr(buf);
    		                        endline(fp);
					break;
				}
				dprintf(1, (ddt, "%s, ", buf));
				if ((int)++zp->z_addrcnt > NSMAX - 1) {
					zp->z_addrcnt = NSMAX - 1;
					dprintf(1, (ddt,
						    "\nns.h NSMAX reached\n"));
				}
			} while (getword(buf, sizeof(buf), fp, 0));
			dprintf(1, (ddt, "addrcnt = %d\n", zp->z_addrcnt));
			if (!source) {
				/*
				 * We will always transfer this zone again
				 * after a reload.
				 */
				sprintf(buf, "%s/NsTmp%ld.%d", _PATH_TMPDIR,
					(long)getpid(), tmpnum++);
				source = savestr(buf);
				zp->z_flags |= Z_TMP_FILE;
			} else
				zp->z_flags &= ~Z_TMP_FILE;
			/*
			 * If we had a backup file name, and it was changed,
			 * free old zone and start over.  If we don't have
			 * current zone contents, try again now in case
			 * we have a new server on the list.
			 */
			if (zp->z_source &&
			    (strcmp(source, zp->z_source) ||
			     (stat(zp->z_source, &f_time) == -1 ||
			     (zp->z_ftime != f_time.st_mtime)))) {
				dprintf(1, (ddt, "backup file changed\n"));
				free(zp->z_source);
				zp->z_source = NULL;
				zp->z_flags &= ~Z_AUTH;
				zp->z_serial = 0;	/* force xfer */
#ifdef	CLEANCACHE
                        	remove_zone(hashtab, zp - zones, 1);
#else
                        	remove_zone(hashtab, zp - zones);
#endif
			}
			if (zp->z_source)
				free(source);
			else
				zp->z_source = source;
			if (!(zp->z_flags & Z_AUTH))
				zoneinit(zp);
#ifdef FORCED_RELOAD
			else {
				/*
				** Force secondary to try transfer right away
				** after SIGHUP.
				*/
				if (!(zp->z_flags & (Z_QSERIAL|Z_XFER_RUNNING))
				    && reloading) {
					zp->z_time = tt.tv_sec;
					needmaint = 1;
				}
			}
#endif /* FORCED_RELOAD */
			break;

		}
		if ((zp->z_flags & Z_FOUND) &&	/* already found? */
		    (zp - zones) != DB_Z_CACHE)	/* cache never sets Z_FOUND */
			syslog(LOG_NOTICE,
			       "Zone \"%s\" declared more than once",
			       zp->z_origin);
		zp->z_flags |= Z_FOUND;
		dprintf(1, (ddt, "zone[%d] type %d: '%s'",
			    zp-zones, type,
			    *(zp->z_origin) == '\0' ? "." : zp->z_origin));
		if (zp->z_refresh && zp->z_time == 0)
			ns_refreshtime(zp, tt.tv_sec);
		if (zp->z_time <= tt.tv_sec)
			needmaint = 1;
		dprintf(1, (ddt, " z_time %lu, z_refresh %lu\n",
			    (u_long)zp->z_time, (u_long)zp->z_refresh));
	}
	(void) my_fclose(fp);
	lineno = slineno;
}

static void
zoneinit(zp)
	register struct zoneinfo *zp;
{
	struct stat sb;
	int result;

	/*
	 * Try to load zone from backup file,
	 * if one was specified and it exists.
	 * If not, or if the data are out of date,
	 * we will refresh the zone from a primary
	 * immediately.
	 */
	if (!zp->z_source)
		return;
	result = stat(zp->z_source, &sb);
#ifdef PURGE_ZONE
	if (result != -1)
		purge_zone(zp->z_origin, hashtab, zp->z_class);
#endif
	if (result == -1 || db_load(zp->z_source, zp->z_origin, zp, NULL)) {
		/*
		 * Set zone to be refreshed immediately.
		 */
		zp->z_refresh = INIT_REFRESH;
		zp->z_retry = INIT_REFRESH;
		if (!(zp->z_flags & (Z_QSERIAL|Z_XFER_RUNNING))) {
			zp->z_time = tt.tv_sec;
			needmaint = 1;
		}
	} else {
		zp->z_flags |= Z_AUTH;
	}
}

#ifdef ALLOW_UPDATES
/*
 * Look for the authoritative zone with the longest matching RHS of dname
 * and return its zone # or zero if not found.
 */
int
findzone(dname, class)
	char *dname;
	int class;
{
	char *dZoneName, *zoneName;
	int dZoneNameLen, zoneNameLen;
	int maxMatchLen = 0;
	int maxMatchZoneNum = 0;
	int zoneNum;

	dprintf(4, (ddt, "findzone(dname=%s, class=%d)\n", dname, class));
#ifdef DEBUG
	if (debug >= 5) {
		fprintf(ddt, "zone dump:\n");
		for (zoneNum = 1; zoneNum < nzones; zoneNum++)
			printzoneinfo(zoneNum);
	}
#endif

	dZoneName = strchr(dname, '.');
	if (dZoneName == NULL)
		dZoneName = "";	/* root */
	else
		dZoneName++;	/* There is a '.' in dname, so use remainder of
				   string as the zone name */
	dZoneNameLen = strlen(dZoneName);
	for (zoneNum = 1; zoneNum < nzones; zoneNum++) {
		if (zones[zoneNum].z_type == Z_NIL)
			continue;
		zoneName = (zones[zoneNum]).z_origin;
		zoneNameLen = strlen(zoneName);
		/* The zone name may or may not end with a '.' */
		if (zoneName[zoneNameLen - 1] == '.')
			zoneNameLen--;
		if (dZoneNameLen != zoneNameLen)
			continue;
		dprintf(5, (ddt, "about to strncasecmp('%s', '%s', %d)\n",
			    dZoneName, zoneName, dZoneNameLen));
		if (strncasecmp(dZoneName, zoneName, dZoneNameLen) == 0) {
			dprintf(5, (ddt, "match\n"));
			/*
			 * See if this is as long a match as any so far.
			 * Check if "<=" instead of just "<" so that if
			 * root domain (whose name length is 0) matches,
			 * we use it's zone number instead of just 0
			 */
			if (maxMatchLen <= zoneNameLen) {
				maxMatchZoneNum = zoneNum;
				maxMatchLen = zoneNameLen;
			}
		} else {
			dprintf(5, (ddt, "no match\n"));
		}
	}
	dprintf(4, (ddt, "findzone: returning %d\n", maxMatchZoneNum));
	return (maxMatchZoneNum);
}
#endif /* ALLOW_UPDATES */

static void
get_forwarders(fp)
	FILE *fp;
{
	char buf[BUFSIZ];
	register struct fwdinfo *fip = NULL, *ftp = NULL;

#ifdef SLAVE_FORWARD
	int forward_count = 0;
#endif

	dprintf(1, (ddt, "forwarders "));

	/* on mulitple forwarder lines, move to end of the list */
#ifdef SLAVE_FORWARD
	if (fwdtab != NULL){
		forward_count++;
		for (fip = fwdtab; fip->next != NULL; fip = fip->next)
			forward_count++;
	}
#else
	if (fwdtab != NULL) {
		for (fip = fwdtab; fip->next != NULL; fip = fip->next) {
			;
		}
	}
#endif /* SLAVE_FORWARD */

	while (getword(buf, sizeof(buf), fp, 0)) {
		if (strlen(buf) == 0)
			break;
		dprintf(1, (ddt," %s",buf));
		if (!ftp) {
			ftp = (struct fwdinfo *)malloc(sizeof(struct fwdinfo));
			if (!ftp)
				panic(errno, "malloc(fwdinfo)");
		}
		if (inet_aton(buf, &ftp->fwdaddr.sin_addr)) {
			ftp->fwdaddr.sin_port = ns_port;
			ftp->fwdaddr.sin_family = AF_INET;
		} else {
			syslog(LOG_NOTICE, "'%s' (ignored, NOT dotted quad)",
			       buf);
			continue;
		}
#ifdef FWD_LOOP
		if (aIsUs(ftp->fwdaddr.sin_addr)) {
			syslog(LOG_NOTICE,
			       "Forwarder '%s' ignored, my address",
			       buf);
			dprintf(1, (ddt, " (ignored, my address)"));
			continue;
		}
#endif /* FWD_LOOP */
		ftp->next = NULL;
		if (fwdtab == NULL)
			fwdtab = ftp;	/* First time only */
		else
			fip->next = ftp;
		fip = ftp;
		ftp = NULL;
#ifdef SLAVE_FORWARD
		forward_count++;
#endif /* SLAVE_FORWARD */
	}
	if (ftp)
		free((char *)ftp);

#ifdef SLAVE_FORWARD
	/*
	** Set the slave retry time to 60 seconds total divided
	** between each forwarder
	*/
	if (forward_count != 0) {
		slave_retry = (int) (60 / forward_count);
		if(slave_retry <= 0)
			slave_retry = 1;
	}
#endif

	dprintf(1, (ddt, "\n"));
#ifdef DEBUG
	if (debug > 2) {
		for (ftp = fwdtab; ftp != NULL; ftp = ftp->next) {
			fprintf(ddt, "ftp x%lx [%s] next x%lx\n",
				(u_long)ftp,
				inet_ntoa(ftp->fwdaddr.sin_addr),
				(u_long)ftp->next);
		}
	}
#endif
}

static void
free_forwarders()
{
	register struct fwdinfo *ftp, *fnext;

	for (ftp = fwdtab; ftp != NULL; ftp = fnext) {
		fnext = ftp->next;
		free((char *)ftp);
	}
	fwdtab = NULL;
}

static struct zoneinfo *
find_zone(name, type, class)
	char *name;
	int type, class;
{
	register struct zoneinfo *zp;

        for (zp = &zones[1]; zp < &zones[nzones]; zp++) {
		if (zp->z_type == type && zp->z_class == class &&
		    strcasecmp(name, zp->z_origin) == 0) {
			dprintf(2, (ddt, ", old zone (%d)", zp - zones));
			return (zp);
		}
	}
	dprintf(2, (ddt, ", new zone"));
	return NULL;
}

#ifdef DEBUG
/* prints out the content of zones */
static void
content_zone(end)
	int end;
{
	int i;

	for (i = 1;  i <= end;  i++) {
		printzoneinfo(i);
	}
}
#endif

static void
ns_limit(name, value)
	const char *name;
	int value;
{
	if (!strcasecmp(name, "transfers-in")) {
		max_xfers_running = value;
	} else if (!strcasecmp(name, "transfers-per-ns")) {
		max_xfers_per_ns = value;
	} else if (!strcasecmp(name, "datasize")) {
		ns_rlimit("datasize", Datasize, value);
	} else {
		syslog(LOG_ERR,
		       "error: unrecognized limit in bootfile: \"%s\"",
		       name);
		exit(1);
	}
}

static void
ns_rlimit(name, limit, value)
	const char *name;
	enum limit limit;
	long value;
{
#ifndef HAVE_GETRUSAGE
# ifdef LINT
	name; limit; value;
# endif
	syslog(LOG_WARNING, "warning: unimplemented limit in bootfile: \"%s\"",
	       name);
#else
	struct rlimit limits;
	int rlimit;

	switch (limit) {
	case Datasize:
		rlimit = RLIMIT_DATA;
		break;
	default:
		abort();
	}
	if (getrlimit(rlimit, &limits) < 0) {
		syslog(LOG_WARNING, "getrlimit(%s): %m", name);
		return;
	}
	limits.rlim_cur = value;
	if (setrlimit(rlimit, &limits) < 0) {
		syslog(LOG_WARNING, "setrlimit(%s, %ld): %m", name, value);
		return;
	}
#endif
}

static void
ns_option(name)
	const char *name;
{
	if (!strcasecmp(name, "no-recursion")) {
		NoRecurse = 1;
	} else if (!strcasecmp(name, "no-fetch-glue")) {
		NoFetchGlue = 1;
#ifdef QRYLOG
	} else if (!strcasecmp(name, "query-log")) {
		qrylog = 1;
#endif
	} else if (!strcasecmp(name, "forward-only")) {
		forward_only = 1;
#ifndef INVQ
	} else if (!strcasecmp(name, "fake-iquery")) {
		fake_iquery = 1;
#endif
	} else {
		syslog(LOG_ERR,
		       "error: unrecognized option in bootfile: \"%s\"",
		       name);
		exit(1);
	}
}
