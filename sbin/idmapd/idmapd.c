/* $FreeBSD$ */
/* $Id: idmapd.c,v 1.5 2003/11/05 14:58:58 rees Exp $ */

/*
 * copyright (c) 2003
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and redistribute
 * this software and such derivative works for any purpose, so long as the name
 * of the university of michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software without specific,
 * written prior authorization.  if the above copyright notice or any other
 * identification of the university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the university
 * of michigan as to its fitness for any purpose, and without warranty by the
 * university of michigan of any kind, either express or implied, including
 * without limitation the implied warranties of merchantability and fitness for
 * a particular purpose. the regents of the university of michigan shall not be
 * liable for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been or is hereafter
 * advised of the possibility of such damages.
 */

/* XXX ignores the domain of received names. */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/queue.h>

#include <nfs4client/nfs4_dev.h>
#include <nfs4client/nfs4_idmap.h>

#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#define DEV_PATH "/dev/nfs4"

#define DOMAIN	"@FreeBSD.org"
#define BADUSER	"nobody"
#define BADGROUP "nogroup"
#define BADUID	65534
#define BADGID	65533

struct idmap_e {
	struct nfs4dev_msg msg;
	TAILQ_ENTRY(idmap_e) next;
};

int fd, verbose;
char *domain = DOMAIN;

TAILQ_HEAD(, idmap_e) upcall_q;

#define add_idmap_e(E) do {	\
  	assert(E != NULL);	\
	TAILQ_INSERT_TAIL(&upcall_q, E, next); \
} while(0)

#define remove_idmap_e(E) do {	\
  	assert(E != NULL && !TAILQ_EMPTY(&upcall_q));	\
	E = TAILQ_FIRST(&upcall_q);	\
	TAILQ_REMOVE(&upcall_q, E, next); \
} while(0)

#define get_idmap_e(E) do {	\
  	if ((E = (struct idmap_e *) malloc(sizeof(struct idmap_e))) == NULL) {\
		fprintf(stderr, "get_idmap_e(): error in malloc\n");\
	} } while(0)

#define put_idmap_e(E) free(E)

/* from marius */
int
validateascii(char *string, u_int32_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (string[i] == '\0')
			break;
		if (string[i] & 0x80)
			return (-1);
	}

	if (string[i] != '\0')
		return (-1);
	return (i + 1);
}

char *
idmap_prune_domain(struct idmap_msg * m)
{
  	size_t i;
	size_t len;
	char * ret = NULL;

	if (m == NULL)
		return NULL;

	len = m->id_namelen;

	if (validateascii(m->id_name, len) < 0) {
		fprintf(stderr, "msg has invalid ascii\n");
		return NULL;
	}

	for (i=0; i < len && m->id_name[i] != '@' ; i++);

	ret = (char *)malloc(i+1);
	if (ret == NULL)
	  return NULL;

	bcopy(m->id_name, ret, i);
	ret[i] = '\0';

	return ret;
}

int
idmap_add_domain(struct idmap_msg * m, char * name)
{
  	size_t len, nlen;

	if (m == NULL || name == NULL)
		return -1;

	len = strlen(name);

	nlen = len + strlen(domain);

	if (nlen > IDMAP_MAXNAMELEN)
	  	return -1;

	bcopy(name, &m->id_name[0], len);
	bcopy(domain, &m->id_name[len], strlen(domain));

	m->id_name[nlen] = '\0';
	m->id_namelen = nlen;

	return 0;
}

int
idmap_name(struct idmap_msg * m, char *name)
{
	if (m == NULL || name == NULL || m->id_namelen != 0)
		return -1;

	if (idmap_add_domain(m, name))
		return -1;

	return 0;
}

int
idmap_id(struct idmap_msg * m, ident_t id)
{
	if (m == NULL || m->id_namelen == 0) {
		fprintf(stderr, "idmap_id: bad msg\n");
	  	return -1;
	}

	switch(m->id_type) {
	case IDMAP_TYPE_UID:
		m->id_id.uid = id.uid;
		break;
	case IDMAP_TYPE_GID:
		m->id_id.gid = id.gid;
		break;
	default:
		return -1;
		break;
	};

	return 0;
}

