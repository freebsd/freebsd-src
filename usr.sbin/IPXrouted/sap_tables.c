/*
 * Copyright (c) 1995 John Hay.  All rights reserved.
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
 *	This product includes software developed by John Hay.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY John Hay AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL John Hay OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/IPXrouted/sap_tables.c,v 1.7 1999/08/28 01:15:04 peter Exp $
 */

#include "defs.h"
#include <string.h>
#include <stdlib.h>

#define FIXLEN(s) { if ((s)->sa_len == 0) (s)->sa_len = sizeof (*(s));}

sap_hash sap_head[SAPHASHSIZ];

void
sapinit(void)
{
	int i;

	for (i=0; i<SAPHASHSIZ; i++)
		sap_head[i].forw = sap_head[i].back = 
					(struct sap_entry *)&sap_head[i];
}

/*
 * This hash use the first 14 letters of the ServName and the ServType
 * to create a 32 bit hash value.
 */
int
saphash(u_short ServType, char *ServName)
{
	int hsh, i;
	char name[SERVNAMELEN];

	bzero(name, SERVNAMELEN);
	strncpy(name, ServName, SERVNAMELEN);
	ServName = name;

	hsh = 0;

#define SMVAL   33

	hsh = hsh * SMVAL + (ServType & 0xff);
	hsh = hsh * SMVAL + (ServType >> 8);

	for (i=0;i<14;i++) {
		hsh = hsh * SMVAL + *ServName++;
		ServName++;
	}

#undef SMVAL

	return hsh;
}

/*
 * Look for an exact match on ServType and ServName. It is
 * mostly used by the function that process SAP RESPONSE packets.
 *
 * A hash is created and used to index into the hash table. Then
 * that list is walk through searching for a match.
 *
 * If no match is found NULL is returned.
 */
struct sap_entry *
sap_lookup(u_short ServType, char *ServName)
{
	register struct sap_entry *sap;
	register struct sap_hash  *sh;
	int hsh;

	hsh = saphash(ServType, ServName);
	sh = &sap_head[hsh & SAPHASHMASK];

	for(sap = sh->forw; sap != (sap_entry *)sh; sap = sap->forw) {
		if ((hsh == sap->hash) &&
		    (ServType == sap->sap.ServType) &&
		    (strncmp(ServName, sap->sap.ServName, SERVNAMELEN) == 0)) {
			return sap;
		}
	}
	return NULL;
}

/*
 * This returns the nearest service of the specified type. If no
 * suitable service is found or if that service is on the interface
 * where the request came from, NULL is returned.
 *
 * When checking interfaces clones must be considered also.
 *
 * XXX TODO:
 * Maybe we can use RIP tables to get the fastest service (ticks).
 */
struct sap_entry *
sap_nearestserver(ushort ServType, struct interface *ifp)
{
	register struct sap_entry *sap;
	register struct sap_entry *csap;
	struct sap_hash  *sh;
	register struct sap_entry *best = NULL;
	register int besthops = HOPCNT_INFINITY;

	sh = sap_head;

	for (; sh < &sap_head[SAPHASHSIZ]; sh++)
		for(sap = sh->forw; sap != (sap_entry *)sh; sap = sap->forw) {
			if (ServType != sap->sap.ServType)
				continue;

			if (ntohs(sap->sap.hops) < besthops) {
				best = sap;
				besthops = ntohs(best->sap.hops);
			}
next:;
		}
	return best;
}

/*
 * Add a entry to the SAP table.
 *
 * If the malloc fail, the entry will silently be thrown away.
 */
void
sap_add(struct sap_info *si, struct sockaddr *from)
{
	register struct sap_entry *nsap;
	register struct sap_hash *sh;

	if (ntohs(si->hops) == HOPCNT_INFINITY)
		return;

	FIXLEN(from);
	nsap = malloc(sizeof(struct sap_entry));
	if (nsap == NULL)
		return;

	nsap->sap = *si;
	nsap->source = *from;
	nsap->clone = NULL;
	nsap->ifp = if_ifwithnet(from);
	nsap->state = RTS_CHANGED;
	nsap->timer = 0;
	nsap->hash = saphash(si->ServType, si->ServName);

	sh = &sap_head[nsap->hash & SAPHASHMASK];

	insque(nsap, sh);
	TRACE_SAP_ACTION("ADD", nsap);
}

