#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <irp.h>
#include <grp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <irp.h>
#include <irs.h>
#include <isc/irpmarshall.h>

void print_passwd(const char *name, struct passwd *pw, FILE *fp);
void print_group(const char *name, struct group *gr, FILE *fp);
void print_servent(const char *name, struct servent *sv, FILE *fp);
void print_host(const char *name, struct hostent *ho, FILE *fp);
void print_netent(const char *name, struct netent *ne, FILE *fp);
void print_proto(const char *name, struct protoent *pr, FILE *fp);

int
main(int argc, char **argv) {
	struct passwd *pw;
	struct group *gr;
	struct servent *sv;
	struct hostent *ho;
	struct netent *ne;
	struct protoent *pr;
	int ch ;
	char *groupname = NULL;
	
	while ((ch = getopt(argc, argv, "u:s:p:g:h:n:g:a:z:")) != -1) {
		switch (ch) {
		case 'u':
			if (strlen (optarg) == 0) {
				do {
					pw = getpwent();
					print_passwd(optarg, pw, stdout);
					printf("\n\n");
				} while (pw != NULL);
				sleep(1);
				setpwent();
				do {
					pw = getpwent();
					print_passwd(optarg, pw, stdout);
					printf("\n\n");
				} while (pw != NULL);
				sleep(1);
			} else {
				if (strspn(optarg, "0123456789") == 
				    strlen(optarg))
					pw = getpwuid(atoi(optarg));
				else
					pw = getpwnam(optarg);
				print_passwd(optarg, pw, stdout);
			}
			
			break;

		case 'g':
			if (strlen (optarg) == 0) {
				do {
					gr = getgrent();
					print_group(optarg, gr, stdout);
					printf("\n\n");
				} while (gr != NULL);
				sleep(1);
				setgrent();
				do {
					gr = getgrent();
					print_group(optarg, gr, stdout);
					printf("\n\n");
				} while (gr != NULL);
				sleep(1);
			} else {
				if (strspn(optarg, "0123456789") == 
				    strlen(optarg))
					gr = getgrgid(atoi(optarg));
				else
					gr = getgrnam(optarg);
				print_group(optarg, gr, stdout);
			}
			break;

		case 's':
			if (strlen (optarg) == 0) {
				do {
					sv = getservent();
					print_servent(optarg, sv, stdout);
					printf("\n\n");
				} while (sv != NULL);
				sleep(1);
				setservent(1);
				do {
					sv = getservent();
					print_servent(optarg, sv, stdout);
					printf("\n\n");
				} while (sv != NULL);
				sleep(1);
			} else {
				if (strspn(optarg, "0123456789") == 
				    strlen(optarg))
					sv = getservbyport(htons(atoi(optarg)),
							   "tcp");
				else
					sv = getservbyname(optarg,"tcp");
				print_servent(optarg, sv, stdout);
			}
			break;

		case 'h':
			if (strlen (optarg) == 0) {
				do {
					ho = gethostent();
					print_host(optarg, ho, stdout);
					printf("\n\n");
				} while (ho != NULL);
				sleep(1);
				sethostent(1);
				do {
					ho = gethostent();
					print_host(optarg, ho, stdout);
					printf("\n\n");
				} while (ho != NULL);
				sleep(1);
			} else {
				if (strspn(optarg, "0123456789.") == 
				    strlen(optarg)) {
					long naddr;
					inet_pton(AF_INET, optarg, &naddr);
					ho = gethostbyaddr((const char *) 
							   &naddr, 
							   sizeof naddr,
							   AF_INET);
				} else
					ho = gethostbyname(optarg);
				print_host(optarg, ho, stdout);
			}
			break;

		case 'n':
			if (strlen (optarg) == 0) {
				do {
					ne = getnetent();
					print_netent(optarg, ne, stdout);
					printf("\n\n");
				} while (ne != NULL);
				sleep(1);
				setnetent(1);
				do {
					ne = getnetent();
					print_netent(optarg, ne, stdout);
					printf("\n\n");
				} while (ne != NULL);
				sleep(1);
			} else {
				if (strspn(optarg, "0123456789./") == 
				    strlen(optarg)) {
					long naddr;
					inet_pton(AF_INET, optarg, &naddr);
					ne = getnetbyaddr(naddr, AF_INET);
				} else
					ne = getnetbyname(optarg);
				print_netent(optarg, ne, stdout);
			}
			break;
			
		case 'p':
			if (strlen (optarg) == 0) {
				do {
					pr = getprotoent();
					print_proto(optarg, pr, stdout);
					printf("\n\n");
				} while (pr != NULL);
				sleep(1);
				setprotoent(1);
				do {
					pr = getprotoent();
					print_proto(optarg, pr, stdout);
					printf("\n\n");
				} while (pr != NULL);
				sleep(1);
			} else {
				if (strspn(optarg, "0123456789") == 
				    strlen(optarg))
					pr = getprotobynumber(atoi(optarg));
				else
					pr = getprotobyname(optarg);
				print_proto(optarg, pr, stdout);
			}
			
			break;

		case 'z': {
			char *host, *user, *domain ;

			groupname = optarg;
			setnetgrent(groupname);
			while (getnetgrent(&host,&user,&domain) == 1) {
				fprintf(stdout, "++++\n");
				fprintf(stdout, "Host: \"%s\"\n",
				       (host == NULL ? "(null)" : host));
				fprintf(stdout, "User: \"%s\"\n",
				       (user == NULL ? "(null)" : user));
				fprintf(stdout, "Domain: \"%s\"\n",
				       (domain == NULL ? "(null)" : domain));
				fprintf(stdout, "----\n\n");
			}
			break;
		}
		
		default:
			printf("Huh?\n");
			exit (1);
		}
	}
	return (0);
}

