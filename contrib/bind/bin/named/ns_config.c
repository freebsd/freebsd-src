#if !defined(lint) && !defined(SABER)
static char rcsid[] = "$Id: ns_config.c,v 8.35 1998/05/05 19:44:48 halley Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996, 1997 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include "port_after.h"

#ifdef HAVE_GETRUSAGE		/* XXX */
#include <sys/resource.h>
#endif

#include "named.h"
#include "ns_parseutil.h"

static int tmpnum = 0;
static int config_initialized = 0;

static int need_logging_free = 0;
static int default_logging_installed;

static int options_installed = 0;
static int logging_installed = 0;
static int default_options_installed;
static int initial_configuration = 1;

static char **logging_categories;
static char *current_pid_filename = NULL;

#define ZONE_SYM_TABLE_SIZE 4973
static symbol_table zone_symbol_table;

/* Zones */

void
free_zone_timerinfo(struct zoneinfo *zp) {
	if (zp->z_timerinfo != NULL) {
		freestr(zp->z_timerinfo->name);
		memput(zp->z_timerinfo, sizeof *zp->z_timerinfo);
		zp->z_timerinfo = NULL;
	} else
		ns_error(ns_log_config, "timer for zone '%s' had no timerinfo",
			 zp->z_origin);
}

void
free_zone_contents(struct zoneinfo *zp, int undefine_sym) {
	INSIST(zp != NULL);

	if (undefine_sym)
		undefine_symbol(zone_symbol_table, zp->z_origin,
				(zp->z_type << 16) | zp->z_class);
	if (zp->z_flags & Z_TIMER_SET) {
		free_zone_timerinfo(zp);
		if (evClearTimer(ev, zp->z_timer) < 0)
			ns_error(ns_log_config,
			 "evClearTimer for zone '%s' failed in ns_init: %s",
				 zp->z_origin,
				 strerror(errno));
	}
	if (zp->z_origin != NULL)
		freestr(zp->z_origin);
	if (zp->z_source != NULL)
		freestr(zp->z_source);
	if (zp->z_update_acl != NULL)
		free_ip_match_list(zp->z_update_acl);
	if (zp->z_query_acl != NULL)
		free_ip_match_list(zp->z_query_acl);
	if (zp->z_transfer_acl != NULL)
		free_ip_match_list(zp->z_transfer_acl);
#ifdef BIND_UPDATE
	if (zp->z_updatelog != NULL)
		freestr(zp->z_updatelog);
#endif /* BIND_UPDATE */
}

static void
free_zone(struct zoneinfo *zp) {
	INSIST(zp != NULL);

	free_zone_contents(zp, 0);
	memput(zp, sizeof *zp);
}

struct zoneinfo *
find_zone(const char *name, int type, int class) {
	struct zoneinfo *zp;
	symbol_value value;

	ns_debug(ns_log_config, 3, "find_zone(%s,%d,%d)", name, type, class);
	if (lookup_symbol(zone_symbol_table, name,
			  (type<<16) | class, &value)) {
		INSIST(value.integer >= 0 && value.integer < nzones);
		ns_debug(ns_log_config, 3,
			 "find_zone: existing zone %d", value.integer);
		zp = &zones[value.integer];
		return (zp);
	}
	ns_debug(ns_log_config, 3, "find_zone: unknown zone");
	return (NULL);
}

static struct zoneinfo *
new_zone(int class, int type) {
	struct zoneinfo *zp;

	if (zones != NULL) {
		if (type == z_hint) {
			zp = &zones[0];
			return (zp);
		}

		for (zp = &zones[1]; zp < &zones[nzones]; zp++)
			if (zp->z_type == z_nil)
				return (zp);
	}

	/*
	 * This code assumes that nzones never decreases.
	 */
	if (nzones % 64 == 0) {
		ns_debug(ns_log_config, 1, "Reallocating zones structure");
		zp = (struct zoneinfo *)
			memget((64 + nzones) * sizeof(struct zoneinfo));
		if (zp == NULL)
			panic("no memory for more zones", NULL);
		memcpy(zp, zones, nzones * sizeof(struct zoneinfo));
		memset(&zp[nzones], 0, 64 * sizeof(struct zoneinfo));
		memput(zones, nzones * sizeof(struct zoneinfo));
		zones = zp;
	}
	zp = &zones[nzones++];

	return (zp);
}

/*
 * Check out a zoneinfo structure and return non-zero if it's OK.
 */
static int
validate_zone(struct zoneinfo *zp) {
	int warnings = 0;
	char filename[MAXPATHLEN+1];

	/* Check name */
	if (!res_dnok(zp->z_origin)) {
		ns_error(ns_log_config, "invalid zone name '%s'",
			 zp->z_origin);
		return (0);
	}

	/* Check class */
	if (zp->z_class == C_ANY || zp->z_class == C_NONE) {
		ns_error(ns_log_config, "invalid class %d for zone '%s'",
			 zp->z_class, zp->z_origin);
		return (0);
	}

	/* Check type. */
	if (zp->z_type == 0) {
		ns_error(ns_log_config, "no type specified for zone '%s'",
			 zp->z_origin);
		return (0);
	}
	if (zp->z_type == z_hint && strcasecmp(zp->z_origin, "") != 0) {
		ns_error(ns_log_config,
			 "only the root zone may be a hint zone (zone '%s')",
			 zp->z_origin);
		return (0);
	}

	/* Check filename. */
	if (zp->z_type == z_master && zp->z_source == NULL) {
		ns_error(ns_log_config,
			 "'file' statement missing for master zone %s",
			 zp->z_origin);
		return (0);
	}
	/*
	 * XXX  We should run filename through an OS-specific
	 *      validator here.
	 */
	if (zp->z_source != NULL && 
	    strlen(zp->z_source) > MAXPATHLEN) {
		ns_error(ns_log_config, "filename too long for zone '%s'",
			 zp->z_origin);
		return (0);
	}

	/* Check masters */
	if (zp->z_addrcnt != 0) {
		if (zp->z_type != z_slave && zp->z_type != z_stub) {
			ns_error(ns_log_config,
				"'masters' statement present for %s zone '%s'",
				 (zp->z_type == z_master) ? "master" : "hint",
				 zp->z_origin);
			return (0);
		}
	} else {
		if (zp->z_type == z_slave || zp->z_type == z_stub) {
			ns_error(ns_log_config,
			  "no 'masters' statement for non-master zone '%s'",
				 zp->z_origin);
			return (0);
		}
	}

	/* Check also-notify */
	if (zp->z_notify_count != 0) {
		if (zp->z_type != z_master && zp->z_type != z_slave) {
			ns_error(ns_log_config,
		 "'also-notify' given for non-master, non-slave zone '%s'",
				 zp->z_origin);
			return (0);
		}
	}

#ifdef BIND_UPDATE
	/* XXX need more checking here */
	if (zp->z_type == z_master) {
		if (!zp->z_soaincrintvl)
			zp->z_soaincrintvl = SOAINCRINTVL;
		if (!zp->z_dumpintvl)
			zp->z_dumpintvl = DUMPINTVL;
		if (!zp->z_deferupdcnt)
			zp->z_deferupdcnt = DEFERUPDCNT;
		if (!zp->z_updatelog) {
			/* XXX OS-specific filename validation here */
			if ((strlen(zp->z_source) + (sizeof ".log" - 1)) >
			    MAXPATHLEN) {
				ns_error(ns_log_config,
				  "filename too long for dynamic zone '%s'",
					 zp->z_origin);
				return (0);
			}
			/* this sprintf() is now safe */
			sprintf(filename, "%s.log", zp->z_source);
			zp->z_updatelog = savestr(filename, 1);
		}
	}
#endif /* BIND_UPDATE */

	return (1);
}

/*
 * Start building a new zoneinfo structure.  Returns an opaque
 * zone_config suitable for use by the parser.
 */
zone_config
begin_zone(char *name, int class) {
	zone_config zh;
	struct zoneinfo *zp;

	/*
	 * require: name is canonical, class is a valid class
	 */

	ns_debug(ns_log_config, 3, "begin_zone('%s', %d)",
		 (*name == '\0') ? "." : name, class);

	zp = (struct zoneinfo *)memget(sizeof (struct zoneinfo));
	if (zp == NULL)
		panic("memget failed in begin_zone", NULL);
	memset(zp, 0, sizeof (struct zoneinfo));
	zp->z_origin = name;
	zp->z_class = class;
	zp->z_checknames = not_set;
	zh.opaque = zp;
	return (zh);
}

/*
 * Merge new configuration information into an existing zone.  The
 * new zoneinfo must be valid.
 */
