#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_init.c	4.38 (Berkeley) 3/21/91";
static char rcsid[] = "$Id: ns_init.c,v 8.25 1997/06/01 20:34:34 vixie Exp $";
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

#include <sys/types.h>
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

enum limit { Datasize , Files };

static void		zoneinit __P((struct zoneinfo *)),
			get_forwarders __P((FILE *)),
			boot_read __P((const char *filename, int includefile)),
#ifdef DEBUG
			content_zone __P((int)),
#endif
			do_reload __P((char *, int, int)),
			free_forwarders __P((void)),
			ns_limit __P((const char *name, int value)),
			ns_checknames __P((const char *names,
					   const char *severity)),
			ns_rlimit __P((const char *name, enum limit limit,
				       long value)),
			ns_option __P((const char *name));

static struct zoneinfo	*find_zone __P((char *, int, int));

static enum severity	checkname_severity[num_trans];

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

	memset(checkname_severity, '\0', sizeof checkname_severity);
	checkname_severity[primary_trans] = fail;
	checkname_severity[secondary_trans] = warn;
	checkname_severity[response_trans] = ignore;

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
		do_reload(zp->z_origin, zp->z_type, zp->z_class);
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
	char buf[MAXDNAME], obuf[MAXDNAME], *source;
	FILE *fp;
	int type;
	int class;
#ifdef GEN_AXFR
	char *class_p;