void
print_passwd(const char *name, struct passwd *pw, FILE *fp) {
	if (pw == NULL) {
		fprintf(fp, "%s -- NONE\n",name) ;
		return ;
	}
	
	fprintf(fp, "++++\n");
	fprintf(fp, "Name: \"%s\"\n", pw->pw_name);
	fprintf(fp, "Uid: %d\n", pw->pw_uid);
	fprintf(fp, "Gid: %d\n", pw->pw_gid);
	fprintf(fp, "Password: \"%s\"\n", pw->pw_passwd);
	fprintf(fp, "Change: %s", ctime(&pw->pw_change));
	fprintf(fp, "Class: \"%s\"\n", pw->pw_class);
	fprintf(fp, "Gecos: \"%s\"\n", pw->pw_gecos);
	fprintf(fp, "Dir: \"%s\"\n", pw->pw_dir);
	fprintf(fp, "Shell: \"%s\"\n", pw->pw_shell);
	fprintf(fp, "Expire: %s", ctime(&pw->pw_expire));
	fprintf(fp, "----\n");
}



void print_group(const char *name, struct group *gr, FILE *fp) {
	char **p ;

	if (gr == NULL) {
		fprintf(fp, "%s -- NONE\n", name);
		return;
	}

	fprintf(fp, "++++\n");
	fprintf(fp, "Name: \"%s\"\n", gr->gr_name);
	fprintf(fp, "Password: \"%s\"\n", gr->gr_passwd);
	fprintf(fp, "Gid: %d\n", gr->gr_gid);
	fprintf(fp, "Members:\n") ;
	for (p = gr->gr_mem ; p != NULL && *p != NULL ; p++) {
		fprintf(fp, "\t\t%s\n",*p);
	}
	fprintf(fp, "----\n");
}



void print_servent(const char *name, struct servent *sv, FILE *fp) {
	char **p ;

	if (sv == NULL) {
		fprintf(fp, "%s -- NONE\n", name);
		return;
	}

	fprintf(fp, "++++\n");
	fprintf(fp, "Name: \"%s\"\n", sv->s_name);
	fprintf(fp, "Aliases:\n") ;
	for (p = sv->s_aliases ; p != NULL && *p != NULL ; p++) {
		fprintf(fp, "\t\t%s\n",*p);
	}
	
	fprintf(fp, "Port: %d\n", ntohs((short)sv->s_port));
	fprintf(fp, "Protocol: \"%s\"\n", sv->s_proto);
	fprintf(fp, "----\n");
}


void print_host(const char *name, struct hostent *ho, FILE *fp) {
	char **p ;
	char addr[24];

	if (ho == NULL) {
		fprintf(fp, "%s -- NONE\n", name);
		return;
	}

	fprintf(fp, "++++\n");
	fprintf(fp, "Name: \"%s\"\n", ho->h_name);
	fprintf(fp, "Aliases:\n") ;
	for (p = ho->h_aliases ; p != NULL && *p != NULL ; p++) {
		fprintf(fp, "\t\t%s\n",*p);
	}
	

	fprintf(fp, "Address Type: %s\n", ADDR_T_STR(ho->h_addrtype));
	fprintf(fp, "Addresses:\n");
	for (p = ho->h_addr_list ; p != NULL && *p ; p++) {
		addr[0] = '\0';
		inet_ntop(ho->h_addrtype, *p, addr, sizeof addr);
		fprintf(fp, "\t\t%s\n",addr);
	}
	fprintf(fp, "----\n");
}


void print_netent(const char *name, struct netent *ne, FILE *fp) {
	char **p ;
	char addr[24];
	long taddr;

	if (ne == NULL) {
		fprintf(fp, "%s -- NONE\n", name);
		return;
	}

	fprintf(fp, "++++\n");
	fprintf(fp, "Name: \"%s\"\n", ne->n_name);
	fprintf(fp, "Aliases:\n") ;
	for (p = ne->n_aliases ; p != NULL && *p != NULL ; p++) {
		fprintf(fp, "\t\t%s\n",*p);
	}
	

	fprintf(fp, "Address Type: %s\n", ADDR_T_STR(ne->n_addrtype));
	taddr = htonl(ne->n_net);
	inet_ntop(ne->n_addrtype, &taddr, addr, sizeof addr);
	fprintf(fp, "Net number: %s\n", addr);
	fprintf(fp, "----\n");
}
	

void print_proto(const char *name, struct protoent *pr, FILE *fp) {
	char **p ;
	char addr[24];
	long taddr;

	if (pr == NULL) {
		fprintf(fp, "%s -- NONE\n", name);
		return;
	}

	fprintf(fp, "++++\n");
	fprintf(fp, "Name: \"%s\"\n", pr->p_name);
	fprintf(fp, "Aliases:\n") ;
	for (p = pr->p_aliases ; p != NULL && *p != NULL ; p++) {
		fprintf(fp, "\t\t%s\n",*p);
	}
	

	fprintf(fp, "Protocol Number: %d\n", pr->p_proto);
	fprintf(fp, "----\n");
}
	
/*
   Local Variables:
   compile-command: "gcc -g -I../../include -L.. -o testirpd testirpd.c ../libbind.a"
   End:
*/

