/*
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * NEW command line interface for IP firewall facility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <kvm.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#define IPFIREWALL
#define IPACCT
#include <netinet/ip_fw.h>

#define MAXSTR	25

char 		progname[MAXSTR];		/* Program name for errors */
char		proto_name[MAXSTR]="";		/* Current line protocol   */
int 		s;				/* main RAW socket 	   */
int 		do_resolv=1;			/* Would try to resolv all */
int 		do_short=0;			/* Compact output          */
int		do_acct=0;			/* Show packet/byte count  */
int 		ports_ok=0;			/* flag allowing ports     */
u_short		flags=0;			/* New entry flags 	   */


#define FW	1	/* Firewall action   */
#define AC	2	/* Accounting action */


#define S_SEP1		"f" /* of "from" */
#define S_SEP2		"t" /* of "to"   */
#define S_SEP3		"v" /* of "via"  */

#define P_AC		"a" /* of "accept" for policy action */
#define P_DE		"d" /* of "deny" for policy action   */

#define CH_FW		"f" /* of "firewall" for chains in zero/flush       */
#define CH_AC		"a" /* of "accounting" for chain in zero/flush/list */

char	action_tab[][MAXSTR]={
"addf",
#define A_ADDF 		0
"delf",
#define A_DELF 		1
"chkf",
#define A_CHKF		2
"adda",
#define A_ADDA		3
"dela",
#define A_DELA		4
"clr",
#define A_CLRA		5
"f",
#define A_FLUSH		6
"z",
#define A_ZERO		7 
"l",
#define A_LIST		8 
"p",
#define A_POLICY	9 
"",
#define A_NONE		10
};


char	type_tab[][MAXSTR]={
"ac",
#define T_ACCEPT	0
"lo",
#define T_LOG		1
"r",
#define T_REJECT	2
"lr",
#define T_LREJECT	3
"d",
#define T_DENY		4
"ld",
#define T_LDENY		5
"si",
#define T_SINGLE	6
"bi",
#define T_BIDIR		7
"",
#define T_NONE		8
};


char	proto_tab[][MAXSTR]={
"all",
#define P_ALL		0
"icmp",
#define P_ICMP		1
"tcp",
#define P_TCP		2
"syn",
#define P_SYN		3
"udp",
#define P_UDP		4
""
#define P_NONE		5
};

struct nlist nlf[]={
#define N_FCHAIN 	0
	{ "_ip_fw_chain" },
#define N_POLICY 	1
	{ "_ip_fw_policy" },
	"" ,
};


struct nlist nla[]={
#define N_ACHAIN	0
	{ "_ip_acct_chain" },
	"" ,
};


int mask_bits(m_ad)
struct in_addr m_ad;
{
int h_fnd=0,h_num=0,i;
u_long mask;

	mask=ntohl(m_ad.s_addr);
	for (i=0;i<sizeof(u_long)*CHAR_BIT;i++) {
		if (mask & 1L) {
			h_fnd=1;
			h_num++;
		} else {
			if (h_fnd)
				return -1;
		}
	mask=mask>>1;
	} 
	return h_num;
}