int
idmap_service(struct idmap_e * e)
{
  	struct idmap_msg * m;
	struct passwd * pwd;
	struct group * grp;
	ident_t id;
	char * name;

	if (e == NULL) {
	  	fprintf(stderr, "bad entry\n");
		return -1;
	}

	if (e->msg.msg_vers != NFS4DEV_VERSION) {
		fprintf(stderr, "kernel/userland version mismatch! %d/%d\n",
		    e->msg.msg_vers, NFS4DEV_VERSION);
		return -1;
	}

	if (e->msg.msg_type != NFS4DEV_TYPE_IDMAP) {
  		fprintf(stderr, "bad type!\n");
		return -1;
	}

	if (e->msg.msg_len != sizeof(struct idmap_msg)) {
		fprintf(stderr, "bad message length: %zu/%zu\n", e->msg.msg_len,
			sizeof(struct idmap_msg));
		return -1;
	}

	if (verbose)
		printf("servicing msg xid: %x\n", e->msg.msg_xid);


	m = (struct idmap_msg *)e->msg.msg_data;

	if (m->id_namelen != 0 && m->id_namelen != strlen(m->id_name)) {
		fprintf(stderr, "bad name length in idmap_msg\n");
		return -1;
	}

	switch (m->id_type) {
	case IDMAP_TYPE_UID:
		if (m->id_namelen == 0) {
			/* id to name */
		  	pwd = getpwuid(m->id_id.uid);

			if (pwd == NULL) {
				fprintf(stderr, "unknown uid %d!\n",
					(uint32_t)m->id_id.uid);
				name = BADUSER;
			} else
			  	name = pwd->pw_name;

			if (idmap_name(m, name))
				return -1;

		} else {
		  	/* name to id */
		  	name = idmap_prune_domain(m);
			if (name == NULL)
				return -1;

			pwd = getpwnam(name);

			if (pwd == NULL) {
				fprintf(stderr, "unknown username %s!\n", name);

				id.uid = (uid_t)BADUID;
			} else
			  	id.uid = pwd->pw_uid;

			free(name);

			if (idmap_id(m, id))
				return -1;
		}
		break;
	case IDMAP_TYPE_GID:
		if (m->id_namelen == 0) {
			/* id to name */
		  	grp = getgrgid(m->id_id.gid);

			if (grp == NULL) {
				fprintf(stderr, "unknown gid %d!\n",
					(uint32_t)m->id_id.gid);
				name = BADGROUP;
			} else
			  	name = grp->gr_name;

			if (idmap_name(m, name))
				return -1;
		} else {
		  	/* name to id */
		  	name = idmap_prune_domain(m);
			if (name == NULL)
				return -1;

			grp = getgrnam(name);

			if (grp == NULL) {
				fprintf(stderr, "unknown groupname %s!\n", name);

				id.gid = (gid_t)BADGID;
			} else
			  	id.gid = grp->gr_gid;

			free(name);

			if (idmap_id(m, id))
				return -1;
		}
		break;
	default:
		fprintf(stderr, "bad idmap type: %d\n", m->id_type);
		return -1;
		break;
	}

	return 0;
}

int
main(int argc, char ** argv)
{
	int error = 0;
	struct idmap_e * entry;
	fd_set read_fds, write_fds;
	int maxfd;
	int ret, ch;

	while ((ch = getopt(argc, argv, "d:v")) != -1) {
		switch (ch) {
		case 'd':
			domain = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr, "usage: %s [-v] [-d domain]\n", argv[0]);
			exit(1);
			break;
		}
	}


	TAILQ_INIT(&upcall_q);

	if (error) {
	  	perror("sigaction");
		exit(1);
	}


	fd = open(DEV_PATH, O_RDWR, S_IRUSR | S_IWUSR);

	if (fd < 0) {
		perror(DEV_PATH);
		exit(1);
	}

	if (!verbose)
		daemon(0,0);

	maxfd = fd;
	for (;;) {
	  	struct timeval timo = {1, 0};
		do {
			FD_ZERO(&read_fds);
			FD_ZERO(&write_fds);

			FD_SET(fd, &read_fds);
			FD_SET(fd, &write_fds);

			ret = select(maxfd+1, &read_fds, &write_fds, NULL, &timo);
		} while (ret < 0 && errno == EINTR);

		if (ret <= 0) {
			if (ret != 0)
				perror("select");
			continue;
		}


		if (FD_ISSET(fd, &read_fds)) {
			for (;;) {
				get_idmap_e(entry);

				error = ioctl(fd, NFS4DEVIOCGET, &entry->msg);

				if (error == -1) {
					if (errno != EAGAIN)
	  					perror("get ioctl:");
					put_idmap_e(entry);
					break;
				}

				switch (entry->msg.msg_type ) {
				case NFS4DEV_TYPE_IDMAP:
					if (idmap_service(entry))
						entry->msg.msg_error = EIO;
				break;
				default:
					fprintf(stderr, "unknown nfs4dev_msg type\n");
					entry->msg.msg_error = EIO;
				break;
				}

				add_idmap_e(entry);
			}
		}

		if (FD_ISSET(fd, &write_fds)) {
			while (!TAILQ_EMPTY(&upcall_q)) {
				remove_idmap_e(entry);

				error = ioctl(fd, NFS4DEVIOCPUT, &entry->msg);

				if (error == -1) {
				  	if (errno != EAGAIN)
	  					perror("put ioctl");
					break;
				}
				put_idmap_e(entry);
			}
		}
	}

	/* never reached */
	exit(1);
}