static void
update_zone_info(struct zoneinfo *zp, struct zoneinfo *new_zp) {
	struct stat f_time;
	char buf[MAXPATHLEN+1];
	int i;

	INSIST(zp != NULL);
	INSIST(new_zp != NULL);

	ns_debug(ns_log_config, 1, "update_zone_info('%s', %d)",
		 (*new_zp->z_origin == '\0') ? "." : new_zp->z_origin,
		 new_zp->z_type);

#ifdef BIND_UPDATE
	/*
	 * A dynamic master zone that's becoming non-dynamic may need to be
	 * dumped before we start the update.
	 */
	if ((zp->z_flags & Z_DYNAMIC) && !(new_zp->z_flags & Z_DYNAMIC) &&
	    ((zp->z_flags & Z_NEED_SOAUPDATE) ||
	     (zp->z_flags & Z_NEED_DUMP)))
		(void)zonedump(zp);
#endif

	/*
	 * First do the simple stuff, making sure to free
	 * any data that was dynamically allocated.
	 */
	if (zp->z_origin != NULL)
		freestr(zp->z_origin);
	zp->z_origin = new_zp->z_origin;
	new_zp->z_origin = NULL;
	zp->z_class = new_zp->z_class;
	zp->z_type = new_zp->z_type;
	zp->z_checknames = new_zp->z_checknames;
	for (i = 0; i < new_zp->z_addrcnt; i++)
		zp->z_addr[i] = new_zp->z_addr[i];
	zp->z_addrcnt = new_zp->z_addrcnt;
	if (zp->z_update_acl)
		free_ip_match_list(zp->z_update_acl);
	zp->z_update_acl = new_zp->z_update_acl;
	new_zp->z_update_acl = NULL;
	if (zp->z_query_acl)
		free_ip_match_list(zp->z_query_acl);
	zp->z_query_acl = new_zp->z_query_acl;
	new_zp->z_query_acl = NULL;
	zp->z_axfr_src = new_zp->z_axfr_src;
	if (zp->z_transfer_acl)
		free_ip_match_list(zp->z_transfer_acl);
	zp->z_transfer_acl = new_zp->z_transfer_acl;
	new_zp->z_transfer_acl = NULL;
	zp->z_max_transfer_time_in = new_zp->z_max_transfer_time_in;

	zp->z_notify = new_zp->z_notify;
	for (i = 0; i < new_zp->z_notify_count; i++)
		zp->z_also_notify[i] = new_zp->z_also_notify[i];
	zp->z_notify_count = new_zp->z_notify_count;

#ifdef BIND_UPDATE
	if (new_zp->z_flags & Z_DYNAMIC)
		zp->z_flags |= Z_DYNAMIC;
	else
		zp->z_flags &= ~Z_DYNAMIC;
	zp->z_soaincrintvl = new_zp->z_soaincrintvl;
	zp->z_dumpintvl = new_zp->z_dumpintvl;
	zp->z_deferupdcnt = new_zp->z_deferupdcnt;
	if (zp->z_updatelog)
		freestr(zp->z_updatelog);
	zp->z_updatelog = new_zp->z_updatelog;
	new_zp->z_updatelog = NULL;
#endif /* BIND_UPDATE */

	/*
	 * now deal with files
	 */
	switch (zp->z_type) {
	case z_hint:
		ns_debug(ns_log_config, 1, "source = %s", new_zp->z_source);
		zp->z_refresh = 0;	/* No dumping. */
		/*
		 * If we've loaded this file, and the file has
		 * not been modified and contains no $include,
		 * then there's no need to reload.
		 */
		if (zp->z_source &&
		    !strcmp(new_zp->z_source, zp->z_source) &&
		    !(zp->z_flags & Z_INCLUDE) && 
		    stat(zp->z_source, &f_time) != -1 &&
		    zp->z_ftime == f_time.st_mtime) {
			ns_debug(ns_log_config, 1, "cache is up to date");
			break;
		}

		/* File has changed, or hasn't been loaded yet. */
		if (zp->z_source) {
			freestr(zp->z_source);
			clean_cache(fcachetab, 1);
		}
		zp->z_source = new_zp->z_source;
		new_zp->z_source = NULL;
		ns_debug(ns_log_config, 1, "reloading zone");
		(void) db_load(zp->z_source, zp->z_origin, zp, NULL);
		break;

	case z_master:
		ns_debug(ns_log_config, 1, "source = %s", new_zp->z_source);
		/*
		 * If we've loaded this file, and the file has
		 * not been modified and contains no $include,
		 * then there's no need to reload.
		 */
		if (zp->z_source &&
		    !strcmp(new_zp->z_source, zp->z_source) &&
		    !(zp->z_flags & Z_INCLUDE) && 
		    stat(zp->z_source, &f_time) != -1 &&
		    zp->z_ftime == f_time.st_mtime) {
			ns_debug(ns_log_config, 1, "zone is up to date");
			break; /* zone is already up to date */
		}
#ifdef BIND_UPDATE
		if (zp->z_source && (zp->z_flags & Z_DYNAMIC))
			ns_warning(ns_log_config,
			  "source file of dynamic zone '%s' has changed",
				   zp->z_origin);

	primary_reload:
#endif /* BIND_UPDATE */
		if (zp->z_source != NULL)
			freestr(zp->z_source);
		zp->z_source = new_zp->z_source;
		new_zp->z_source = NULL;
		zp->z_flags &= ~Z_AUTH;
		ns_stopxfrs(zp);
		purge_zone(zp->z_origin, hashtab, zp->z_class);
		ns_debug(ns_log_config, 1, "reloading zone");
#ifdef BIND_UPDATE
		if (zp->z_flags & Z_DYNAMIC) {
			struct stat sb;

			if (stat(zp->z_source, &sb) < 0)
				ns_error(ns_log_config, "stat(%s) failed: %s",
					 zp->z_source, strerror(errno));
			else {
				if (sb.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH))
					ns_warning(ns_log_config,
					 "dynamic zone file '%s' is writable",
						   zp->z_source);
			}
		}
#endif
		if (!db_load(zp->z_source, zp->z_origin, zp, NULL))
			zp->z_flags |= Z_AUTH;
		zp->z_refresh = 0;	/* no maintenance needed */
		zp->z_time = 0;
#ifdef BIND_UPDATE
		zp->z_lastupdate = 0;
		if (zp->z_flags & Z_DYNAMIC)
			/*
			 * Note that going to primary_reload
			 * unconditionally reloads the zone.
			 */
			if (merge_logs(zp) == 1) {
				new_zp->z_source = savestr(zp->z_source, 1);
				goto primary_reload;
			}
#endif
		break;

	case z_slave:
#ifdef STUBS
	case z_stub:
#endif
		ns_debug(ns_log_config, 1, "addrcnt = %d", zp->z_addrcnt);
		if (!new_zp->z_source) {
			/*
			 * We will always transfer this zone again
			 * after a reload.
			 */
			sprintf(buf, "NsTmp%ld.%d", (long)getpid(), tmpnum++);
			new_zp->z_source = savestr(buf, 1);
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
		    (strcmp(new_zp->z_source, zp->z_source) ||
		     (stat(zp->z_source, &f_time) == -1 ||
		      (zp->z_ftime != f_time.st_mtime)))) {
			ns_debug(ns_log_config, 1,
				 "backup file changed or missing");
			freestr(zp->z_source);
			zp->z_source = NULL;
			zp->z_serial = 0;	/* force xfer */
			ns_stopxfrs(zp);
			/*
			 * We only need to do_reload if we have
			 * successfully transferred the zone.
			 */
			if (zp->z_flags & Z_AUTH) {
				zp->z_flags &= ~Z_AUTH;
				/*
				 * Purge old data and reload parent so that
				 * NS records are present during the zone
				 * transfer.
				 */
				do_reload(zp->z_origin, zp->z_type,
					  zp->z_class);
			}
		}
		if (zp->z_source == NULL) {
			zp->z_source = new_zp->z_source;
			new_zp->z_source = NULL;
		}
		if (!(zp->z_flags & Z_AUTH))
			zoneinit(zp);
#ifdef FORCED_RELOAD
		else {
			/* 
			** Force secondary to try transfer soon
			** after SIGHUP.
			*/
			if (!(zp->z_flags & (Z_QSERIAL|Z_XFER_RUNNING))
			    && reloading) {
				qserial_retrytime(zp, tt.tv_sec);
				sched_zone_maint(zp);
			}
		}
#endif /* FORCED_RELOAD */
		break;
	}
	if ((zp->z_flags & Z_FOUND) &&	/* already found? */
	    (zp - zones) != DB_Z_CACHE)	/* cache never sets Z_FOUND */
		ns_error(ns_log_config, "Zone \"%s\" declared more than once",
			 zp->z_origin);
	zp->z_flags |= Z_FOUND;
	ns_debug(ns_log_config, 1,
		 "zone[%d] type %d: '%s' z_time %lu, z_refresh %u",
		 zp-zones, zp->z_type,
		 *(zp->z_origin) == '\0' ? "." : zp->z_origin,
		 (u_long)zp->z_time, zp->z_refresh);
}