void
show_ipfw(chain,c_t)
struct ip_fw *chain;
int	c_t;
{
char *comma;
u_long adrt;
struct hostent *he;
int i,mb;


if (do_short && do_acct) {
	printf("%8d:%8d ",chain->fw_bcnt,chain->fw_pcnt);
}
	

if (do_short)
	if (c_t==FW) {
		if (chain->fw_flg & IP_FW_F_ACCEPT) 
			if (chain->fw_flg & IP_FW_F_PRN)
				printf("l");
			else
				printf("a");
		else 
			if (chain->fw_flg & IP_FW_F_PRN)
				if (chain->fw_flg & IP_FW_F_ICMPRPL)
					printf("R");
				else
					printf("D");
			else
				if (chain->fw_flg & IP_FW_F_ICMPRPL)
					printf("r");
				else
					printf("d");
	} else {
		if (chain->fw_flg & IP_FW_F_BIDIR) 
			printf("b");
		else 
			printf("s");
	}
else
	if (c_t==FW) {
		if (chain->fw_flg & IP_FW_F_ACCEPT) 
			if (chain->fw_flg & IP_FW_F_PRN)
				printf("log ");
			else
				printf("accept ");
		else 
			if (chain->fw_flg & IP_FW_F_PRN)
				if (chain->fw_flg & IP_FW_F_ICMPRPL)
					printf("lreject ");
				else
					printf("ldeny ");
			else
				if (chain->fw_flg & IP_FW_F_ICMPRPL)
					printf("reject ");
				else
					printf("deny ");
	} else {
		if (chain->fw_flg & IP_FW_F_BIDIR) 
			printf("bidir  ");
		else 
			printf("single ");
	}

if (do_short)
	switch (chain->fw_flg & IP_FW_F_KIND) {
		case IP_FW_F_ICMP:
			printf("I ");
			break;
		case IP_FW_F_TCP:
			if (chain->fw_flg&IP_FW_F_TCPSYN)
				printf("S ");
			else
				printf("T ");
			break;
		case IP_FW_F_UDP:
			printf("U ");
			break;
		case IP_FW_F_ALL:
			printf("A ");
			break;
		default:
			break;
	}
else
	switch (chain->fw_flg & IP_FW_F_KIND) {
		case IP_FW_F_ICMP:
			printf("icmp ");
			break;
		case IP_FW_F_TCP:
			if (chain->fw_flg&IP_FW_F_TCPSYN)
				printf("syn  ");
			else
				printf("tcp  ");
			break;
		case IP_FW_F_UDP:
			printf("udp  ");
			break;
		case IP_FW_F_ALL:
			printf("all  ");
			break;
		default:
			break;
	}

if (do_short)
	printf("[");
else
	printf("from ");

	adrt=ntohl(chain->fw_smsk.s_addr);
	if (adrt==ULONG_MAX && do_resolv) {
		adrt=(chain->fw_src.s_addr);
		he=gethostbyaddr((char *)&adrt,sizeof(u_long),AF_INET);
		if (he==NULL) {
			printf(inet_ntoa(chain->fw_src));
			printf(":");
			printf(inet_ntoa(chain->fw_smsk));
		} else
			printf("%s",he->h_name);
	} else {
		printf(inet_ntoa(chain->fw_src));
		if (adrt!=ULONG_MAX)
			if ((mb=mask_bits(chain->fw_smsk))>=0)
				printf("/%d",mb);
			else {
				printf(":");
				printf(inet_ntoa(chain->fw_smsk));
			}
	}

	comma = " ";
	for (i=0;i<chain->fw_nsp; i++ ) {
		printf("%s%d",comma,chain->fw_pts[i]);
		if (i==0 && (chain->fw_flg & IP_FW_F_SRNG)) 
			comma = ":";
		else 
			comma = ",";
	}

if (do_short)
	printf("][");
else
	printf(" to ");

	adrt=ntohl(chain->fw_dmsk.s_addr);
	if (adrt==ULONG_MAX && do_resolv) {
		adrt=(chain->fw_dst.s_addr);
		he=gethostbyaddr((char *)&adrt,sizeof(u_long),AF_INET);
		if (he==NULL) {
			printf(inet_ntoa(chain->fw_dst));
			printf(":");
			printf(inet_ntoa(chain->fw_dmsk));
		} else
			printf("%s",he->h_name);
	} else {
		printf(inet_ntoa(chain->fw_dst));
		if (adrt!=ULONG_MAX) 
			if ((mb=mask_bits(chain->fw_dmsk))>=0)
				printf("/%d",mb);
			else {
				printf(":");
				printf(inet_ntoa(chain->fw_dmsk));
			}
	}

	comma = " ";
	for (i=0;i<chain->fw_ndp;i++) {
		printf("%s%d",comma,chain->fw_pts[chain->fw_nsp+i]);
		if (i==chain->fw_nsp && (chain->fw_flg & IP_FW_F_DRNG))
			comma = ":";
		else 
		    comma = ",";
	    }

if (chain->fw_via.s_addr) {
	if (do_short)
		printf("][");
	else
		printf(" via ");
	printf(inet_ntoa(chain->fw_via));
}
if (do_short)
	printf("]\n");
else
	printf("\n");
}