/*
 * Change an existing SAP entry. If a clone exist for the old one,
 * check if it is cheaper. If it is change tothe clone, otherwise
 * delete all the clones.
 */
void
sap_change(struct sap_entry *sap, 
           struct sap_info *si,
           struct sockaddr *from)
{
	struct sap_entry *osap = NULL;

	FIXLEN(from);
	TRACE_SAP_ACTION("CHANGE FROM", sap);
	/*
	 * If the hopcount (metric) is HOPCNT_INFINITY (16) it means that
	 * a service has gone down. We should keep it like that for 30
	 * seconds, so that it will get broadcast and then change to a
	 * clone if one exist.
	 */
	if (sap->clone && (ntohs(si->hops) != HOPCNT_INFINITY)) {
		/*
		 * There are three possibilities:
		 * 1. The new path is cheaper than the old one.
		 *      Free all the clones.
		 *
		 * 2. The new path is the same cost as the old ones.
		 *      If it is on the list of clones remove it
		 *      from the clone list and free it.
		 *
		 * 3. The new path is more expensive than the old one.
		 *      Use the values of the first clone and take it
		 *      out of the list, to be freed at the end.
		 */
		osap = sap->clone;
		if (ntohs(osap->sap.hops) > ntohs(si->hops)) {
			struct sap_entry *nsap;

			while (osap) {
				nsap = osap->clone;
				TRACE_SAP_ACTION("DELETE", osap);
				free(osap);
				osap = nsap;
			}
			sap->clone = NULL;
		} else if (ntohs(osap->sap.hops) == ntohs(si->hops)) {
			struct sap_entry *psap;

			psap = sap;
			while (osap) {
				if (equal(&osap->source, from)) {
					psap->clone = osap->clone;
					TRACE_SAP_ACTION("DELETE", osap);
					free(osap);
					osap = psap->clone;
				} else {
					psap = osap;
					osap = osap->clone;
				}
			}
		} else {
		from = &osap->source;
		si = &osap->sap;
		sap->clone = osap->clone;
		}
	}
	sap->sap = *si;
	sap->source = *from;
	sap->ifp = if_ifwithnet(from);
	sap->state = RTS_CHANGED;
	if (ntohs(si->hops) == HOPCNT_INFINITY)
		sap->timer = EXPIRE_TIME;
	else
		sap->timer = 0;

	if (osap) {
		TRACE_SAP_ACTION("DELETE", osap);
		free(osap);
	}
	TRACE_SAP_ACTION("CHANGE TO", sap);
}

/*
 * Add a clone to the specified SAP entry. A clone is a different
 * route to the same service. We must know about them when we use
 * the split horizon algorithm.
 *
 * If the malloc fail, the entry will silently be thrown away.
 */
void 
sap_add_clone(struct sap_entry *sap, 
	      struct sap_info *clone,
	      struct sockaddr *from)
{
	register struct sap_entry *nsap;
	register struct sap_entry *csap;

	if (ntohs(clone->hops) == HOPCNT_INFINITY)
		return;

	FIXLEN(from);
	nsap = malloc(sizeof(struct sap_entry));
	if (nsap == NULL)
		return;

	if (ftrace)
		fprintf(ftrace, "CLONE ADD %04.4X %s.\n", 
			ntohs(clone->ServType),
			clone->ServName);

	nsap->sap = *clone;
	nsap->source = *from;
	nsap->clone = NULL;
	nsap->ifp = if_ifwithnet(from);
	nsap->state = RTS_CHANGED;
	nsap->timer = 0;
	nsap->hash = saphash(clone->ServType, clone->ServName);

	csap = sap;
	while (csap->clone)
		csap = csap->clone;
	csap->clone = nsap;
	TRACE_SAP_ACTION("ADD CLONE", nsap);
}

/*
 * Remove a SAP entry from the table and free the memory
 * used by it.
 *
 * If the service have clone, do a sap_change to it and free
 * the clone.
 */
void
sap_delete(struct sap_entry *sap)
{
	if (sap->clone) {
		sap_change(sap, &sap->clone->sap, &sap->clone->source);
		return;
	}
	remque(sap);
	TRACE_SAP_ACTION("DELETE", sap);
	free(sap);
}