/*
 * Finish constructing a new zone.  If valid, the constructed zone is
 * merged into the zone database.  The zone_config used is invalid after
 * end_zone() completes.
 */
void
end_zone(zone_config zh, int should_install) {
	struct zoneinfo *zp, *new_zp;
	char *zname;
	symbol_value value;

	new_zp = zh.opaque;
	INSIST(new_zp != NULL);

	zname = (new_zp->z_origin[0] == '\0') ? "." : new_zp->z_origin;
	ns_debug(ns_log_config, 3, "end_zone('%s', %d)", zname,
		 should_install);

	if (!should_install) {
		free_zone(new_zp);
		return;
	}
	if (!validate_zone(new_zp)) {
		ns_error(ns_log_config,
			 "zone '%s' did not validate, skipping", zname);
		free_zone(new_zp);
		return;
	}
	zp = find_zone(new_zp->z_origin, new_zp->z_type, new_zp->z_class);
	if (zp == NULL) {
		zp = new_zone(new_zp->z_class, new_zp->z_type);
		INSIST(zp != NULL);
		value.integer = (zp - zones);
		define_symbol(zone_symbol_table, savestr(new_zp->z_origin, 1),
			      (new_zp->z_type << 16) | new_zp->z_class,
			      value, SYMBOL_FREE_KEY);
	}
	ns_debug(ns_log_config, 5, "zone '%s', type = %d, class = %d", zname,
		 new_zp->z_type, new_zp->z_class);
	if (new_zp->z_source != NULL)
		ns_debug(ns_log_config, 5, "  file = %s", new_zp->z_source);
	ns_debug(ns_log_config, 5, "  checknames = %d", new_zp->z_checknames);
	if (new_zp->z_addrcnt) {
		int i;

		ns_debug(ns_log_config, 5, "  masters:");
		for (i=0; i<new_zp->z_addrcnt; i++)
			ns_debug(ns_log_config, 5, "    %s",
				 inet_ntoa(new_zp->z_addr[i]));
	}

	update_zone_info(zp, new_zp);
	free_zone(new_zp);
	zh.opaque = NULL;
}

int
set_zone_type(zone_config zh, int type) {
	struct zoneinfo *zp;

	zp = zh.opaque;
	INSIST(zp != NULL);

	/* Fail if type already set for this zone */
	if (zp->z_type != 0)
		return (0);
	zp->z_type = type;
	return (1);
}

int
set_zone_filename(zone_config zh, char *filename) {
	struct zoneinfo *zp;

	zp = zh.opaque;
	INSIST(zp != NULL);

	/* Fail if filename already set for this zone */
	if (zp->z_source != NULL)
		return (0);
	zp->z_source = filename;
	return (1);
}

int
set_zone_checknames(zone_config zh, enum severity s) {
	struct zoneinfo *zp;

	zp = zh.opaque;
	INSIST(zp != NULL);

	/* Fail if checknames already set for this zone */
	if (zp->z_checknames != not_set)
		return (0);
	zp->z_checknames = s;
	return (1);
}

int
set_zone_notify(zone_config zh, int value) {
	struct zoneinfo *zp;

	zp = zh.opaque;
	INSIST(zp != NULL);

	if (value)
		zp->z_notify = znotify_yes;
	else
		zp->z_notify = znotify_no;

	return (1);
}

int
set_zone_update_acl(zone_config zh, ip_match_list iml) {
	struct zoneinfo *zp;

	zp = zh.opaque;
	INSIST(zp != NULL);

	/* Fail if checknames already set for this zone */
	if (zp->z_update_acl != NULL)
		return (0);
	zp->z_update_acl = iml;
#ifdef BIND_UPDATE
	if (!ip_match_is_none(iml))
		zp->z_flags |= Z_DYNAMIC;
	else
		ns_debug(ns_log_config, 3, "update acl is none for '%s'",
			 zp->z_origin);
#endif
	return (1);
}

int
set_zone_query_acl(zone_config zh, ip_match_list iml) {
	struct zoneinfo *zp;

	zp = zh.opaque;
	INSIST(zp != NULL);

	/* Fail if checknames already set for this zone */
	if (zp->z_query_acl != NULL)
		return (0);
	zp->z_query_acl = iml;
	return (1);
}

int
set_zone_transfer_source(zone_config zh, struct in_addr ina) {
	struct zoneinfo *zp = zh.opaque;

	zp->z_axfr_src = ina;
	return (1);
}

int
set_zone_transfer_acl(zone_config zh, ip_match_list iml) {
	struct zoneinfo *zp;

	zp = zh.opaque;
	INSIST(zp != NULL);

	/* Fail if checknames already set for this zone */
	if (zp->z_transfer_acl != NULL)
		return (0);
	zp->z_transfer_acl = iml;
	return (1);
}

int
set_zone_transfer_time_in(zone_config zh, long max_time) {
	struct zoneinfo *zp;

	zp = zh.opaque;
	INSIST(zp != NULL);

	/* Fail if checknames already set for this zone */
	if (zp->z_max_transfer_time_in)
		return (0);
	zp->z_max_transfer_time_in = max_time;
	return (1);
}

int
add_zone_master(zone_config zh, struct in_addr address) {
	struct zoneinfo *zp;

	zp = zh.opaque;
	INSIST(zp != NULL);

	zp->z_addr[zp->z_addrcnt] = address;
	zp->z_addrcnt++;
	if (zp->z_addrcnt >= NSMAX) {
		ns_warning(ns_log_config, "NSMAX reached for zone '%s'",
			   zp->z_origin);
		zp->z_addrcnt = NSMAX - 1;
	}
	return (1);
}

int
add_zone_notify(zone_config zh, struct in_addr address) {
	struct zoneinfo *zp;

	zp = zh.opaque;
	INSIST(zp != NULL);

	zp->z_also_notify[zp->z_notify_count] = address;
	zp->z_notify_count++;
	if (zp->z_notify_count >= NSMAX) {
		ns_warning(ns_log_config, "also-notify set full for zone '%s'",
			   zp->z_origin);
		zp->z_notify_count = NSMAX - 1;
	}
	return (1);
}

/* Options */

options
new_options() {
	options op;
	ip_match_list iml;
	ip_match_element ime;

	op = (options)memget(sizeof (struct options));
	if (op == NULL)
		panic("memget failed in new_options()", NULL);

	op->directory = savestr(".", 1);
	op->pid_filename = savestr(_PATH_PIDFILE, 1);
	op->named_xfer = savestr(_PATH_XFER, 1);
	op->dump_filename = savestr(_PATH_DUMPFILE, 1);
	op->stats_filename = savestr(_PATH_STATS, 1);
	op->memstats_filename = savestr(_PATH_MEMSTATS, 1);
	op->flags = DEFAULT_OPTION_FLAGS;
	op->transfers_in = DEFAULT_XFERS_RUNNING;
	op->transfers_per_ns = DEFAULT_XFERS_PER_NS;
	op->transfers_out = 0;
	op->transfer_format = axfr_one_answer;
	op->max_transfer_time_in = MAX_XFER_TIME;
	memset(&op->query_source, 0, sizeof op->query_source);
	op->query_source.sin_family = AF_INET;
	op->query_source.sin_addr.s_addr = htonl(INADDR_ANY);
	op->query_source.sin_port = htons(0);		/* INPORT_ANY */
	op->query_acl = NULL;
	op->transfer_acl = NULL;
	/* default topology is { localhost; localnets; } */
	iml = new_ip_match_list();
	ime = new_ip_match_localhost();
	add_to_ip_match_list(iml, ime);
	ime = new_ip_match_localnets();
	add_to_ip_match_list(iml, ime);
	op->topology = iml;
	op->data_size = 0UL;	/* use system default */
	op->stack_size = 0UL;	/* use system default */
	op->core_size = 0UL;	/* use system default */
	op->files = ULONG_MAX;  /* unlimited */
	op->check_names[primary_trans] = fail;
	op->check_names[secondary_trans] = warn;
	op->check_names[response_trans] = ignore;
	op->listen_list = NULL;
	op->fwdtab = NULL;
	/* XXX init forwarding */
	op->clean_interval = 3600;
	op->interface_interval = 3600;
	op->stats_interval = 3600;
	return (op);
}