list(av)
char 	**av;
{
kvm_t *kd;
static char errb[_POSIX2_LINE_MAX];
struct ip_fw b,*btmp;

	if (!(kd=kvm_openfiles(NULL,NULL,NULL,O_RDONLY,errb))) {
     		fprintf(stderr,"%s: kvm_openfiles: %s\n",
					progname,kvm_geterr(kd));
     		exit(1);
	}

if (*av==NULL || !strncmp(*av,CH_FW,strlen(CH_FW))) {
	if (kvm_nlist(kd,nlf)<0 || nlf[0].n_type==0) {
		fprintf(stderr,"%s: kvm_nlist: no namelist in %s\n",
						progname,getbootfile());
      		exit(1);
    	}
}

if (*av==NULL || !strncmp(*av,CH_FW,strlen(CH_FW))) {
	kvm_read(kd,(u_long)nlf[N_FCHAIN].n_value,&b,sizeof(struct ip_fw));
	printf("FireWall chain entries:\n");
	while(b.fw_next!=NULL) {
		btmp=b.fw_next;
		kvm_read(kd,(u_long)btmp,&b,sizeof(struct ip_fw));
		show_ipfw(&b,FW);
	}
}


if (*av==NULL ||  !strncmp(*av,CH_AC,strlen(CH_AC))) {
	if (kvm_nlist(kd,nla)<0 || nla[0].n_type==0) {
		fprintf(stderr,"%s: kvm_nlist: no namelist in %s\n",
						progname,getbootfile());
      		exit(1);
    	}
}

if (*av==NULL || !strncmp(*av,CH_AC,strlen(CH_AC))) {
	kvm_read(kd,(u_long)nla[N_ACHAIN].n_value,&b,sizeof(struct ip_fw));
	printf("Accounting chain entries:\n");
	while(b.fw_next!=NULL) {
		btmp=b.fw_next;
		kvm_read(kd,(u_long)btmp,&b,sizeof(struct ip_fw));
		show_ipfw(&b,AC);
	}
}

}






int get_num(str,tab)
char 	*str;
char	tab[][MAXSTR];
{
int	i=0;
	while(tab[i][0]!='\0') {
		if (strlen(str)>=strlen(tab[i]))
			if (!strncmp(str,tab[i],strlen(tab[i])))
				return i;
		i++;
	}
return i;
}



void show_usage()
{
	printf("%s: bad arguments\n",progname);
}




u_short get_port(str)
char	*str;
{
struct servent *sptr;
char *end;
int port,slen = strlen(str);

	if ((slen>0) && (strspn(str,"0123456789")==slen)) {
		port = strtol(str,&end,10);
		if (*end!='\0') {
	    		fprintf(stderr,"%s: illegal port number :%s\n"
							,progname,str);
	    	exit(1);
		}

		if ((port<=0) || (port>USHRT_MAX)) {
			fprintf(stderr,"%s: port number out of range :%d\n"
							,progname,port);
	    		exit(1);
		}
		return((u_short)port);
    	} else {
		sptr = getservbyname(str,proto_name);
		if (!sptr) {
	    		fprintf(stderr,"%s: unknown service :%s\n"
							,progname,str);
	    		exit(1);
		}
		return((u_short)ntohs(sptr->s_port));
    	}
}


char *findchar(str,c)
char	*str;
char	c;
{
int i,len=strlen(str);

for (i=0;i<len;i++) {
	if (str[i]==c)
		return(char*)(&str[i]);
}
return NULL;
}


int set_entry_ports(str,ports,a_max,is_range)
char		*str;
u_short		*ports;
int		a_max;
int		*is_range;
{
char 	*s_pr2,*s_h,*s_t,*cp;
u_short	p1,p2; 
int i=0;

	(void)strtok(str,":");
	s_pr2=strtok(NULL,"");
	if (s_pr2) {
		p1 = get_port(str);
		p2 = get_port(s_pr2);
		if (a_max<2) {
			fprintf(stderr,"%s: too many ports.\n",progname);
			exit(1);
		}
		ports[0]=p1;
		ports[1]=p2;
		*is_range=1;
		return 2;
	}
	s_h=str;
	while ((cp=findchar(s_h,','))!=NULL) {
		if (i>a_max) {
			fprintf(stderr,"%s: too many ports.\n",progname);
			exit(1);
		}
		*cp='\0';
		if ((s_t=(++cp))=='\0') {
			fprintf(stderr,"%s: bad port list.\n",progname);
			exit(1);
		}
		ports[i++]=get_port(s_h);
		s_h=s_t;
	}
	if (i>a_max) {
		fprintf(stderr,"%s: too many ports.\n",progname);
		exit(1);
	}
	ports[i]=get_port(s_h);
	*is_range=0;
	return (i+1);
}



void set_entry_ip(str,addr,mask)
char 	*str;
struct in_addr  *addr,*mask;
{
char	*sm_bit,*sm_oct,*end;
int	n_bit;
struct	hostent *hptr;

