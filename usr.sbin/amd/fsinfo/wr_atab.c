/*
 * Copyright (c) 1989 Jan-Simon Pendry
 * Copyright (c) 1989 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)wr_atab.c	8.1 (Berkeley) 6/6/93
 *
 * $Id: wr_atab.c,v 5.2.2.1 1992/02/09 15:09:44 jsp beta $
 *
 */

#include "../fsinfo/fsinfo.h"

/*
 * Write a sequence of automount mount map entries
 */
static int write_amount_info(af, ap, sk)
FILE *af;
automount *ap;
int sk;
{
	int errors = 0;
	if (ap->a_mount) {
		/*
		 * A pseudo-directory.
		 * This can also be a top-level directory, in which
		 * case the type:=auto is not wanted...
		 *
		 * type:=auto;fs:=${map};pref:=whatever/
		 */
		automount *ap2;
		if (strlen(ap->a_name) > sk) {
			fprintf(af, "%s type:=auto;fs:=${map};pref:=%s/\n",
				ap->a_name + sk, ap->a_name + sk);
		}
		ITER(ap2, automount, ap->a_mount)
			errors += write_amount_info(af, ap2, sk);
	} else if (ap->a_mounted) {
		/*
		 * A mounted partition
		 * type:=link [ link entries ] type:=nfs [ nfs entries ]
		 */
		dict_data *dd;
		dict_ent *de = ap->a_mounted;
		int done_type_link = 0;
		char *key = ap->a_name + sk;

		/*
		 * Output the map key
		 */
		fputs(key, af);

		/*
		 * First output any Link locations that would not
		 * otherwise be correctly mounted.  These refer
		 * to filesystem which are not mounted in the same
		 * place which the automounter would use.
		 */
		ITER(dd, dict_data, &de->de_q) {
			mount *mp = (mount *) dd->dd_data;
			/*
			 * If the mount point and the exported volname are the
			 * same then this filesystem will be recognised by
			 * the restart code - so we don't need to put out a
			 * special rule for it.
			 */
			if (mp->m_dk->d_host->h_lochost) {
				char amountpt[1024];
				compute_automount_point(amountpt, mp->m_dk->d_host, mp->m_exported->m_volname);
				if (strcmp(mp->m_dk->d_mountpt, amountpt) != 0) {
					/*
					 * ap->a_volname is the name of the aliased volume
					 * mp->m_name is the mount point of the filesystem
					 * mp->m_volname is the volume name of the filesystems
					 */

					/*
					 * Find length of key and volume names
					 */
					int avlen = strlen(ap->a_volname);
					int mnlen = strlen(mp->m_volname);
					/*
					 * Make sure a -type:=link is output once
					 */
					if (!done_type_link) {
						done_type_link = 1;
						fputs(" -type:=link", af);
					}
					/*
					 * Output a selector for the hostname,
					 * the device from which to mount and
					 * where to mount.  This will correspond
					 * to the values output for the fstab.
					 */
					if (mp->m_dk->d_host->h_lochost)
						fprintf(af, " host==%s", mp->m_dk->d_host->h_lochost);
					else
						fprintf(af, " hostd==%s", mp->m_dk->d_host->h_hostname);
					fprintf(af, ";fs:=%s", mp->m_name);
					/*
					 * ... and a sublink if needed
					 */
					if (mnlen < avlen) {
						char *sublink = ap->a_volname + mnlen + 1;
						fprintf(af, "/%s", sublink);
					}
					fputs(" ||", af);
				}
			}
		}

		/*
		 * Next do the NFS locations
		 */

		if (done_type_link)
			fputs(" -", af);

		ITER(dd, dict_data, &de->de_q) {
			mount *mp = (mount *) dd->dd_data;
			int namelen = mp->m_name_len;
			int exp_namelen = mp->m_exported->m_name_len;
			int volnlen = strlen(ap->a_volname);
			int mvolnlen = strlen(mp->m_volname);
			fputc(' ', af);
#ifdef notdef
			fprintf(af, "\\\n /* avolname = %s, mname = %s,\n  * mvolname = %s, mexp_name = %s,\n  * mexp_volname = %s\n  */\\\n",
				ap->a_volname, mp->m_name, mp->m_volname, mp->m_exported->m_name, mp->m_exported->m_volname);
#endif
			/*
			 * Output any selectors
			 */
			if (mp->m_sel)
				fprintf(af, "%s;", mp->m_sel);
			/*
			 * Print host and volname of exported filesystem
			 */
			fprintf(af, "rhost:=%s",
				mp->m_dk->d_host->h_lochost ?
				mp->m_dk->d_host->h_lochost :
				mp->m_dk->d_host->h_hostname);
			fprintf(af, ";rfs:=%s", mp->m_exported->m_volname);
			/*
			 * Now determine whether a sublink is required.
			 */
			if (exp_namelen < namelen || mvolnlen < volnlen) {
				char sublink[1024];
				sublink[0] = '\0';
				if (exp_namelen < namelen) {
					strcat(sublink, mp->m_name + exp_namelen + 1);
					if (mvolnlen < volnlen)
						strcat(sublink, "/");
				}
				if (mvolnlen < volnlen)
					strcat(sublink, ap->a_volname + mvolnlen + 1);

				fprintf(af, ";sublink:=%s", sublink);
			}
		}
		fputc('\n', af);
	} else if (ap->a_symlink) {
		/*
		 * A specific link.
		 *
		 * type:=link;fs:=whatever
		 */
		fprintf(af, "%s type:=link;fs:=%s\n", ap->a_name + sk, ap->a_symlink);
	}
	return errors;
}

/*
 * Write a single automount configuration file
 */
static int write_amount(q, def)
qelem *q;
char *def;
{
	automount *ap;
	int errors = 0;
	int direct = 0;

	/*
	 * Output all indirect maps
	 */
	ITER(ap, automount, q) {
		FILE *af;
		char *p;
		/*
		 * If there is no a_mount node then this is really
		 * a direct mount, so just keep a count and continue.
		 * Direct mounts are output into a special file during
		 * the second pass below.
		 */
		if (!ap->a_mount) {
			direct++;
			continue;
		}
		p = strrchr(ap->a_name, '/');
		if (!p) p = ap->a_name;
		else p++;
		af = pref_open(mount_pref, p, gen_hdr, ap->a_name);
		if (af) {
			show_new(ap->a_name);
			fputs("/defaults ", af);
			if (*def)
				fprintf(af, "%s;", def);
			fputs("type:=nfs\n", af);
			errors += write_amount_info(af, ap, strlen(ap->a_name) + 1);
			errors += pref_close(af);
		}
	}

	/*
	 * Output any direct map entries which were found during the
	 * previous pass over the data.
	 */
	if (direct) {
		FILE *af = pref_open(mount_pref, "direct.map", info_hdr, "direct mount");
		if (af) {
			show_new("direct mounts");
			fputs("/defaults ", af);
			if (*def)
				fprintf(af, "%s;", def);
			fputs("type:=nfs\n", af);
			ITER(ap, automount, q)
				if (!ap->a_mount)
					errors += write_amount_info(af, ap, 1);
			errors += pref_close(af);
		}
	}
	
	return errors;
}

/*
 * Write all the needed automount configuration files
 */
write_atab(q)
qelem *q;
{
	int errors = 0;

	if (mount_pref) {
		auto_tree *tp;
		show_area_being_processed("write automount", "");
		ITER(tp, auto_tree, q)
			errors += write_amount(tp->t_mount, tp->t_defaults);
	}

	return errors;
}