void
free_options(options op) {
	INSIST(op != NULL);

	if (op->directory)
		freestr(op->directory);
	if (op->pid_filename)
		freestr(op->pid_filename);
	if (op->named_xfer)
		freestr(op->named_xfer);
	if (op->dump_filename)
		freestr(op->dump_filename);
	if (op->stats_filename)
		freestr(op->stats_filename);
	if (op->memstats_filename)
		freestr(op->memstats_filename);
	if (op->query_acl)
		free_ip_match_list(op->query_acl);
	if (op->transfer_acl)
		free_ip_match_list(op->transfer_acl);
	if (op->topology)
		free_ip_match_list(op->topology);
	if (op->listen_list)
		free_listen_info_list(op->listen_list);
	if (op->fwdtab)
		free_forwarders(op->fwdtab);
	memput(op, sizeof *op);
}

void
set_boolean_option(options op, int bool_opt, int value) {
	INSIST(op != NULL);

	switch (bool_opt) {
	case OPTION_NORECURSE:
	case OPTION_NOFETCHGLUE:
	case OPTION_FORWARD_ONLY:
	case OPTION_FAKE_IQUERY:
	case OPTION_NONOTIFY:
	case OPTION_NONAUTH_NXDOMAIN:
	case OPTION_MULTIPLE_CNAMES:
	case OPTION_HOSTSTATS:
	case OPTION_DEALLOC_ON_EXIT:
		if (value)
			op->flags |= bool_opt;
		else
			op->flags &= ~bool_opt;
		break;
	default:
		panic("unexpected option in set_boolean_option", NULL);
	}
}

#ifdef HAVE_GETRUSAGE
enum limit { Datasize, Stacksize, Coresize, Files };

static struct rlimit initial_data_size;
static struct rlimit initial_stack_size;
static struct rlimit initial_core_size;
static struct rlimit initial_num_files;

static void
get_initial_limits() {
# ifdef RLIMIT_DATA
	if (getrlimit(RLIMIT_DATA, &initial_data_size) < 0)
		ns_warning(ns_log_config, "getrlimit(DATA): %s",
			   strerror(errno));
# endif
# ifdef RLIMIT_STACK
	if (getrlimit(RLIMIT_STACK, &initial_stack_size) < 0)
		ns_warning(ns_log_config, "getrlimit(STACK): %s",
			   strerror(errno));
# endif
# ifdef RLIMIT_CORE
	if (getrlimit(RLIMIT_CORE, &initial_core_size) < 0)
		ns_warning(ns_log_config, "getrlimit(CORE): %s",
			   strerror(errno));
# endif
# ifdef RLIMIT_NOFILE
	if (getrlimit(RLIMIT_NOFILE, &initial_num_files) < 0)
		ns_warning(ns_log_config, "getrlimit(NOFILE): %s",
			   strerror(errno));
# endif
}

static void
ns_rlimit(enum limit limit, u_long limit_value) {
	struct rlimit limits, old_limits;
	int rlimit = -1;
	char *name;
	rlimit_type value;

	if (limit_value == ULONG_MAX) {
#ifndef RLIMIT_FILE_INFINITY
		if (limit == Files)
			value = MAX((rlimit_type)sysconf(_SC_OPEN_MAX),
				    initial_num_files.rlim_max);
		else
#endif
			value = (rlimit_type)RLIM_INFINITY;
	} else
		value = (rlimit_type)limit_value;

	limits.rlim_cur = limits.rlim_max = value;
	switch (limit) {
	case Datasize:
#ifdef RLIMIT_DATA
		rlimit = RLIMIT_DATA;
#endif
		name = "max data size";
		if (value == 0)
			limits = initial_data_size;
		break;
	case Stacksize:
#ifdef RLIMIT_STACK
		rlimit = RLIMIT_STACK;
#endif
		name = "max stack size";
		if (value == 0)
			limits = initial_stack_size;
		break;
	case Coresize:
#ifdef RLIMIT_CORE
		rlimit = RLIMIT_CORE;
#endif
		name = "max core size";
		if (value == 0)
			limits = initial_core_size;
		break;
	case Files:
#ifdef RLIMIT_NOFILE
		rlimit = RLIMIT_NOFILE;
#endif
		name = "max number of open files";
		if (value == 0)
			limits = initial_num_files;
		/* XXX check < FD_SETSIZE? */
		break;
	default:
		name = NULL;	/* Make gcc happy. */
		panic("impossible condition in ns_rlimit()", NULL);
	}
	if (rlimit == -1) {
		ns_warning(ns_log_config,
		  "limit \"%s\" not supported on this system - ignored",
			   name);
		return;
	}
	if (getrlimit(rlimit, &old_limits) < 0) {
		ns_warning(ns_log_config, "getrlimit(%s): %s", name,
			   strerror(errno));
	}
	if (user_id != 0 && limits.rlim_max == RLIM_INFINITY)
		limits.rlim_cur = limits.rlim_max = old_limits.rlim_max;
	if (setrlimit(rlimit, &limits) < 0) {
		ns_warning(ns_log_config, "setrlimit(%s): %s", name,
			   strerror(errno));
		return;
	} else {
		if (value == 0)
			ns_debug(ns_log_config, 3, "%s is default",
				 name);
		else if (value == RLIM_INFINITY)
			ns_debug(ns_log_config, 3, "%s is unlimited", name);
		else
#ifdef RLIMIT_LONGLONG
			ns_debug(ns_log_config, 3, "%s is %llu", name,
				 (unsigned long long)value);
#else
			ns_debug(ns_log_config, 3, "%s is %lu", name, value);
#endif
	}
}
#endif /* HAVE_GETRUSAGE */

listen_info_list
new_listen_info_list() {
	listen_info_list ll;

	ll = (listen_info_list)memget(sizeof (struct listen_info_list));
	if (ll == NULL)
		panic("memget failed in new_listen_info_list()", NULL);
	ll->first = NULL;
	ll->last = NULL;
	return (ll);
}

void
free_listen_info_list(listen_info_list ll) {
	listen_info li, next_li;

	INSIST(ll != NULL);
	for (li = ll->first; li != NULL; li = next_li) {
		next_li = li->next;
		free_ip_match_list(li->list);
		memput(li, sizeof *li);
	}
	memput(ll, sizeof *ll);
}

void
add_listen_on(options op, u_short port, ip_match_list iml) {
	listen_info_list ll;
	listen_info ni;

	INSIST(op != NULL);

	if (op->listen_list == NULL)
		op->listen_list = new_listen_info_list();
	ll = op->listen_list;
	ni = (listen_info)memget(sizeof (struct listen_info));
	if (ni == NULL)
		panic("memget failed in add_listen_on", NULL);
	ni->port = port;
	ni->list = iml;
	ni->next = NULL;
	if (ll->last != NULL)
		ll->last->next = ni;
	ll->last = ni;
	if (ll->first == NULL)
		ll->first = ni;
}