	if (mask) {
		(void)strtok(str,"/");
		sm_bit=strtok(NULL,"");
		(void)strtok(str,":");
		sm_oct=strtok(NULL,"");
	}
	
	if (!inet_aton(str,addr)) {
		if (do_resolv) {
			if (!(hptr=gethostbyname(str))) {
				fprintf(stderr,"%s: Unknown host name : %s\n",
						progname,str);
				exit(1);
			} else {
				addr->s_addr=*((u_long *)hptr->h_addr);
			}
		} else {
			fprintf(stderr,"%s: Bad IP : %s\n",progname,str);
			exit(1);
		}
	}

		/*
		 * This is in case mask we 
		 * want to set IP only
		 */
	if (!mask)
		return;
	mask->s_addr=htonl(ULONG_MAX);

		if (sm_bit) {
			n_bit = strtol(sm_bit,&end,10);
            		if (*end!='\0') {
                		show_usage();
                		exit(1);
            		}
			if (n_bit<0 || n_bit>sizeof(u_long)*CHAR_BIT) {
				show_usage();
				exit(1);
			}
			if (n_bit>0)
		 	   mask->s_addr=
			     htonl(ULONG_MAX<<(sizeof(u_long)*CHAR_BIT-n_bit));
			else
			   mask->s_addr=0L;
		} 

		if (sm_oct) {
			if (!inet_aton(sm_oct,mask)) {
				show_usage();
				exit(1);
			}
		}
	/*
 	 * Ugh..better of corse do it in kernel so no error possible
	 * but faster here so this way it goes...
	 */

	addr->s_addr=mask->s_addr & addr->s_addr;
}


void set_entry(av,frwl) 
char 	**av;
struct ip_fw * frwl;
{
int p_num=0,ir=0;

	frwl->fw_nsp=0;
	frwl->fw_ndp=0;
	frwl->fw_via.s_addr=0L;

	if (strncmp(*av,S_SEP1,strlen(S_SEP1))) {
		show_usage();
		exit(1);
	}

	if (*(++av)==NULL) {
			show_usage();
			exit(1);
	}

	set_entry_ip(*av,&(frwl->fw_src),&(frwl->fw_smsk));

	if (*(++av)==NULL) {
			show_usage();
			exit(1);
	}

	if (!strncmp(*av,S_SEP2,strlen(S_SEP2))) 
		goto no_src_ports;

	if (ports_ok) {
		frwl->fw_nsp=
			set_entry_ports(*av,frwl->fw_pts,IP_FW_MAX_PORTS,&ir);
		if (ir)
			flags|=IP_FW_F_SRNG;

		if (*(++av)==NULL) {
				show_usage();
				exit(1);
		}
	}

no_src_ports:

	if (strncmp(*av,S_SEP2,strlen(S_SEP2))) {
		show_usage();
		exit(1);
	}

	if (*(++av)==NULL) {
			show_usage();
			exit(1);
	}
	
	set_entry_ip(*av,&(frwl->fw_dst),&(frwl->fw_dmsk));

	if (*(++av)==NULL) 
		goto no_tail;

	if (!strncmp(*av,S_SEP3,strlen(S_SEP3))) 
		goto no_dst_ports;

	if (ports_ok) {
		frwl->fw_ndp=
			set_entry_ports(*av,&(frwl->fw_pts[frwl->fw_nsp]),
					(IP_FW_MAX_PORTS-frwl->fw_nsp),&ir);
		if (ir)
			flags|=IP_FW_F_DRNG;
	}
no_dst_ports:
	if (strncmp(*av,S_SEP3,strlen(S_SEP3))) {
		show_usage();
		exit(1);
	}

	if (*(++av)==NULL) {
			show_usage();
			exit(1);
	}

	set_entry_ip(*av,&(frwl->fw_via),NULL);
no_tail:

}



flush(av)
char **av;
{
	if (*av==NULL) {
 		if (setsockopt(s,IPPROTO_IP,IP_FW_FLUSH,NULL,0)<0) {
			fprintf(stderr,"%s: setsockopt failed.\n",progname);
			exit(1);
		} else {
			printf("All firewall entries flushed.\n");
		}
 		if (setsockopt(s,IPPROTO_IP,IP_ACCT_FLUSH,NULL,0)<0) {
			fprintf(stderr,"%s: setsockopt failed.\n",progname);
			exit(1);
		} else {
			printf("All accounting entries flushed.\n");
		}
		exit(0);
	}
	if (!strncmp(*av,CH_FW,strlen(CH_FW))) {
 		if (setsockopt(s,IPPROTO_IP,IP_FW_FLUSH,NULL,0)<0) {
			fprintf(stderr,"%s: setsockopt failed.\n",progname);
			exit(1);
		} else {
			printf("All firewall entries flushed.\n");
			exit(0);
		}
	}
	if (!strncmp(*av,CH_AC,strlen(CH_AC))) {
 		if (setsockopt(s,IPPROTO_IP,IP_ACCT_FLUSH,NULL,0)<0) {
			fprintf(stderr,"%s: setsockopt failed.\n",progname);
			exit(1);
		} else {
			printf("All accounting entries flushed.\n");
			exit(0);
		}
	}

}