#endif
	struct stat f_time;
	static int tmpnum = 0;		/* unique number for tmp zone files */
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
		} else if (strcasecmp(buf, "check-names") == 0) {
			(void) getword(buf, sizeof(buf), fp, 0);
			(void) getword(obuf, sizeof(obuf), fp, 0);
			ns_checknames(buf, obuf);
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
    		        endline(fp);

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
			zp->z_refresh = 0;	/* no maintenance needed */
			zp->z_time = 0;
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
				dprintf(1, (ddt,
				        "backup file changed or missing\n"));
				free(zp->z_source);
				zp->z_source = NULL;
				zp->z_serial = 0;	/* force xfer */
				if (zp->z_flags & Z_AUTH) {
					zp->z_flags &= ~Z_AUTH;
#ifdef	CLEANCACHE
					remove_zone(hashtab, zp - zones, 1);
#else
					remove_zone(hashtab, zp - zones);
#endif
					/*
					 * reload parent so that NS records are
					 * present during the zone transfer.
					 */
					do_reload(zp->z_origin, zp->z_type,
						  zp->z_class);
				}
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

static void
get_forwarders(fp)
	FILE *fp;
{
	char buf[MAXDNAME];
	register struct fwdinfo *fip = NULL, *ftp = NULL;

#ifdef SLAVE_FORWARD
	int forward_count = 0;
#endif

	dprintf(1, (ddt, "forwarders "));

	/* On multiple forwarder lines, move to end of the list. */
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

static void
do_reload(domain, type, class)
	char *domain;
	int type;
	int class;
{
	char *s;
	struct zoneinfo *zp;

	dprintf(1, (ddt, "do_reload: %s %d %d\n", 
		    *domain ? domain : ".", type, class));

	/* the zone has changed type? */
	/* NOTE: we still exist so don't match agains ourselves */
	/* If we are a STUB or SECONDARY check that we have loaded */
	if (((type != Z_STUB) && (zp = find_zone(domain, Z_STUB, class)) &&
	     zp->z_serial) ||
	    ((type != Z_CACHE) && find_zone(domain, Z_CACHE, class)) ||
	    ((type != Z_PRIMARY) && find_zone(domain, Z_PRIMARY, class)) ||
	    ((type != Z_SECONDARY)
	     && (zp = find_zone(domain, Z_SECONDARY, class)) && zp->z_serial)
	    ) {
		return;
	}

	while ((s = strchr(domain, '.')) || *domain) {
		if (s)
			domain = s + 1;	/* skip dot */
		else
			domain = "";	/* root zone */

		if ((zp = find_zone(domain, Z_STUB, class)) ||
		    (zp = find_zone(domain, Z_CACHE, class)) ||
		    (zp = find_zone(domain, Z_PRIMARY, class)) ||
		    (zp = find_zone(domain, Z_SECONDARY, class))) {

			dprintf(1, (ddt, "do_reload: matched %s\n",
				    *domain ? domain : "."));

#ifdef CLEANCACHE
			if (zp->z_type == Z_CACHE)
				remove_zone(fcachetab, 0, 1);
			else
				remove_zone(hashtab, zp - zones, 1);
#else
			if (zp->z_type == Z_CACHE)
				remove_zone(fcachetab, 0);
			else
				remove_zone(hashtab, zp - zones);
#endif
			zp->z_flags &= ~Z_AUTH;

			switch (zp->z_type) {
			case Z_SECONDARY:
			case Z_STUB:
				zoneinit(zp);
				break;
			case Z_PRIMARY:
			case Z_CACHE:
				if (db_load(zp->z_source, zp->z_origin, zp, 0)
				    == 0)
					zp->z_flags |= Z_AUTH;
				break;
			}
			break;
		}
	}
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
	} else if (!strcasecmp(name, "files")) {
		ns_rlimit("files", Files, value);
	} else {
		syslog(LOG_ERR,
		       "error: unrecognized limit in bootfile: \"%s\"",
		       name);
		exit(1);
	}
}

static int
select_string(strings, string)
	const char *strings[];
	const char *string;
{
	int i;

	for (i = 0; strings[i] != NULL; i++)
		if (!strcasecmp(strings[i], string))
			return (i);
	return (-1);
}

static void
ns_checknames(transport_str, severity_str)
	const char *transport_str;
	const char *severity_str;
{
	enum transport transport;
	enum severity severity;
	int i;

	if ((i = select_string(transport_strings, transport_str)) == -1) {
		syslog(LOG_ERR,
		      "error: unrecognized transport type in bootfile: \"%s\"",
		       transport_str);
		exit(1);
	}
	transport = (enum transport) i;

	if ((i = select_string(severity_strings, severity_str)) == -1) {
		syslog(LOG_ERR,
		       "error: unrecognized severity type in bootfile: \"%s\"",
		       severity_str);
		exit(1);
	}
	severity = (enum severity) i;

	checkname_severity[transport] = severity;
	syslog(LOG_INFO, "check-names %s %s", transport_str, severity_str);
}

enum context
ns_ptrcontext(owner)
	const char *owner;
{
	if (samedomain(owner, "in-addr.arpa") || samedomain(owner, "ip6.int"))
		return (hostname_ctx);
	return (domain_ctx);
}

enum context
ns_ownercontext(type, transport)
	int type;
	enum transport transport;
{
	enum context context;

	switch (type) {
	case T_A:
	case T_WKS:
	case T_MX:
		switch (transport) {
		case primary_trans:
		case secondary_trans:
			context = owner_ctx;
			break;
		case response_trans:
			context = hostname_ctx;
			break;
		default:
			panic(-1, "impossible condition in ns_ownercontext()");
		}
		break;
	case T_MB:
	case T_MG:
		context = mailname_ctx;
	default:
		context = domain_ctx;
		break;
	}
	return (context);
}

int
ns_nameok(name, class, transport, context, owner, source)
	const char *name;
	int class;
	enum transport transport;
	enum context context;
	struct in_addr source;
	const char *owner;
{
	enum severity severity = checkname_severity[transport];
	int ok;

	if (severity == ignore)
		return (1);
	switch (context) {
	case domain_ctx:
		ok = (class != C_IN) || res_dnok(name);
		break;
	case owner_ctx:
		ok = (class != C_IN) || res_ownok(name);
		break;
	case mailname_ctx:
		ok = res_mailok(name);
		break;
	case hostname_ctx:
		ok = res_hnok(name);
		break;
	default:
		panic(-1, "impossible condition in ns_nameok()");
	}
	if (!ok) {
		char *s, *o;

		if (source.s_addr == INADDR_ANY)
			s = strdup(transport_strings[transport]);
		else {
			s = malloc(strlen(transport_strings[transport]) +
				   sizeof " from [000.000.000.000]");
			if (s)
				sprintf(s, "%s from [%s]",
					transport_strings[transport],
					inet_ntoa(source));
		}
		if (strcasecmp(owner, name) == 0)
			o = strdup("");
		else {
			const char *t = (*owner == '\0') ? "." : owner;

			o = malloc(strlen(t) + sizeof " (owner \"\")");
			if (o)
				sprintf(o, " (owner \"%s\")", t);
		}
#ifndef ultrix
		syslog((transport == response_trans) ? LOG_INFO : LOG_NOTICE,
		       "%s name \"%s\"%s %s (%s) is invalid - %s",
		       context_strings[context],
		       name, o != NULL ? o : "[malloc failed]", p_class(class),
		       s != NULL ? s : "[malloc failed]",
		       (severity == fail) ? "rejecting" : "proceeding anyway");
#endif
		if (severity == warn)
			ok = 1;
		if (s)
			free(s);
		if (o)
			free(o);
	}
	return (ok);
}

int
ns_wildcard(name)
	const char *name;
{
	if (*name != '*')
		return (0);
	return (*++name == '\0');
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
	int rlimit = -1;

	switch (limit) {
	case Datasize:
		rlimit = RLIMIT_DATA;
		break;
	case Files:
#ifdef RLIMIT_NOFILE
		rlimit = RLIMIT_NOFILE;
#endif
		break;
	default:
		panic(-1, "impossible condition in ns_rlimit()");
	}
	if (rlimit == -1) {
		syslog(LOG_WARNING,
		       "limit \"%s\" not supported on this system - ignored",
		       name);
		return;
	}
	if (getrlimit(rlimit, &limits) < 0) {
		syslog(LOG_WARNING, "getrlimit(%s): %m", name);
		return;
	}
	limits.rlim_cur = limits.rlim_max = value;
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