FILE *
write_open(char *filename) {
	FILE *stream;
	int fd;
	struct stat sb;
	int regular;

	if (stat(filename, &sb) < 0) {
		if (errno != ENOENT) {
			ns_error(ns_log_os,
				 "write_open: stat of %s failed: %s",
				 filename, strerror(errno));
			return (NULL);
		}
		regular = 1;
	} else
		regular = (sb.st_mode & S_IFREG);

	if (!regular) {
		ns_error(ns_log_os, "write_open: %s isn't a regular file",
			 filename);
		return (NULL);
	}
		
	(void)unlink(filename);
	fd = open(filename, O_WRONLY|O_CREAT|O_EXCL,
		  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if (fd < 0)
		return (NULL);
	stream = fdopen(fd, "w");
	if (stream == NULL)
		(void)close(fd);
	return (stream);
}

void
update_pid_file() {
	FILE *fp;

	REQUIRE(server_options != NULL);
	REQUIRE(server_options->pid_filename != NULL);

	/* XXX */ ns_debug(ns_log_default, 1, "update_pid_file()");
	if (current_pid_filename != NULL) {
		(void)unlink(current_pid_filename);
		freestr(current_pid_filename);
		current_pid_filename = NULL;
	}
	current_pid_filename = savestr(server_options->pid_filename, 0);
	if (current_pid_filename == NULL) {
		ns_error(ns_log_config,
			 "savestr() failed in update_pid_file()");
		return;
	}
	fp = write_open(current_pid_filename);
	if (fp != NULL) {
		(void) fprintf(fp, "%ld\n", (long)getpid());
		(void) fclose(fp);
	} else
		ns_error(ns_log_config, "couldn't create pid file '%s'",
			 server_options->pid_filename);
}

/*
 * XXX This function will eventually be public and will be relocated to
 *     the UNIX OS support library.
 */

static int
os_change_directory(const char *name) {
	struct stat sb;

	if (name == NULL ||
	    *name == '\0') {
		errno = EINVAL;
		return (0);
	}

	if (chdir(name) < 0)
		return (0);

	if (stat(name, &sb) < 0) {
		ns_error(ns_log_os, "stat(%s) failed: %s", name,
			 strerror(errno));
		return (1);
	}
	if (sb.st_mode & S_IWOTH)
		ns_warning(ns_log_os, "directory %s is world-writable", name);

	return (1);
}

static void
periodic_getnetconf(evContext ctx, void *uap, struct timespec due,
		    struct timespec inter)
{
	getnetconf(1);
}
				   
static void
set_interval_timer(int which_timer, int interval) {
	evTimerID *tid = NULL;
	evTimerFunc func = NULL;

	switch (which_timer) {
	case CLEAN_TIMER:
		tid = &clean_timer;
		func = ns_cleancache;
		break;
	case INTERFACE_TIMER:
		tid = &interface_timer;
		func = periodic_getnetconf;
		break;
	case STATS_TIMER:
		tid = &stats_timer;
		func = ns_logstats;
		break;
	default:
		ns_panic(ns_log_config, 1,
			 "set_interval_timer: unknown timer %d", which_timer);
	}
	if ((active_timers & which_timer) != 0) {
		if (interval > 0) {
			if (evResetTimer(ev, *tid, func, NULL, 
					 evAddTime(evNowTime(), 
						   evConsTime(interval, 0)),
					 evConsTime(interval, 0)) < 0)
				ns_error(ns_log_config,
				 "evResetTimer %d interval %d failed: %s",
					 which_timer, interval,
					 strerror(errno));
		} else {
			if (evClearTimer(ev, *tid) < 0)
				ns_error(ns_log_config,
					 "evClearTimer %d failed: %s",
					 which_timer, strerror(errno));
			else
				active_timers &= ~which_timer;
		}
	} else if (interval > 0) {
		if (evSetTimer(ev, func, NULL, 
			       evAddTime(evNowTime(), 
					 evConsTime(interval, 0)),
			       evConsTime(interval, 0), tid) < 0)
			ns_error(ns_log_config,
				 "evSetTimer %d interval %d failed: %s",
				 which_timer, interval, strerror(errno));
		else
			active_timers |= which_timer;
	}
}

/*
 * Set all named global options based on the global options structure
 * generated by the parser.
 */
void
set_options(options op, int is_default) {
	listen_info li;
	INSIST(op != NULL);

	if (op->listen_list == NULL) {
		ip_match_list iml;
		ip_match_element ime;
		struct in_addr address;

		op->listen_list = new_listen_info_list();

		address.s_addr = htonl(INADDR_ANY);
		iml = new_ip_match_list();
		ime = new_ip_match_pattern(address, 0);
		add_to_ip_match_list(iml, ime);
		add_listen_on(op, htons(NS_DEFAULTPORT), iml);
	}
	if (server_options != NULL)
		free_options(server_options);
	server_options = op;

	/* XXX should validate pid filename */
	INSIST(op->pid_filename != NULL);

	if (op->directory && !os_change_directory(op->directory))
		ns_panic(ns_log_config, 0, "can't change directory to %s: %s",
			 op->directory, strerror(errno));

	/* XXX currently a value of 0 means "use default"; it would be
	   better if the options block had a "attributes updated" vector
	   (like the way X deals with GC updates) */

	if (!op->transfers_in)
		op->transfers_in = DEFAULT_XFERS_RUNNING;
	else if (op->transfers_in > MAX_XFERS_RUNNING) {
		ns_warning(ns_log_config,
		   "the maximum number of concurrent inbound transfers is %d",
			   MAX_XFERS_RUNNING);
		op->transfers_in = MAX_XFERS_RUNNING;
	}

	if (!op->transfers_per_ns)
		op->transfers_per_ns = DEFAULT_XFERS_PER_NS;

	if (!op->max_transfer_time_in)
		op->max_transfer_time_in = MAX_XFER_TIME;

	/* XXX currently transfers_out is not used */

	/*
	 * Limits
	 */

#ifdef HAVE_GETRUSAGE
	ns_rlimit(Datasize, op->data_size);
	ns_rlimit(Stacksize, op->stack_size);
	ns_rlimit(Coresize, op->core_size);
	ns_rlimit(Files, op->files);
#else
	ns_info(ns_log_config, "cannot set resource limits on this system");
#endif


	/*
	 * Timers
	 */
	set_interval_timer(CLEAN_TIMER, server_options->clean_interval);
	set_interval_timer(INTERFACE_TIMER,
			   server_options->interface_interval);
	set_interval_timer(STATS_TIMER, server_options->stats_interval);

	options_installed = 1;
	default_options_installed = is_default;
}

void 
use_default_options() {
	set_options(new_options(), 1);	
}

/*
 * IP Matching Lists
 */

ip_match_list
new_ip_match_list() {
	ip_match_list iml;

	iml = (ip_match_list)memget(sizeof (struct ip_match_list));
	if (iml == NULL)
		panic("memget failed in new_ip_match_list", NULL);
	iml->first = NULL;
	iml->last = NULL;
	return (iml);
}

void
free_ip_match_list(ip_match_list iml) {
	ip_match_element ime, next_element;

	for (ime = iml->first; ime != NULL; ime = next_element) {
		next_element = ime->next;
		memput(ime, sizeof *ime);
	}
	memput(iml, sizeof *iml);
}	

ip_match_element
new_ip_match_pattern(struct in_addr address, u_int mask_bits) {
	ip_match_element ime;
	u_int32_t mask;

	ime = (ip_match_element)memget(sizeof (struct ip_match_element));
	if (ime == NULL)
		panic("memget failed in new_ip_match_pattern", NULL);
	ime->type = ip_match_pattern;
	ime->flags = 0;
	ime->u.direct.address = address;
	if (mask_bits == 0)
		/* can't shift >= the size of a type in bits, so 
		   we deal with an empty mask here */
		mask = 0;
	else {
		/* set the 'mask_bits' most significant bits */
		mask = 0xffffffffU;
		mask >>= (32 - mask_bits);
		mask <<= (32 - mask_bits);
	}
	mask = ntohl(mask);
	ime->u.direct.mask.s_addr = mask;
	ime->next = NULL;
	if (!ina_onnet(ime->u.direct.address, ime->u.direct.address,
		       ime->u.direct.mask)) {
		memput(ime, sizeof *ime);
		ime = NULL;
	}
	return (ime);
}

ip_match_element
new_ip_match_mask(struct in_addr address, struct in_addr mask) {
	ip_match_element ime;

	ime = (ip_match_element)memget(sizeof (struct ip_match_element));
	if (ime == NULL)
		panic("memget failed in new_ip_match_pattern", NULL);
	ime->type = ip_match_pattern;
	ime->flags = 0;
	ime->u.direct.address = address;
	ime->u.direct.mask = mask;
	ime->next = NULL;
	if (!ina_onnet(ime->u.direct.address, ime->u.direct.address,
		       ime->u.direct.mask)) {
		memput(ime, sizeof *ime);
		ime = NULL;
	}
	return (ime);
}

ip_match_element
new_ip_match_indirect(ip_match_list iml) {
	ip_match_element ime;

	INSIST(iml != NULL);

	ime = (ip_match_element)memget(sizeof (struct ip_match_element));
	if (ime == NULL)
		panic("memget failed in new_ip_match_indirect", NULL);
	ime->type = ip_match_indirect;
	ime->flags = 0;
	ime->u.indirect.list = iml;
	ime->next = NULL;
	return (ime);
}

ip_match_element
new_ip_match_localhost() {
	ip_match_element ime;

	ime = (ip_match_element)memget(sizeof (struct ip_match_element));
	if (ime == NULL)
		panic("memget failed in new_ip_match_localhost", NULL);
	ime->type = ip_match_localhost;
	ime->flags = 0;
	ime->u.indirect.list = NULL;
	ime->next = NULL;
	return (ime);
}

ip_match_element
new_ip_match_localnets() {
	ip_match_element ime;

	ime = (ip_match_element)memget(sizeof (struct ip_match_element));
	if (ime == NULL)
		panic("memget failed in new_ip_match_localnets", NULL);
	ime->type = ip_match_localnets;
	ime->flags = 0;
	ime->u.indirect.list = NULL;
	ime->next = NULL;
	return (ime);
}

void
ip_match_negate(ip_match_element ime) {
	if (ime->flags & IP_MATCH_NEGATE)
		ime->flags &= ~IP_MATCH_NEGATE;
	else
		ime->flags |= IP_MATCH_NEGATE;
}

void
add_to_ip_match_list(ip_match_list iml, ip_match_element ime) {
	INSIST(iml != NULL);
	INSIST(ime != NULL);

	if (iml->last != NULL)
		iml->last->next = ime;
	ime->next = NULL;
	iml->last = ime;
	if (iml->first == NULL)
		iml->first = ime;
}

void
dprint_ip_match_list(int category, ip_match_list iml, int indent,
		     char *allow, char *deny) {
	ip_match_element ime;
	char spaces[40+1];
	char addr_text[sizeof "255.255.255.255"];
	char mask_text[sizeof "255.255.255.255"];
	char *tmp;

	INSIST(iml != NULL);

	if (indent > 40)
		indent = 40;
	if (indent)
		memset(spaces, ' ', indent);
	spaces[indent] = '\0';

	for (ime = iml->first; ime != NULL; ime = ime->next) {
		switch (ime->type) {
		case ip_match_pattern:
			memset(addr_text, 0, sizeof addr_text);
			strncpy(addr_text, inet_ntoa(ime->u.direct.address),
				((sizeof addr_text) - 1));
			memset(mask_text, 0, sizeof mask_text);
			strncpy(mask_text, inet_ntoa(ime->u.direct.mask),
				((sizeof mask_text) - 1));
			ns_debug(category, 7, "%s%saddr: %s, mask: %s",
				 spaces,
				 (ime->flags & IP_MATCH_NEGATE) ? deny : allow,
				 addr_text, mask_text);
			break;
		case ip_match_localhost:
			ns_debug(category, 7, "%s%slocalhost", spaces,
				 (ime->flags & IP_MATCH_NEGATE) ?
				 deny : allow);
			break;
		case ip_match_localnets:
			ns_debug(category, 7, "%s%slocalnets", spaces,
				 (ime->flags & IP_MATCH_NEGATE) ?
				 deny : allow);
			break;
		case ip_match_indirect:
			ns_debug(category, 7, "%s%sindirect list %p", spaces,
				 (ime->flags & IP_MATCH_NEGATE) ? deny : allow,
				 ime->u.indirect.list);
			if (ime->u.indirect.list != NULL)
				dprint_ip_match_list(category,
						     ime->u.indirect.list,
						     indent+2, allow, deny);
			break;
		default:
			panic("unexpected ime type in dprint_ip_match_list()",
			      NULL);
		}
	}
}

int
ip_match_address(ip_match_list iml, struct in_addr address) {
	ip_match_element ime;
	int ret;
	int indirect;

	INSIST(iml != NULL);
	for (ime = iml->first; ime != NULL; ime = ime->next) {
		switch (ime->type) {
		case ip_match_pattern:
			indirect = 0;
			break;
		case ip_match_indirect:
			indirect = 1;
			break;
		case ip_match_localhost:
			ime->u.indirect.list = local_addresses;
			indirect = 1;
			break;
		case ip_match_localnets:
			ime->u.indirect.list = local_networks;
			indirect = 1;
			break;
		default:
			indirect = 0;	/* Make gcc happy. */
			panic("unexpected ime type in ip_match_address()",
			      NULL);
		}
		if (indirect) {
			ret = ip_match_address(ime->u.indirect.list, address);
			if (ret >= 0) {
				if (ime->flags & IP_MATCH_NEGATE)
					ret = (ret) ? 0 : 1;
				return (ret);
			}
		} else {
			if (ina_onnet(address, ime->u.direct.address,
				      ime->u.direct.mask)) {
				if (ime->flags & IP_MATCH_NEGATE)
					return (0);
				else
					return (1);
			}
		}
	}
	return (-1);
}

int
ip_address_allowed(ip_match_list iml, struct in_addr address) {
	int ret;

	if (iml == NULL)
		return (0);
	ret = ip_match_address(iml, address);
	if (ret < 0)
		ret = 0;
	return (ret);
}

int
ip_match_network(ip_match_list iml, struct in_addr address,
		 struct in_addr mask) {
	ip_match_element ime;
	int ret;
	int indirect;

	INSIST(iml != NULL);
	for (ime = iml->first; ime != NULL; ime = ime->next) {
		switch (ime->type) {
		case ip_match_pattern:
			indirect = 0;
			break;
		case ip_match_indirect:
			indirect = 1;
			break;
		case ip_match_localhost:
			ime->u.indirect.list = local_addresses;
			indirect = 1;
			break;
		case ip_match_localnets:
			ime->u.indirect.list = local_networks;
			indirect = 1;
			break;
		default:
			indirect = 0;	/* Make gcc happy. */
			panic("unexpected ime type in ip_match_network()",
			      NULL);
		}
		if (indirect) {
			ret = ip_match_network(ime->u.indirect.list,
					       address, mask);
			if (ret >= 0) {
				if (ime->flags & IP_MATCH_NEGATE)
					ret = (ret) ? 0 : 1;
				return (ret);
			}
		} else {
			if (address.s_addr == ime->u.direct.address.s_addr &&
			    mask.s_addr == ime->u.direct.mask.s_addr) {
				if (ime->flags & IP_MATCH_NEGATE)
					return (0);
				else
					return (1);
			}
		}
	}
	return (-1);
}

int
distance_of_address(ip_match_list iml, struct in_addr address) {
	ip_match_element ime;
	int ret;
	int indirect;
	int distance;

	INSIST(iml != NULL);
	for (distance = 1, ime = iml->first;
	     ime != NULL; ime = ime->next, distance++) {
		switch (ime->type) {
		case ip_match_pattern:
			indirect = 0;
			break;
		case ip_match_indirect:
			indirect = 1;
			break;
		case ip_match_localhost:
			ime->u.indirect.list = local_addresses;
			indirect = 1;
			break;
		case ip_match_localnets:
			ime->u.indirect.list = local_networks;
			indirect = 1;
			break;
		default:
			indirect = 0;	/* Make gcc happy. */
			panic("unexpected ime type in distance_of_address()",
			      NULL);
		}
		if (indirect) {
			ret = ip_match_address(ime->u.indirect.list, address);
			if (ret >= 0) {
				if (ime->flags & IP_MATCH_NEGATE)
					ret = (ret) ? 0 : 1;
				if (distance > MAX_TOPOLOGY_DISTANCE)
					distance = MAX_TOPOLOGY_DISTANCE;
				if (ret)
					return (distance);
				else
					return (MAX_TOPOLOGY_DISTANCE);
			}
		} else {
			if (ina_onnet(address, ime->u.direct.address,
				      ime->u.direct.mask)) {
				if (distance > MAX_TOPOLOGY_DISTANCE)
					distance = MAX_TOPOLOGY_DISTANCE;
				if (ime->flags & IP_MATCH_NEGATE)
					return (MAX_TOPOLOGY_DISTANCE);
				else
					return (distance);
			}
		}
	}
	return (UNKNOWN_TOPOLOGY_DISTANCE);
}

int
ip_match_is_none(ip_match_list iml) {
	ip_match_element ime;

	if (iml == NULL)
		return (1);
	ime = iml->first;
	if (ime->type == ip_match_indirect) {
		if (ime->flags & IP_MATCH_NEGATE)
			return (0);
		iml = ime->u.indirect.list;
		if (iml == NULL)
			return (0);
		ime = iml->first;
	}
	if (ime->type == ip_match_pattern) {
		if ((ime->flags & IP_MATCH_NEGATE) &&
		    ime->u.direct.address.s_addr == 0 && 
		    ime->u.direct.mask.s_addr == 0)
			return (1);
	}
	return (0);
}


/*
 * Forwarder glue
 *
 * XXX  This will go away when the rest of bind understands 
 *      forward zones.
 */

void
add_forwarder(options op, struct in_addr address) {
	struct fwdinfo *fip = NULL, *ftp = NULL;

#ifdef SLAVE_FORWARD
	int forward_count = 0;
#endif

	INSIST(op != NULL);

	/* On multiple forwarder lines, move to end of the list. */
#ifdef SLAVE_FORWARD
	if (op->fwdtab != NULL) {
		forward_count++;
		for (fip = op->fwdtab; fip->next != NULL; fip = fip->next)
			forward_count++;
	}
#else
	if (op->fwdtab != NULL) {
		for (fip = op->fwdtab; fip->next != NULL; fip = fip->next) {
			;
		}
	}
#endif /* SLAVE_FORWARD */

	ftp = (struct fwdinfo *)memget(sizeof(struct fwdinfo));
	if (!ftp)
		panic("memget failed in add_forwarder", NULL);
	ftp->fwdaddr.sin_family = AF_INET;
	ftp->fwdaddr.sin_addr = address;
	ftp->fwdaddr.sin_port = ns_port;
#ifdef FWD_LOOP
	if (aIsUs(ftp->fwdaddr.sin_addr)) {
		ns_error(ns_log_config, "forwarder '%s' ignored, my address",
			 inet_ntoa(address));
		memput(ftp, sizeof *ftp);
		return;
	}
#endif /* FWD_LOOP */
	ns_debug(ns_log_config, 2, "added forwarder %s", inet_ntoa(address));
	ftp->next = NULL;
	if (op->fwdtab == NULL)
		op->fwdtab = ftp;	/* First time only */
	else
		fip->next = ftp;
#ifdef SLAVE_FORWARD
	forward_count++;

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
}

void
free_forwarders(struct fwdinfo *fwdtab) {
	struct fwdinfo *ftp, *fnext;

	for (ftp = fwdtab; ftp != NULL; ftp = fnext) {
		fnext = ftp->next;
		memput(ftp, sizeof *ftp);
	}
}

/*
 * Servers 
 */


static server_info
new_server(struct in_addr address) {
	server_info si;

	si = (server_info)memget(sizeof (struct server_info));
	if (si == NULL)
		panic("memget failed in new_server()", NULL);
	si->address = address;
	si->flags = 0U;
	si->transfers = 0;
	si->transfer_format = axfr_use_default;
	si->key_list = NULL;
	si->next = NULL;
	return si;
}

static void
free_server(server_info si) {
	/* Don't free key; it'll be done when the auth table is freed. */
	memput(si, sizeof *si);
}

server_info
find_server(struct in_addr address) {
	server_info si;

	for (si = nameserver_info; si != NULL; si = si->next)
		if (si->address.s_addr == address.s_addr)
			break;
	return (si);
}

static void
add_server(server_info si) {
	ip_match_element ime;

	si->next = nameserver_info;
	nameserver_info = si;

	/*
	 * To ease transition, we'll add bogus nameservers to an
	 * ip matching list.  This will probably be redone when the
	 * merging of nameserver data structures occurs.
	 */
	if (si->flags & SERVER_INFO_BOGUS) {
		ime = new_ip_match_pattern(si->address, 32);
		INSIST(ime != NULL);
		add_to_ip_match_list(bogus_nameservers, ime);
	}
	ns_debug(ns_log_config, 3, "server %s: flags %08x transfers %d",
		 inet_ntoa(si->address), si->flags, si->transfers);
	if (si->key_list != NULL)
		dprint_key_info_list(si->key_list);
}

static void
free_nameserver_info() {
	server_info si_next, si;
	
	for (si = nameserver_info; si != NULL; si = si_next) {
		si_next = si->next;
		free_server(si);
	}
	nameserver_info = NULL;
	if (bogus_nameservers != NULL) {
		free_ip_match_list(bogus_nameservers);
		bogus_nameservers = NULL;
	}
}

server_config
begin_server(struct in_addr address) {
	server_config sc;
	
	sc.opaque = new_server(address);
	return (sc);
}

void
end_server(server_config sc, int should_install) {
	server_info si;

	si = sc.opaque;

	INSIST(si != NULL);
	
	if (should_install)
		add_server(si);
	else
		free_server(si);
	sc.opaque = NULL;
}

void
set_server_option(server_config sc, int bool_opt, int value) {
	server_info si;

	si = sc.opaque;

	INSIST(si != NULL);

	switch (bool_opt) {
	case SERVER_INFO_BOGUS:
		if (value)
			si->flags |= bool_opt;
		else
			si->flags &= ~bool_opt;
		break;
	default:
		panic("unexpected option in set_server_option", NULL);
	}
}

void
set_server_transfers(server_config sc, int transfers) {
	server_info si;

	si = sc.opaque;

	INSIST(si != NULL);

	if (transfers < 0)
		transfers = 0;
	si->transfers = transfers;
}

void
set_server_transfer_format(server_config sc,
			   enum axfr_format transfer_format) {
	server_info si;

	si = sc.opaque;

	INSIST(si != NULL);

	si->transfer_format = transfer_format;
}

void
add_server_key_info(server_config sc, key_info ki) {
	server_info si;

	si = sc.opaque;

	INSIST(si != NULL);

	if (si->key_list == NULL)
		si->key_list = new_key_info_list();
	add_to_key_info_list(si->key_list, ki);
}

/*
 * Keys
 */

key_info
new_key_info(char *name, char *algorithm, char *secret) {
	key_info ki;

	INSIST(name != NULL);
	INSIST(algorithm != NULL);
	INSIST(secret != NULL);
	ki = (key_info)memget(sizeof (struct key_info));
	if (ki == NULL)
		panic("memget failed in new_key_info", NULL);
	ki->name = name;
	ki->algorithm = algorithm;
	ki->secret = secret;
	return (ki);
}

void
free_key_info(key_info ki) {
	INSIST(ki != NULL);
	freestr(ki->name);
	freestr(ki->algorithm);
	freestr(ki->secret);
	memput(ki, sizeof *ki);
}

void
dprint_key_info(key_info ki) {
	INSIST(ki != NULL);
	ns_debug(ns_log_config, 7, "key %s", ki->name);
	ns_debug(ns_log_config, 7, "  algorithm %s", ki->algorithm);
	ns_debug(ns_log_config, 7, "  secret %s", ki->secret);
}

key_info_list
new_key_info_list() {
	key_info_list kil;

	kil = (key_info_list)memget(sizeof (struct key_info_list));
	if (kil == NULL)
		panic("memget failed in new_key_info_list()", NULL);
	kil->first = NULL;
	kil->last = NULL;
	return (kil);
}

void
free_key_info_list(key_info_list kil) {
	key_list_element kle, kle_next;

	INSIST(kil != NULL);
	for (kle = kil->first; kle != NULL; kle = kle_next) {
		kle_next = kle->next;
		/* note we do NOT free kle->info */
		memput(kle, sizeof *kle);
	}
	memput(kil, sizeof *kil);
}

void
add_to_key_info_list(key_info_list kil, key_info ki) {
	key_list_element kle;

	INSIST(kil != NULL);
	INSIST(ki != NULL);

	kle = (key_list_element)memget(sizeof (struct key_list_element));
	if (kle == NULL)
		panic("memget failed in add_to_key_info_list()", NULL);
	kle->info = ki;
	if (kil->last != NULL)
		kil->last->next = kle;
	kle->next = NULL;
	kil->last = kle;
	if (kil->first == NULL)
		kil->first = kle;
}

void
dprint_key_info_list(key_info_list kil) {
	key_list_element kle;

	INSIST(kil != NULL);

	for (kle = kil->first; kle != NULL; kle = kle->next)
		dprint_key_info(kle->info);
}

/*
 * Logging.
 */

log_config
begin_logging() {
	log_config log_cfg;
	log_context lc;

	log_cfg = (log_config)memget(sizeof (struct log_config));
	if (log_cfg == NULL)
		ns_panic(ns_log_config, 0,
			 "memget failed creating log_config");
	if (log_new_context(ns_log_max_category, logging_categories, &lc) < 0)
		ns_panic(ns_log_config, 0,
			 "log_new_context() failed: %s", strerror(errno));
	log_cfg->log_ctx = lc;
	log_cfg->eventlib_channel = NULL;
	log_cfg->packet_channel = NULL;
	log_cfg->default_debug_active = 0;
	return (log_cfg);
}

void
add_log_channel(log_config log_cfg, int category, log_channel chan) {
	log_channel_type type;

	INSIST(log_cfg != NULL);
	
	type = log_get_channel_type(chan);
	if (category == ns_log_eventlib) {
		if (type != log_file && type != log_null) {
			ns_error(ns_log_config,
	 "must specify a file or null channel for the eventlib category");
			return;
		}
		if (log_cfg->eventlib_channel != NULL) {
			ns_error(ns_log_config,
	 "only one channel allowed for the eventlib category");
			return;
		}
		log_cfg->eventlib_channel = chan;
	}
	if (category == ns_log_packet) {
		if (type != log_file && type != log_null) {
			ns_error(ns_log_config,
	 "must specify a file or null channel for the packet category");
			return;
		}
		if (log_cfg->packet_channel != NULL) {
			ns_error(ns_log_config,
	 "only one channel allowed for the packet category");
			return;
		}
		log_cfg->packet_channel = chan;
	}

	if (log_add_channel(log_cfg->log_ctx, category, chan) < 0) {
		ns_error(ns_log_config, "log_add_channel() failed");
		return;
	}

	if (chan == debug_channel)
		log_cfg->default_debug_active = 1;
}

void
open_special_channels() {
	int using_null = 0;

	if (log_open_stream(eventlib_channel) == NULL) {
		eventlib_channel = null_channel;
		using_null = 1;
	}
	if (log_open_stream(packet_channel) == NULL) {
		packet_channel = null_channel;
		using_null = 1;
	}

	if (using_null &&
	    log_open_stream(null_channel) == NULL)
		ns_panic(ns_log_config, 1, "couldn't open null channel");
}

void
set_logging(log_config log_cfg, int is_default) {
	log_context lc;
	int skip_debug = 0;

	INSIST(log_cfg != NULL);
	lc = log_cfg->log_ctx;

	/*
	 * Add the default category if it's not in the context already.
	 */
	if (!log_category_is_active(lc, ns_log_default)) {
		add_log_channel(log_cfg, ns_log_default, debug_channel);
		add_log_channel(log_cfg, ns_log_default, syslog_channel);
	}

	/*
	 * Add the panic category if it's not in the context already.
	 */
	if (!log_category_is_active(lc, ns_log_panic)) {
		add_log_channel(log_cfg, ns_log_panic, stderr_channel);
		add_log_channel(log_cfg, ns_log_panic, syslog_channel);
	}

	/*
	 * Add the eventlib category if it's not in the context already.
	 */
	if (!log_category_is_active(lc, ns_log_eventlib))
		add_log_channel(log_cfg, ns_log_eventlib, debug_channel);

	/*
	 * Add the packet category if it's not in the context already.
	 */
	if (!log_category_is_active(lc, ns_log_packet))
		add_log_channel(log_cfg, ns_log_packet, debug_channel);

#ifdef DEBUG
	/*
	 * Preserve debugging state.
	 */
	log_option(lc, LOG_OPTION_DEBUG, debug);
	log_option(lc, LOG_OPTION_LEVEL, debug);
#endif

	/*
	 * Special case for query-log, so we can co-exist with the command
	 * line option and SIGWINCH.
	 */
	if (log_category_is_active(lc, ns_log_queries))
		qrylog = 1;

	/*
	 * Cleanup the old context.
	 */
	if (need_logging_free)
		log_free_context(log_ctx);

	/*
	 * The default file channels will never have their reference counts
	 * drop to zero, and so they will not be closed by the logging system
	 * when log_free_context() is called.  We don't want to keep files
	 * open unnecessarily, and we want them to behave like user-created
	 * channels, so we close them here.
	 */
	if (log_get_stream(debug_channel) != stderr)
		(void)log_close_stream(debug_channel);
	(void)log_close_stream(null_channel);

	/*
	 * Install the new context.
	 */
	log_ctx = lc;
	eventlib_channel = log_cfg->eventlib_channel;
	packet_channel = log_cfg->packet_channel;

#ifdef DEBUG
	if (debug) {
		open_special_channels();
		evSetDebug(ev, debug, log_get_stream(eventlib_channel));
	}
#endif

	log_ctx_valid = 1;
	need_logging_free = 1;
	logging_installed = 1;
	default_logging_installed = is_default;
}

void
end_logging(log_config log_cfg, int should_install) {
	if (should_install)
		set_logging(log_cfg, 0);
	else
		log_free_context(log_cfg->log_ctx);
	memput(log_cfg, sizeof (struct log_config));
}

void
use_default_logging() {
	log_config log_cfg;

	log_cfg = begin_logging();
	set_logging(log_cfg, 1);
	memput(log_cfg, sizeof (struct log_config));
}

static void
init_default_log_channels() {
	FILE *null_stream;
	u_int flags;
	char *name;
	FILE *stream;

	syslog_channel = log_new_syslog_channel(0, log_info, LOG_DAEMON);
	if (syslog_channel == NULL || log_inc_references(syslog_channel) < 0)
		ns_panic(ns_log_config, 0, "couldn't create syslog_channel");

	flags = LOG_USE_CONTEXT_LEVEL|LOG_REQUIRE_DEBUG;
	if (foreground) {
		name = NULL;
		stream = stderr;
	} else {
		name = _PATH_DEBUG;
		stream = NULL;
	}
	debug_channel = log_new_file_channel(flags, log_info, name, stream,
					     0, ULONG_MAX);
	if (debug_channel == NULL || log_inc_references(debug_channel) < 0)
		ns_panic(ns_log_config, 0, "couldn't create debug_channel");

	stderr_channel = log_new_file_channel(0, log_info, NULL, stderr,
					      0, ULONG_MAX);
	if (stderr_channel == NULL || log_inc_references(stderr_channel) < 0)
		ns_panic(ns_log_config, 0, "couldn't create stderr_channel");

	null_channel = log_new_file_channel(LOG_CHANNEL_OFF, log_info,
					    _PATH_DEVNULL, NULL, 0, ULONG_MAX);
	if (null_channel == NULL || log_inc_references(null_channel) < 0)
		ns_panic(ns_log_config, 0, "couldn't create null_channel");
}

static void
shutdown_default_log_channels() {
	log_free_channel(syslog_channel);
	log_free_channel(debug_channel);
	log_free_channel(stderr_channel);
	log_free_channel(null_channel);
}

void
init_logging() {
	int i;
	int size;
	const struct ns_sym *s;
	char category_name[256];

	size = ns_log_max_category * (sizeof (char *));

	logging_categories = (char **)memget(size);
	if (logging_categories == NULL)
		ns_panic(ns_log_config, 0, "memget failed in init_logging");
	memset(logging_categories, 0, size);
	for (s = category_constants; s != NULL && s->name != NULL; s++) {
		sprintf(category_name, "%s: ", s->name);
		logging_categories[s->number] = savestr(category_name, 1);
	}

	init_default_log_channels();
	use_default_logging();
}

void
shutdown_logging() {
	int size;
	const struct ns_sym *s;

	evSetDebug(ev, 0, NULL);
	shutdown_default_log_channels();
	log_free_context(log_ctx);

	for (s = category_constants; s != NULL && s->name != NULL; s++)
		freestr(logging_categories[s->number]);
	size = ns_log_max_category * (sizeof (char *));
	memput(logging_categories, size);
}

/*
 * Main Loader
 */

void
init_configuration() {
	/*
	 * Remember initial limits for use if "default" is specified in
	 * a config file.
	 */
#ifdef HAVE_GETRUSAGE
	get_initial_limits();
#endif
	zone_symbol_table = new_symbol_table(ZONE_SYM_TABLE_SIZE, NULL);
	use_default_options();
	parser_initialize();
	config_initialized = 1;
}

void
shutdown_configuration() {
	REQUIRE(config_initialized);

	if (server_options != NULL) {
		free_options(server_options);
		server_options = NULL;
	}
	if (current_pid_filename != NULL)
		freestr(current_pid_filename);
	free_nameserver_info();
	free_symbol_table(zone_symbol_table);
	parser_shutdown();
	config_initialized = 0;
}

void
load_configuration(const char *filename) {
	REQUIRE(config_initialized);

	ns_debug(ns_log_config, 3, "load configuration %s", filename);

	loading = 1;

	/*
	 * Clean up any previous configuration and initialize
	 * global data structures we'll be updating.
	 */
	free_nameserver_info();
	bogus_nameservers = new_ip_match_list();

	options_installed = 0;
	logging_installed = 0;

	parse_configuration(filename);

	/*
	 * If the user didn't specify logging or options, but they previously
	 * had specified one or both of them, then we need to
	 * re-establish the default environment.  We have to be careful
	 * about when we install default options because the parser
	 * must respect limits (e.g. data-size, number of open files)
	 * specified in the options file.  In the ordinary case where the
	 * options section isn't changing on a zone reload, it would be bad
	 * to lower these limits temporarily, because we might not survive
	 * to the point where they get raised back again.  The logging case
	 * has similar motivation -- we don't want to override the existing
	 * logging scheme (perhaps causing log messages to go somewhere 
	 * unexpected) when the user hasn't expressed a desire for a new
	 * scheme.
	 */
	if (!logging_installed)
		use_default_logging();
	if (!options_installed && !default_options_installed) {
		use_default_options();
		ns_warning(ns_log_config, "re-establishing default options");
	}

	update_pid_file();

	/* Init or reinit the interface/port list and associated sockets. */
	getnetconf(0);
	opensocket_f();

	initial_configuration = 0;
	loading = 0;
}