void policy(av)
char **av;
{
 u_short p=0,b;
 kvm_t *kd;
 static char errb[_POSIX2_LINE_MAX];

if (*av==NULL || strlen(*av)<=0) {
 if ( (kd=kvm_openfiles(NULL,NULL,NULL,O_RDONLY,errb)) == NULL) {
     fprintf(stderr,"%s: kvm_openfiles: %s\n",progname,kvm_geterr(kd));
     exit(1);
 }
 if (kvm_nlist(kd,nlf) < 0 || nlf[0].n_type == 0) {
      fprintf(stderr,"%s: kvm_nlist: no namelist in %s\n",
					progname,getbootfile()); 
      exit(1);
 }

kvm_read(kd,(u_long)nlf[N_POLICY].n_value,&b,sizeof(int));

if (b&IP_FW_P_DENY)
	printf("Default policy: DENY\n");
else
	printf("Default policy: ACCEPT\n");
exit(1);
}

if (!strncmp(*av,P_DE,strlen(P_DE)))
	p|=IP_FW_P_DENY;
else
if (!strncmp(*av,P_AC,strlen(P_AC)))
	p&=~IP_FW_P_DENY;
else {
	fprintf(stderr,"%s: bad policy value.\n",progname);
	exit(1);
}

if (setsockopt(s,IPPROTO_IP,IP_FW_POLICY,&p,sizeof(p))<0) {
	fprintf(stderr,"%s: setsockopt failed.\n",progname);
	exit(1);
} else {
	if (p&IP_FW_P_DENY)
		printf("Policy set to DENY.\n");
	else
		printf("Policy set to ACCEPT.\n");
	exit(0);
}
}



zero()
{
	if (setsockopt(s,IPPROTO_IP,IP_ACCT_ZERO,NULL,0)<0) {
		fprintf(stderr,"%s: setsockopt failed.\n",progname);
		exit(1);
	} else {
		printf("Accounting cleared.\n");
		exit(0);
	}
}

main(ac,av)
int 	ac;
char 	**av;
{

char 		ch;
extern int 	optind;
int 		ctl,int_t,is_check=0;
struct ip_fw	frwl;

	strcpy(progname,*av);

	s = socket( AF_INET, SOCK_RAW, IPPROTO_RAW );
	if ( s < 0 ) {
		fprintf(stderr,"%s: Can't open raw socket.Must be root to use this programm. \n",progname);	
		exit(1);
	}
	   if ( ac == 1 ) {
	show_usage();
	exit(1);
    }

	while ((ch = getopt(ac, av ,"ans")) != EOF)
	switch(ch) {
		case 'a':
			do_acct=1;
			break;
		case 'n':
	 		do_resolv=0;
        		break;
        	case 's':
		    	do_short=1;
            		break;
        	case '?':
         	default:
            		show_usage();
            		exit(1);                                      
	}

	if (*(av+=optind)==NULL) {
		 show_usage();
         	 exit(1);
	}

    switch(get_num(*av,action_tab)) {
			case A_ADDF:
				ctl=IP_FW_ADD;
				int_t=FW;
				break;
			case A_DELF:
				ctl=IP_FW_DEL;
				int_t=FW;
				break;
			case A_CHKF:
				int_t=FW;
				is_check=1;
				break;
			case A_ADDA:
				ctl=IP_ACCT_ADD;
				int_t=AC;
				break;
			case A_DELA:
				ctl=IP_ACCT_DEL;
				int_t=AC;
				break;
			case A_CLRA:
				ctl=IP_ACCT_CLR;
				int_t=AC;
				break;
			case A_FLUSH:
				flush(++av); 
				exit(0); /* successful exit */
			case A_LIST:
				list(++av); 
				exit(0); /* successful exit */
			case A_ZERO:
				zero(); 
				exit(0); /* successful exit */
			case A_POLICY:
				policy(++av);
				exit(0); /* we never get here */
			default:
				show_usage();
				exit(1);
	} /*  main action switch  */

	if (is_check)
		goto proto_switch;
	
	if (*(++av)==NULL) {
			show_usage();
			exit(1);
	}

	switch(get_num(*av,type_tab)) {
			case T_LREJECT:
				flags|=IP_FW_F_PRN;
			case T_REJECT:
				flags|=IP_FW_F_ICMPRPL;
				if (int_t!=FW) {
					show_usage();
					exit(1);
				}
				break;
			case T_LDENY:
				flags|=IP_FW_F_PRN;
			case T_DENY:
				flags|=0; /* just to show it related to flags */
				if (int_t!=FW) {
					show_usage();
					exit(1);
				}
				break;
			case T_LOG:
				flags|=IP_FW_F_PRN;
			case T_ACCEPT:
				flags|=IP_FW_F_ACCEPT;
				if (int_t!=FW) {
					show_usage();
					exit(1);
				}
				break;
			case T_SINGLE:
				flags|=0; /* just to show it related to flags */
				if (int_t!=AC) {
					show_usage();
					exit(1);
				}
				break;
			case T_BIDIR:
				flags|=IP_FW_F_BIDIR;
				if (int_t!=AC) {
					show_usage();
					exit(1);
				}
				break;
			default:
				show_usage();
				exit(1);

	} /* type of switch */

proto_switch:

	if (*(++av)==NULL) {
			show_usage();
			exit(1);
	}

	switch(get_num(*av,proto_tab)) {
		case P_ALL:
			flags|=IP_FW_F_ALL;
			break;
		case P_ICMP:
			flags|=IP_FW_F_ICMP;
			break;
		case P_SYN:
			flags|=IP_FW_F_TCPSYN;
		case P_TCP:
			flags|=IP_FW_F_TCP;
			ports_ok=1;
			strcpy(proto_name,"tcp");
			break;
		case P_UDP:
			flags|=IP_FW_F_UDP;
			ports_ok=1;
			strcpy(proto_name,"udp");
			break;
		default:
			show_usage();
			exit(1);
	}

	if (*(++av)==NULL) {
			show_usage();
			exit(1);
	}

	set_entry(av,&frwl); 
	frwl.fw_flg=flags;

	if (is_check) {
#ifndef disabled
		fprintf(stderr,"%s: checking disabled.\n",progname);
#else
		
		struct ip 		*pkt;
		struct tcphdr 		*th;
		int p_len=sizeof(struct ip)+sizeof(struct tcphdr);

		pkt=(struct ip*)malloc(p_len);
		pkt->ip_v = IPVERSION;
		pkt->ip_hl = sizeof(struct ip)/sizeof(int);

		th=(struct tcphdr *)(pkt+1);

		switch(get_num(proto_name,proto_tab)) {
			case P_TCP:
				pkt->ip_p = IPPROTO_TCP;
				break;
			case P_UDP:
				pkt->ip_p = IPPROTO_UDP;
				break;
			default:
				fprintf(stderr,"%s: can check TCP/UDP packets\
							only.\n",progname);
				exit(1);
		}
		if (frwl.fw_nsp!=1 || frwl.fw_ndp!=1) {
			fprintf(stderr,"%s: check needs one src/dst port.\n",
							progname);
			exit(1);
		}
		if (ntohl(frwl.fw_smsk.s_addr)!=ULONG_MAX ||
		    ntohl(frwl.fw_dmsk.s_addr)!=ULONG_MAX) {
			fprintf(stderr,"%s: can't check masked IP.\n",progname);
			exit(1);
		}
		pkt->ip_src.s_addr=frwl.fw_src.s_addr;
		pkt->ip_dst.s_addr=frwl.fw_dst.s_addr;
	
		th->th_sport=htons(frwl.fw_pts[0]);
		th->th_dport=htons(frwl.fw_pts[frwl.fw_nsp]);
		
		if (setsockopt(s,IPPROTO_IP,ctl,pkt,p_len))
			printf("Packet DENYED.\n");
		else
			printf("Packet ACCEPTED.\n");
		exit(0);
#endif
	} else {
		if (setsockopt(s,IPPROTO_IP,ctl,&frwl,sizeof(frwl))<0) {
			fprintf(stderr,"%s: setsockopt failed.\n",progname);
			exit(1);
		}
	}

    close(s);
}


