/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - config file processing
 *	-----------------------------------
 *
 *	$Id: rc_config.c,v 1.60 2000/10/09 11:17:07 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Oct  6 10:08:09 2000]
 *
 *---------------------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/callout.h>
#include <sys/ioctl.h>

#include <net/if.h>
#ifdef __NetBSD__
#include <net/if_sppp.h>
#else
#if __FreeBSD__ == 3
#include <sys/param.h>
#include <sys/mbuf.h>
#endif
#include <net/if_var.h>
#include <machine/i4b_isppp.h>
#endif

#include "isdnd.h"
#include "y.tab.h"

#include "monitor.h"

extern int entrycount;
extern int controllercount;
extern int lineno;
extern char *yytext;

extern FILE *yyin;
extern int yyparse();

static void set_config_defaults(void);
static void check_config(void);
static void print_config(void);
static void parse_valid(int entrycount, char *dt);

static int nregexpr = 0;
static int nregprog = 0;

/*---------------------------------------------------------------------------*
 *	called from main to read and process config file
 *---------------------------------------------------------------------------*/
void
configure(char *filename, int reread)
{
	extern void reset_scanner(FILE *inputfile);
	
	set_config_defaults();

	yyin = fopen(filename, "r");

	if(reread)
	{
		reset_scanner(yyin);
	}
	
	if (yyin == NULL)
	{
		log(LL_ERR, "cannot fopen file [%s]", filename);
		exit(1);
	}

	yyparse();
	
	monitor_fixup_rights();

	check_config();		/* validation and consistency check */

	fclose(yyin);

	if(do_print)
	{
		if(config_error_flag)
		{
			log(LL_ERR, "there were %d error(s) in the configuration file, terminating!", config_error_flag);
			exit(1);
		}
		print_config();
		do_exit(0);
	}
}

/*---------------------------------------------------------------------------*
 *	yacc error routine
 *---------------------------------------------------------------------------*/
void
yyerror(const char *msg)
{
	log(LL_ERR, "configuration error: %s at line %d, token \"%s\"", msg, lineno+1, yytext);
	config_error_flag++;
}

/*---------------------------------------------------------------------------*
 *	fill all config entries with default values
 *---------------------------------------------------------------------------*/
static void
set_config_defaults(void)
{
	cfg_entry_t *cep = &cfg_entry_tab[0];	/* ptr to config entry */
	int i;

	/* system section cleanup */
	
	nregprog = nregexpr = 0;

	rt_prio = RTPRIO_NOTUSED;

	mailer[0] = '\0';
	mailto[0] = '\0';	
	
	/* clean regular expression table */
	
	for(i=0; i < MAX_RE; i++)
	{
		if(rarr[i].re_expr)
			free(rarr[i].re_expr);
		rarr[i].re_expr = NULL;
		
		if(rarr[i].re_prog)
			free(rarr[i].re_prog);
		rarr[i].re_prog = NULL;

		rarr[i].re_flg = 0;
	}

	strcpy(rotatesuffix, "");
	
	/*
	 * controller table cleanup, beware: has already
	 * been setup in main, init_controller() !
	 */
	
	for(i=0; i < ncontroller; i++)
	{
		isdn_ctrl_tab[i].protocol = PROTOCOL_DSS1;
	}

	/* entry section cleanup */
	
	for(i=0; i < CFG_ENTRY_MAX; i++, cep++)
	{
		bzero(cep, sizeof(cfg_entry_t));

		/* ====== filled in at startup configuration, then static */

		sprintf(cep->name, "ENTRY%d", i);	

		cep->isdncontroller = INVALID;
		cep->isdnchannel = CHAN_ANY;

		cep->usrdevicename = INVALID;
		cep->usrdeviceunit = INVALID;
		
		cep->remote_numbers_handling = RNH_LAST;

		cep->dialin_reaction = REACT_IGNORE;

		cep->b1protocol = BPROT_NONE;

		cep->unitlength = UNITLENGTH_DEFAULT;

		cep->earlyhangup = EARLYHANGUP_DEFAULT;
		
		cep->ratetype = INVALID_RATE;
		
	 	cep->unitlengthsrc = ULSRC_NONE;

		cep->answerprog = ANSWERPROG_DEF;	 	

		cep->callbackwait = CALLBACKWAIT_MIN;

		cep->calledbackwait = CALLEDBACKWAIT_MIN;		

		cep->dialretries = DIALRETRIES_DEF;

		cep->recoverytime = RECOVERYTIME_MIN;
	
		cep->dialouttype = DIALOUT_NORMAL;
		
		cep->inout = DIR_INOUT;
		
		cep->ppp_expect_auth = AUTH_UNDEF;
		
		cep->ppp_send_auth = AUTH_UNDEF;
		
		cep->ppp_auth_flags = AUTH_RECHALLENGE | AUTH_REQUIRED;
		
		/* ======== filled in after start, then dynamic */

		cep->cdid = CDID_UNUSED;

		cep->state = ST_IDLE;

		cep->aoc_valid = AOC_INVALID;
 	}
}

/*---------------------------------------------------------------------------*
 *	internaly set values for ommitted controler sectin
 *---------------------------------------------------------------------------*/
void
cfg_set_controller_default()
{
	controllercount = 0;
	DBGL(DL_RCCF, (log(LL_DBG, "[defaults, no controller section] controller %d: protocol = dss1", controllercount)));
	isdn_ctrl_tab[controllercount].protocol = PROTOCOL_DSS1;
}

#define PPP_PAP		0xc023
#define PPP_CHAP	0xc223

static void
set_isppp_auth(int entry)
{
	cfg_entry_t *cep = &cfg_entry_tab[entry];	/* ptr to config entry */

	struct ifreq ifr;
	struct spppreq spr;
	int s;
	int doioctl = 0;

	if(cep->usrdevicename != BDRV_ISPPP)
		return;

	if(cep->ppp_expect_auth == AUTH_UNDEF 
	   && cep->ppp_send_auth == AUTH_UNDEF)
		return;

	if(cep->ppp_expect_auth == AUTH_NONE 
	   || cep->ppp_send_auth == AUTH_NONE)
		doioctl = 1;

	if ((cep->ppp_expect_auth == AUTH_CHAP 
	     || cep->ppp_expect_auth == AUTH_PAP)
	    && cep->ppp_expect_name[0] != 0
	    && cep->ppp_expect_password[0] != 0)
		doioctl = 1;

	if ((cep->ppp_send_auth == AUTH_CHAP || cep->ppp_send_auth == AUTH_PAP)
			&& cep->ppp_send_name[0] != 0
			&& cep->ppp_send_password[0] != 0)
		doioctl = 1;

	if(!doioctl)
		return;

	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "isp%d", cep->usrdeviceunit);

	/* use a random AF to create the socket */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		log(LL_ERR, "ERROR opening control socket at line %d!", lineno);
		config_error_flag++;
		return;
	}
	spr.cmd = (int)SPPPIOGDEFS;
	ifr.ifr_data = (caddr_t)&spr;

	if (ioctl(s, SIOCGIFGENERIC, &ifr) == -1) {
		log(LL_ERR, "ERROR fetching active PPP authentication info for %s at line %d!", ifr.ifr_name, lineno);
		close(s);
		config_error_flag++;
		return;
	}
	if (cep->ppp_expect_auth != AUTH_UNDEF)
	{
		if(cep->ppp_expect_auth == AUTH_NONE)
		{
			spr.defs.myauth.proto = 0;
		}
		else if ((cep->ppp_expect_auth == AUTH_CHAP 
			  || cep->ppp_expect_auth == AUTH_PAP)
			 && cep->ppp_expect_name[0] != 0
			 && cep->ppp_expect_password[0] != 0)
		{
			spr.defs.myauth.proto = cep->ppp_expect_auth == AUTH_PAP ? PPP_PAP : PPP_CHAP;
			strncpy(spr.defs.myauth.name, cep->ppp_expect_name, AUTHNAMELEN);
			strncpy(spr.defs.myauth.secret, cep->ppp_expect_password, AUTHKEYLEN);
		}
	}
	if (cep->ppp_send_auth != AUTH_UNDEF)
	{
		if(cep->ppp_send_auth == AUTH_NONE)
		{
			spr.defs.hisauth.proto = 0;
		}
		else if ((cep->ppp_send_auth == AUTH_CHAP 
			  || cep->ppp_send_auth == AUTH_PAP)
			 && cep->ppp_send_name[0] != 0
			 && cep->ppp_send_password[0] != 0)
		{
			spr.defs.hisauth.proto = cep->ppp_send_auth == AUTH_PAP ? PPP_PAP : PPP_CHAP;
			strncpy(spr.defs.hisauth.name, cep->ppp_send_name, AUTHNAMELEN);
			strncpy(spr.defs.hisauth.secret, cep->ppp_send_password, AUTHKEYLEN);

			if(cep->ppp_auth_flags & AUTH_REQUIRED)
				spr.defs.hisauth.flags &= ~AUTHFLAG_NOCALLOUT;
			else
				spr.defs.hisauth.flags |= AUTHFLAG_NOCALLOUT;

			if(cep->ppp_auth_flags & AUTH_RECHALLENGE)
				spr.defs.hisauth.flags &= ~AUTHFLAG_NORECHALLENGE;
			else
				spr.defs.hisauth.flags |= AUTHFLAG_NORECHALLENGE;
		}
	}

	spr.cmd = (int)SPPPIOSDEFS;

	if (ioctl(s, SIOCSIFGENERIC, &ifr) == -1) {
		log(LL_ERR, "ERROR setting new PPP authentication parameters for %s at line %d!", ifr.ifr_name, lineno);
		config_error_flag++;
	}
	close(s);
}

/*---------------------------------------------------------------------------*
 *	extract values from config and fill table
 *---------------------------------------------------------------------------*/
void
cfg_setval(int keyword)
{
	int i;
	
	switch(keyword)
	{
		case ACCTALL:
			acct_all = yylval.booln;
			DBGL(DL_RCCF, (log(LL_DBG, "system: acctall = %d", yylval.booln)));
			break;
			
		case ACCTFILE:
			strcpy(acctfile, yylval.str);
			DBGL(DL_RCCF, (log(LL_DBG, "system: acctfile = %s", yylval.str)));
			break;

		case ALERT:
			if(yylval.num < MINALERT)
			{
				yylval.num = MINALERT;
				DBGL(DL_RCCF, (log(LL_DBG, "entry %d: alert < %d, min = %d", entrycount, MINALERT, yylval.num)));
			}
			else if(yylval.num > MAXALERT)
			{
				yylval.num = MAXALERT;
				DBGL(DL_RCCF, (log(LL_DBG, "entry %d: alert > %d, min = %d", entrycount, MAXALERT, yylval.num)));
			}
				
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: alert = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].alert = yylval.num;
			break;

		case ALIASING:
			DBGL(DL_RCCF, (log(LL_DBG, "system: aliasing = %d", yylval.booln)));
			aliasing = yylval.booln;
			break;

		case ALIASFNAME:
			strcpy(aliasfile, yylval.str);
			DBGL(DL_RCCF, (log(LL_DBG, "system: aliasfile = %s", yylval.str)));
			break;

		case ANSWERPROG:
			if((cfg_entry_tab[entrycount].answerprog = malloc(strlen(yylval.str)+1)) == NULL)
			{
				log(LL_ERR, "entry %d: answerstring, malloc failed!", entrycount);
				do_exit(1);
			}
			strcpy(cfg_entry_tab[entrycount].answerprog, yylval.str);
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: answerprog = %s", entrycount, yylval.str)));
			break;
			
		case B1PROTOCOL:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: b1protocol = %s", entrycount, yylval.str)));
			if(!(strcmp(yylval.str, "raw")))
				cfg_entry_tab[entrycount].b1protocol = BPROT_NONE;
			else if(!(strcmp(yylval.str, "hdlc")))
				cfg_entry_tab[entrycount].b1protocol = BPROT_RHDLC;
			else
			{
				log(LL_ERR, "ERROR parsing config file: unknown parameter for keyword \"b1protocol\" at line %d!", lineno);
				config_error_flag++;
			}
			break;

		case BEEPCONNECT:
			do_bell = yylval.booln;
			DBGL(DL_RCCF, (log(LL_DBG, "system: beepconnect = %d", yylval.booln)));
			break;

		case BUDGETCALLBACKPERIOD:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: budget-callbackperiod = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].budget_callbackperiod = yylval.num;
			break;

		case BUDGETCALLBACKNCALLS:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: budget-callbackncalls = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].budget_callbackncalls = yylval.num;
			break;
			
		case BUDGETCALLOUTPERIOD:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: budget-calloutperiod = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].budget_calloutperiod = yylval.num;
			break;

		case BUDGETCALLOUTNCALLS:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: budget-calloutncalls = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].budget_calloutncalls = yylval.num;
			break;

		case BUDGETCALLBACKSFILEROTATE:
			cfg_entry_tab[entrycount].budget_callbacksfile_rotate = yylval.booln;
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: budget-callbacksfile-rotate = %d", entrycount, yylval.booln)));
			break;
			
		case BUDGETCALLBACKSFILE:
			{
				FILE *fp;
				int s, l;
				int n;
				DBGL(DL_RCCF, (log(LL_DBG, "entry %d: budget-callbacksfile = %s", yylval.str)));
				fp = fopen(yylval.str, "r");
				if(fp != NULL)
				{
					if((fscanf(fp, "%d %d %d", (int *)&s, (int *)&l, &n)) != 3)
					{
						DBGL(DL_RCCF, (log(LL_DBG, "entry %d: initializing budget-callbacksfile %s", entrycount, yylval.str)));
						fclose(fp);
						fp = fopen(yylval.str, "w");
						if(fp != NULL)
							fprintf(fp, "%d %d %d", (int)time(NULL), (int)time(NULL), 0);
						fclose(fp);
					}
				}
				else
				{
					DBGL(DL_RCCF, (log(LL_DBG, "entry %d: creating budget-callbacksfile %s", entrycount, yylval.str)));
					fp = fopen(yylval.str, "w");
					if(fp != NULL)
						fprintf(fp, "%d %d %d", (int)time(NULL), (int)time(NULL), 0);
					fclose(fp);
				}

				fp = fopen(yylval.str, "r");
				if(fp != NULL)
				{
					if((fscanf(fp, "%d %d %d", (int *)&s, (int *)&l, &n)) == 3)
					{
						if((cfg_entry_tab[entrycount].budget_callbacks_file = malloc(strlen(yylval.str)+1)) == NULL)
						{
							log(LL_ERR, "entry %d: budget-callbacksfile, malloc failed!", entrycount);
							do_exit(1);
						}
						strcpy(cfg_entry_tab[entrycount].budget_callbacks_file, yylval.str);
						DBGL(DL_RCCF, (log(LL_DBG, "entry %d: using callbacksfile %s", entrycount, yylval.str)));
					}
					fclose(fp);
				}
			}
			break;

		case BUDGETCALLOUTSFILEROTATE:
			cfg_entry_tab[entrycount].budget_calloutsfile_rotate = yylval.booln;
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: budget-calloutsfile-rotate = %d", entrycount, yylval.booln)));
			break;

		case BUDGETCALLOUTSFILE:
			{
				FILE *fp;
				int s, l;
				int n;
				DBGL(DL_RCCF, (log(LL_DBG, "entry %d: budget-calloutsfile = %s", entrycount, yylval.str)));
				fp = fopen(yylval.str, "r");
				if(fp != NULL)
				{
					if((fscanf(fp, "%d %d %d", (int *)&s, (int *)&l, &n)) != 3)
					{
						DBGL(DL_RCCF, (log(LL_DBG, "entry %d: initializing budget-calloutsfile %s", entrycount, yylval.str)));
						fclose(fp);
						fp = fopen(yylval.str, "w");
						if(fp != NULL)
							fprintf(fp, "%d %d %d", (int)time(NULL), (int)time(NULL), 0);
						fclose(fp);
					}
				}
				else
				{
					DBGL(DL_RCCF, (log(LL_DBG, "entry %d: creating budget-calloutsfile %s", entrycount, yylval.str)));
					fp = fopen(yylval.str, "w");
					if(fp != NULL)
						fprintf(fp, "%d %d %d", (int)time(NULL), (int)time(NULL), 0);
					fclose(fp);
				}

				fp = fopen(yylval.str, "r");
				if(fp != NULL)
				{
					if((fscanf(fp, "%d %d %d", (int *)&s, (int *)&l, &n)) == 3)
					{
						if((cfg_entry_tab[entrycount].budget_callouts_file = malloc(strlen(yylval.str)+1)) == NULL)
						{
							log(LL_ERR, "entry %d: budget-calloutsfile, malloc failed!", entrycount);
							do_exit(1);
						}
						strcpy(cfg_entry_tab[entrycount].budget_callouts_file, yylval.str);
						DBGL(DL_RCCF, (log(LL_DBG, "entry %d: using calloutsfile %s", entrycount, yylval.str)));
					}
					fclose(fp);
				}
			}
			break;
		
		case CALLBACKWAIT:
			if(yylval.num < CALLBACKWAIT_MIN)
			{
				yylval.num = CALLBACKWAIT_MIN;
				DBGL(DL_RCCF, (log(LL_DBG, "entry %d: callbackwait < %d, min = %d", entrycount, CALLBACKWAIT_MIN, yylval.num)));
			}

			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: callbackwait = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].callbackwait = yylval.num;
			break;
			
		case CALLEDBACKWAIT:
			if(yylval.num < CALLEDBACKWAIT_MIN)
			{
				yylval.num = CALLEDBACKWAIT_MIN;
				DBGL(DL_RCCF, (log(LL_DBG, "entry %d: calledbackwait < %d, min = %d", entrycount, CALLEDBACKWAIT_MIN, yylval.num)));
			}

			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: calledbackwait = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].calledbackwait = yylval.num;
			break;

		case CONNECTPROG:
			if((cfg_entry_tab[entrycount].connectprog = malloc(strlen(yylval.str)+1)) == NULL)
			{
				log(LL_ERR, "entry %d: connectprog, malloc failed!", entrycount);
				do_exit(1);
			}
			strcpy(cfg_entry_tab[entrycount].connectprog, yylval.str);
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: connectprog = %s", entrycount, yylval.str)));
			break;
			
		case DIALOUTTYPE:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: dialouttype = %s", entrycount, yylval.str)));
			if(!(strcmp(yylval.str, "normal")))
				cfg_entry_tab[entrycount].dialouttype = DIALOUT_NORMAL;
			else if(!(strcmp(yylval.str, "calledback")))
				cfg_entry_tab[entrycount].dialouttype = DIALOUT_CALLEDBACK;
			else
			{
				log(LL_ERR, "ERROR parsing config file: unknown parameter for keyword \"dialout-type\" at line %d!", lineno);
				config_error_flag++;
			}
			break;

		case DIALRETRIES:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: dialretries = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].dialretries = yylval.num;
			break;

		case DIALRANDINCR:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: dialrandincr = %d", entrycount, yylval.booln)));
			cfg_entry_tab[entrycount].dialrandincr = yylval.booln;
			break;

		case DIRECTION:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: direction = %s", entrycount, yylval.str)));

			if(!(strcmp(yylval.str, "inout")))
				cfg_entry_tab[entrycount].inout = DIR_INOUT;
			else if(!(strcmp(yylval.str, "in")))
				cfg_entry_tab[entrycount].inout = DIR_INONLY;
			else if(!(strcmp(yylval.str, "out")))
				cfg_entry_tab[entrycount].inout = DIR_OUTONLY;
			else
			{
				log(LL_ERR, "ERROR parsing config file: unknown parameter for keyword \"direction\" at line %d!", lineno);
				config_error_flag++;
			}
			break;

		case DISCONNECTPROG:
			if((cfg_entry_tab[entrycount].disconnectprog = malloc(strlen(yylval.str)+1)) == NULL)
			{
				log(LL_ERR, "entry %d: disconnectprog, malloc failed!", entrycount);
				do_exit(1);
			}
			strcpy(cfg_entry_tab[entrycount].disconnectprog, yylval.str);
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: disconnectprog = %s", entrycount, yylval.str)));
			break;

		case DOWNTRIES:
			if(yylval.num > DOWN_TRIES_MAX)
				yylval.num = DOWN_TRIES_MAX;
			else if(yylval.num < DOWN_TRIES_MIN)
				yylval.num = DOWN_TRIES_MIN;
		
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: downtries = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].downtries = yylval.num;
			break;

		case DOWNTIME:
			if(yylval.num > DOWN_TIME_MAX)
				yylval.num = DOWN_TIME_MAX;
			else if(yylval.num < DOWN_TIME_MIN)
				yylval.num = DOWN_TIME_MIN;
		
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: downtime = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].downtime = yylval.num;
			break;

		case EARLYHANGUP:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: earlyhangup = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].earlyhangup = yylval.num;
			break;

		case EXTCALLATTR:
			DBGL(DL_RCCF, (log(LL_DBG, "system: extcallattr = %d", yylval.booln)));
			extcallattr = yylval.booln;
			break;

		case HOLIDAYFILE:
			strcpy(holidayfile, yylval.str);
			DBGL(DL_RCCF, (log(LL_DBG, "system: holidayfile = %s", yylval.str)));
			break;

		case IDLE_ALG_OUT:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: idle-algorithm-outgoing = %s", entrycount, yylval.str)));

			if(!(strcmp(yylval.str, "fix-unit-size")))
			{
				cfg_entry_tab[entrycount].shorthold_algorithm = SHA_FIXU;
			}
			else if(!(strcmp(yylval.str, "var-unit-size")))
			{
				cfg_entry_tab[entrycount].shorthold_algorithm = SHA_VARU;
			}
			else
			{
				log(LL_ERR, "ERROR parsing config file: unknown parameter for keyword \"idle-algorithm-outgoing\" at line %d!", lineno);
				config_error_flag++;
			}
			break;

		case IDLETIME_IN:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: idle_time_in = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].idle_time_in = yylval.num;
			break;
			
		case IDLETIME_OUT:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: idle_time_out = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].idle_time_out = yylval.num;
			break;

		case ISDNCONTROLLER:
			cfg_entry_tab[entrycount].isdncontroller = yylval.num;
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: isdncontroller = %d", entrycount, yylval.num)));
			break;

		case ISDNCHANNEL:
			switch(yylval.num)
			{
				case 0:
				case -1:
					cfg_entry_tab[entrycount].isdnchannel = CHAN_ANY;
					DBGL(DL_RCCF, (log(LL_DBG, "entry %d: isdnchannel = any", entrycount)));
					break;
				case 1:
					cfg_entry_tab[entrycount].isdnchannel = CHAN_B1;
					DBGL(DL_RCCF, (log(LL_DBG, "entry %d: isdnchannel = one", entrycount)));
					break;
				case 2:
					cfg_entry_tab[entrycount].isdnchannel = CHAN_B2;
					DBGL(DL_RCCF, (log(LL_DBG, "entry %d: isdnchannel = two", entrycount)));
					break;
				default:
					log(LL_DBG, "entry %d: isdnchannel value out of range", entrycount);
					config_error_flag++;
					break;
			}
			break;

		case ISDNTIME:
			DBGL(DL_RCCF, (log(LL_DBG, "system: isdntime = %d", yylval.booln)));
			isdntime = yylval.booln;
			break;

		case ISDNTXDELIN:
			cfg_entry_tab[entrycount].isdntxdelin = yylval.num;
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: isdntxdel-incoming = %d", entrycount, yylval.num)));
			break;

		case ISDNTXDELOUT:
			cfg_entry_tab[entrycount].isdntxdelout = yylval.num;
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: isdntxdel-outgoing = %d", entrycount, yylval.num)));
			break;

		case LOCAL_PHONE_DIALOUT:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: local_phone_dialout = %s", entrycount, yylval.str)));
			strcpy(cfg_entry_tab[entrycount].local_phone_dialout, yylval.str);
			break;

		case LOCAL_PHONE_INCOMING:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: local_phone_incoming = %s", entrycount, yylval.str)));
			strcpy(cfg_entry_tab[entrycount].local_phone_incoming, yylval.str);
			break;

		case MAILER:
			strcpy(mailer, yylval.str);
			DBGL(DL_RCCF, (log(LL_DBG, "system: mailer = %s", yylval.str)));
			break;

		case MAILTO:
			strcpy(mailto, yylval.str);
			DBGL(DL_RCCF, (log(LL_DBG, "system: mailto = %s", yylval.str)));
			break;

		case MONITORPORT:
			monitorport = yylval.num;
			DBGL(DL_RCCF, (log(LL_DBG, "system: monitorport = %d", yylval.num)));
			break;

		case MONITORSW:
			if (yylval.booln && inhibit_monitor)
			{
				do_monitor = 0;
				DBGL(DL_RCCF, (log(LL_DBG, "system: monitor-enable overriden by command line flag")));
			}
			else
			{
				do_monitor = yylval.booln;
				DBGL(DL_RCCF, (log(LL_DBG, "system: monitor-enable = %d", yylval.booln)));
			}
			break;

		case NAME:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: name = %s", entrycount, yylval.str)));
			strcpy(cfg_entry_tab[entrycount].name, yylval.str);
			break;

		case PPP_AUTH_RECHALLENGE:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: ppp-auth-rechallenge = %d", entrycount, yylval.booln)));
			if(yylval.booln)
				cfg_entry_tab[entrycount].ppp_auth_flags |= AUTH_RECHALLENGE;
			else
				cfg_entry_tab[entrycount].ppp_auth_flags &= ~AUTH_RECHALLENGE;
			set_isppp_auth(entrycount);
			break;

		case PPP_AUTH_PARANOID:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: ppp-auth-paranoid = %d", entrycount, yylval.booln)));
			if(yylval.booln)
				cfg_entry_tab[entrycount].ppp_auth_flags |= AUTH_REQUIRED;
			else
				cfg_entry_tab[entrycount].ppp_auth_flags &= ~AUTH_REQUIRED;
			set_isppp_auth(entrycount);
			break;

		case PPP_EXPECT_AUTH:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: ppp-expect-auth = %s", entrycount, yylval.str)));
			if(!(strcmp(yylval.str, "none")))
				cfg_entry_tab[entrycount].ppp_expect_auth = AUTH_NONE;
			else if(!(strcmp(yylval.str, "pap")))
				cfg_entry_tab[entrycount].ppp_expect_auth = AUTH_PAP;
			else if(!(strcmp(yylval.str, "chap")))
				cfg_entry_tab[entrycount].ppp_expect_auth = AUTH_CHAP;
			else
			{
				log(LL_ERR, "ERROR parsing config file: unknown parameter for keyword \"ppp-expect-auth\" at line %d!", lineno);
				config_error_flag++;
				break;
			}
			set_isppp_auth(entrycount);
			break;

		case PPP_EXPECT_NAME:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: ppp-expect-name = %s", entrycount, yylval.str)));
			strncpy(cfg_entry_tab[entrycount].ppp_expect_name, yylval.str, sizeof(cfg_entry_tab[entrycount].ppp_expect_name) -1);
			set_isppp_auth(entrycount);
			break;

		case PPP_EXPECT_PASSWORD:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: ppp-expect-password = %s", entrycount, yylval.str)));
			strncpy(cfg_entry_tab[entrycount].ppp_expect_password, yylval.str, sizeof(cfg_entry_tab[entrycount].ppp_expect_password) -1);
			set_isppp_auth(entrycount);
			break;

		case PPP_SEND_AUTH:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: ppp-send-auth = %s", entrycount, yylval.str)));
			if(!(strcmp(yylval.str, "none")))
				cfg_entry_tab[entrycount].ppp_send_auth = AUTH_NONE;
			else if(!(strcmp(yylval.str, "pap")))
				cfg_entry_tab[entrycount].ppp_send_auth = AUTH_PAP;
			else if(!(strcmp(yylval.str, "chap")))
				cfg_entry_tab[entrycount].ppp_send_auth = AUTH_CHAP;
			else
			{
				log(LL_ERR, "ERROR parsing config file: unknown parameter for keyword \"ppp-send-auth\" at line %d!", lineno);
				config_error_flag++;
				break;
			}
			set_isppp_auth(entrycount);
			break;

		case PPP_SEND_NAME:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: ppp-send-name = %s", entrycount, yylval.str)));
			strncpy(cfg_entry_tab[entrycount].ppp_send_name, yylval.str, sizeof(cfg_entry_tab[entrycount].ppp_send_name) -1);
			set_isppp_auth(entrycount);
			break;

		case PPP_SEND_PASSWORD:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: ppp-send-password = %s", entrycount, yylval.str)));
			strncpy(cfg_entry_tab[entrycount].ppp_send_password, yylval.str, sizeof(cfg_entry_tab[entrycount].ppp_send_password) -1);
			set_isppp_auth(entrycount);
			break;

		case PROTOCOL:
			DBGL(DL_RCCF, (log(LL_DBG, "controller %d: protocol = %s", controllercount, yylval.str)));
			if(!(strcmp(yylval.str, "dss1")))
				isdn_ctrl_tab[controllercount].protocol = PROTOCOL_DSS1;
			else if(!(strcmp(yylval.str, "d64s")))
				isdn_ctrl_tab[controllercount].protocol = PROTOCOL_D64S;
			else
			{
				log(LL_ERR, "ERROR parsing config file: unknown parameter for keyword \"protocol\" at line %d!", lineno);
				config_error_flag++;
			}
			break;

		case REACTION:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: dialin_reaction = %s", entrycount, yylval.str)));
			if(!(strcmp(yylval.str, "accept")))
				cfg_entry_tab[entrycount].dialin_reaction = REACT_ACCEPT;
			else if(!(strcmp(yylval.str, "reject")))
				cfg_entry_tab[entrycount].dialin_reaction = REACT_REJECT;
			else if(!(strcmp(yylval.str, "ignore")))
				cfg_entry_tab[entrycount].dialin_reaction = REACT_IGNORE;
			else if(!(strcmp(yylval.str, "answer")))
				cfg_entry_tab[entrycount].dialin_reaction = REACT_ANSWER;
			else if(!(strcmp(yylval.str, "callback")))
				cfg_entry_tab[entrycount].dialin_reaction = REACT_CALLBACK;
			else
			{
				log(LL_ERR, "ERROR parsing config file: unknown parameter for keyword \"dialin_reaction\" at line %d!", lineno);
				config_error_flag++;
			}
			break;

		case REMOTE_PHONE_DIALOUT:
			if(cfg_entry_tab[entrycount].remote_numbers_count >= MAXRNUMBERS)
			{
				log(LL_ERR, "ERROR parsing config file: too many remote numbers at line %d!", lineno);
				config_error_flag++;
				break;
			}				
			
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: remote_phone_dialout #%d = %s",
				entrycount, cfg_entry_tab[entrycount].remote_numbers_count, yylval.str)));

			strcpy(cfg_entry_tab[entrycount].remote_numbers[cfg_entry_tab[entrycount].remote_numbers_count].number, yylval.str);
			cfg_entry_tab[entrycount].remote_numbers[cfg_entry_tab[entrycount].remote_numbers_count].flag = 0;

			cfg_entry_tab[entrycount].remote_numbers_count++;
			
			break;

		case REMOTE_NUMBERS_HANDLING:			
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: remdial_handling = %s", entrycount, yylval.str)));
			if(!(strcmp(yylval.str, "next")))
				cfg_entry_tab[entrycount].remote_numbers_handling = RNH_NEXT;
			else if(!(strcmp(yylval.str, "last")))
				cfg_entry_tab[entrycount].remote_numbers_handling = RNH_LAST;
			else if(!(strcmp(yylval.str, "first")))
				cfg_entry_tab[entrycount].remote_numbers_handling = RNH_FIRST;
			else
			{
				log(LL_ERR, "ERROR parsing config file: unknown parameter for keyword \"remdial_handling\" at line %d!", lineno);
				config_error_flag++;
			}
			break;

		case REMOTE_PHONE_INCOMING:
			{
				int n;
				n = cfg_entry_tab[entrycount].incoming_numbers_count;
				if (n >= MAX_INCOMING)
				{
					log(LL_ERR, "ERROR parsing config file: too many \"remote_phone_incoming\" entries at line %d!", lineno);
					config_error_flag++;
					break;
				}
				DBGL(DL_RCCF, (log(LL_DBG, "entry %d: remote_phone_incoming #%d = %s", entrycount, n, yylval.str)));
				strcpy(cfg_entry_tab[entrycount].remote_phone_incoming[n].number, yylval.str);
				cfg_entry_tab[entrycount].incoming_numbers_count++;
			}
			break;

		case RATESFILE:
			strcpy(ratesfile, yylval.str);
			DBGL(DL_RCCF, (log(LL_DBG, "system: ratesfile = %s", yylval.str)));
			break;

		case RATETYPE:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: ratetype = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].ratetype = yylval.num;
			break;
		
		case RECOVERYTIME:
			if(yylval.num < RECOVERYTIME_MIN)
			{
				yylval.num = RECOVERYTIME_MIN;
				DBGL(DL_RCCF, (log(LL_DBG, "entry %d: recoverytime < %d, min = %d", entrycount, RECOVERYTIME_MIN, yylval.num)));
			}

			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: recoverytime = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].recoverytime = yylval.num;
			break;
		
		case REGEXPR:
			if(nregexpr >= MAX_RE)
			{
				log(LL_ERR, "system: regexpr #%d >= MAX_RE", nregexpr);
				config_error_flag++;
				break;
			}

			if((i = regcomp(&(rarr[nregexpr].re), yylval.str, REG_EXTENDED|REG_NOSUB)) != 0)
		        {
                		char buf[256];
                		regerror(i, &(rarr[nregexpr].re), buf, sizeof(buf));
				log(LL_ERR, "system: regcomp error for %s: [%s]", yylval.str, buf);
				config_error_flag++;
                		break;
			}
			else
			{
				if((rarr[nregexpr].re_expr = malloc(strlen(yylval.str)+1)) == NULL)
				{
					log(LL_ERR, "system: regexpr malloc error error for %s", yylval.str);
					config_error_flag++;
					break;
				}
				strcpy(rarr[nregexpr].re_expr, yylval.str);

				DBGL(DL_RCCF, (log(LL_DBG, "system: regexpr %s stored into slot %d", yylval.str, nregexpr)));
				
				if(rarr[nregexpr].re_prog != NULL)
					rarr[nregexpr].re_flg = 1;
				
				nregexpr++;
				
			}
			break;

		case REGPROG:
			if(nregprog >= MAX_RE)
			{
				log(LL_ERR, "system: regprog #%d >= MAX_RE", nregprog);
				config_error_flag++;
				break;
			}
			if((rarr[nregprog].re_prog = malloc(strlen(yylval.str)+1)) == NULL)
			{
				log(LL_ERR, "system: regprog malloc error error for %s", yylval.str);
				config_error_flag++;
				break;
			}
			strcpy(rarr[nregprog].re_prog, yylval.str);

			DBGL(DL_RCCF, (log(LL_DBG, "system: regprog %s stored into slot %d", yylval.str, nregprog)));
			
			if(rarr[nregprog].re_expr != NULL)
				rarr[nregprog].re_flg = 1;

			nregprog++;
			break;

		case ROTATESUFFIX:
			strcpy(rotatesuffix, yylval.str);
			DBGL(DL_RCCF, (log(LL_DBG, "system: rotatesuffix = %s", yylval.str)));
			break;

		case RTPRIO:
#ifdef USE_RTPRIO
			rt_prio = yylval.num;
			if(rt_prio < RTP_PRIO_MIN || rt_prio > RTP_PRIO_MAX)
			{
				config_error_flag++;
				log(LL_ERR, "system: error, rtprio (%d) out of range!", yylval.num);
			}
			else
			{
				DBGL(DL_RCCF, (log(LL_DBG, "system: rtprio = %d", yylval.num)));
			}
#else
			rt_prio = RTPRIO_NOTUSED;
#endif
			break;

		case TINAINITPROG:
			strcpy(tinainitprog, yylval.str);
			DBGL(DL_RCCF, (log(LL_DBG, "system: tinainitprog = %s", yylval.str)));
			break;

		case UNITLENGTH:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: unitlength = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].unitlength = yylval.num;
			break;

		case UNITLENGTHSRC:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: unitlengthsrc = %s", entrycount, yylval.str)));
			if(!(strcmp(yylval.str, "none")))
				cfg_entry_tab[entrycount].unitlengthsrc = ULSRC_NONE;
			else if(!(strcmp(yylval.str, "cmdl")))
				cfg_entry_tab[entrycount].unitlengthsrc = ULSRC_CMDL;
			else if(!(strcmp(yylval.str, "conf")))
				cfg_entry_tab[entrycount].unitlengthsrc = ULSRC_CONF;
			else if(!(strcmp(yylval.str, "rate")))
				cfg_entry_tab[entrycount].unitlengthsrc = ULSRC_RATE;
			else if(!(strcmp(yylval.str, "aocd")))
				cfg_entry_tab[entrycount].unitlengthsrc = ULSRC_DYN;
			else
			{
				log(LL_ERR, "ERROR parsing config file: unknown parameter for keyword \"unitlengthsrc\" at line %d!", lineno);
				config_error_flag++;
			}
			break;

		case USRDEVICENAME:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: usrdevicename = %s", entrycount, yylval.str)));
			if(!strcmp(yylval.str, "rbch"))
				cfg_entry_tab[entrycount].usrdevicename = BDRV_RBCH;
			else if(!strcmp(yylval.str, "tel"))
				cfg_entry_tab[entrycount].usrdevicename = BDRV_TEL;
			else if(!strcmp(yylval.str, "ipr"))
				cfg_entry_tab[entrycount].usrdevicename = BDRV_IPR;
			else if(!strcmp(yylval.str, "isp"))
				cfg_entry_tab[entrycount].usrdevicename = BDRV_ISPPP;
#ifdef __bsdi__
			else if(!strcmp(yylval.str, "ibc"))
				cfg_entry_tab[entrycount].usrdevicename = BDRV_IBC;
#endif
			else if(!strcmp(yylval.str, "ing"))
				cfg_entry_tab[entrycount].usrdevicename = BDRV_ING;
			else
			{
				log(LL_ERR, "ERROR parsing config file: unknown parameter for keyword \"usrdevicename\" at line %d!", lineno);
				config_error_flag++;
			}
			break;

		case USRDEVICEUNIT:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: usrdeviceunit = %d", entrycount, yylval.num)));
			cfg_entry_tab[entrycount].usrdeviceunit = yylval.num;
			break;

		case USEACCTFILE:
			useacctfile = yylval.booln;
			DBGL(DL_RCCF, (log(LL_DBG, "system: useacctfile = %d", yylval.booln)));
			break;

		case USEDOWN:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: usedown = %d", entrycount, yylval.booln)));
			cfg_entry_tab[entrycount].usedown = yylval.booln;
			break;

		case VALID:
			DBGL(DL_RCCF, (log(LL_DBG, "entry %d: valid = %s", entrycount, yylval.str)));
			parse_valid(entrycount, yylval.str);
			break;

		default:
			log(LL_ERR, "ERROR parsing config file: unknown keyword at line %d!", lineno);
			config_error_flag++;
			break;			
	}
}

/*---------------------------------------------------------------------------*
 *	parse a date/time range
 *---------------------------------------------------------------------------*/
static void
parse_valid(int entrycount, char *dt)
{
	/* a valid string consists of some days of week separated by
	 * commas, where 0=sunday, 1=monday .. 6=saturday and a special
	 * value of 7 which is a holiday from the holiday file.
	 * after the days comes an optional (!) time range in the form
	 * aa:bb-cc:dd, this format is fixed to be parsable by sscanf.
	 * Valid specifications looks like this:
	 * 1,2,3,4,5,09:00-18:00	Monday-Friday 9-18h
	 * 1,2,3,4,5,18:00-09:00	Monday-Friday 18-9h
	 * 6				Saturday (whole day)
	 * 0,7				Sunday and Holidays
	 */

	int day = 0;
	int fromhr = 0;
	int frommin = 0;
	int tohr = 0;
	int tomin = 0;
	int ret;
	
	for(;;)
	{
		if( ( ((*dt >= '0') && (*dt <= '9')) && (*(dt+1) == ':') ) ||
		    ( ((*dt >= '0') && (*dt <= '2')) && ((*(dt+1) >= '0') && (*(dt+1) <= '9')) && (*(dt+2) == ':') ) )
		{
			/* dt points to time spec */
			ret = sscanf(dt, "%d:%d-%d:%d", &fromhr, &frommin, &tohr, &tomin);
			if(ret !=4)
			{
				log(LL_ERR, "ERROR parsing config file: timespec [%s] error at line %d!", *dt, lineno);
				config_error_flag++;
				return;
			}

			if(fromhr < 0 || fromhr > 24 || tohr < 0 || tohr > 24 ||
			   frommin < 0 || frommin > 59 || tomin < 0 || tomin > 59)
			{
				log(LL_ERR, "ERROR parsing config file: invalid time [%s] at line %d!", *dt, lineno);
				config_error_flag++;
				return;
			}
			break;
		}
		else if ((*dt >= '0') && (*dt <= '7'))
		{
			/* dt points to day spec */
			day |= 1 << (*dt - '0');
			dt++;
			continue;
		}
		else if (*dt == ',')
		{
			/* dt points to delimiter */
			dt++;
			continue;
		}
		else if (*dt == '\0')
		{
			/* dt points to end of string */
			break;
		}
		else
		{
			/* dt points to illegal character */
			log(LL_ERR, "ERROR parsing config file: illegal character [%c=0x%x] in date/time spec at line %d!", *dt, *dt, lineno);
			config_error_flag++;
			return;
		}
	}
	cfg_entry_tab[entrycount].day = day;
	cfg_entry_tab[entrycount].fromhr = fromhr;
	cfg_entry_tab[entrycount].frommin = frommin;
	cfg_entry_tab[entrycount].tohr = tohr;
	cfg_entry_tab[entrycount].tomin = tomin;
}

/*---------------------------------------------------------------------------*
 *	configuration validation and consistency check
 *---------------------------------------------------------------------------*/
static void
check_config(void)
{
	cfg_entry_t *cep = &cfg_entry_tab[0];	/* ptr to config entry */
	int i;
	int error = 0;

	/* regular expression table */
	
	for(i=0; i < MAX_RE; i++)
	{
		if((rarr[i].re_expr != NULL) && (rarr[i].re_prog == NULL))
		{
			log(LL_ERR, "check_config: regular expression %d without program!", i);
			error++;
		}
		if((rarr[i].re_prog != NULL) && (rarr[i].re_expr == NULL))
		{
			log(LL_ERR, "check_config: regular expression program %d without expression!", i);
			error++;
		}
	}

	/* entry sections */
	
	for(i=0; i <= entrycount; i++, cep++)
	{
		/* isdn controller number */

		if((cep->isdncontroller < 0) || (cep->isdncontroller > (ncontroller-1)))
		{
			log(LL_ERR, "check_config: WARNING, isdncontroller out of range in entry %d!", i);
		}

		/* numbers used for dialout */
		
		if((cep->inout != DIR_INONLY) && (cep->dialin_reaction != REACT_ANSWER))
		{
			if(cep->remote_numbers_count == 0)
			{
				log(LL_ERR, "check_config: remote-phone-dialout not set in entry %d!", i);
				error++;
			}
			if(strlen(cep->local_phone_dialout) == 0)
			{
				log(LL_ERR, "check_config: local-phone-dialout not set in entry %d!", i);
				error++;
			}
		}

		/* numbers used for incoming calls */
		
		if(cep->inout != DIR_OUTONLY)
		{
			if(strlen(cep->local_phone_incoming) == 0)
			{
				log(LL_ERR, "check_config: local-phone-incoming not set in entry %d!", i);
				error++;
			}
			if(cep->incoming_numbers_count == 0)
			{
				log(LL_ERR, "check_config: remote-phone-incoming not set in entry %d!", i);
				error++;
			}
		}

		if((cep->dialin_reaction == REACT_ANSWER) && (cep->b1protocol != BPROT_NONE))
		{
			log(LL_ERR, "check_config: b1protocol not raw for telephony in entry %d!", i);
			error++;
		}

		if((cep->ppp_send_auth == AUTH_PAP) || (cep->ppp_send_auth == AUTH_CHAP))
		{
			if(cep->ppp_send_name[0] == 0)
			{
				log(LL_ERR, "check_config: no remote authentification name in entry %d!", i);
				error++;
			}
			if(cep->ppp_send_password[0] == 0)
			{
				log(LL_ERR, "check_config: no remote authentification password in entry %d!", i);
				error++;
			}
		}
		if((cep->ppp_expect_auth == AUTH_PAP) || (cep->ppp_expect_auth == AUTH_CHAP))
		{
			if(cep->ppp_expect_name[0] == 0)
			{
				log(LL_ERR, "check_config: no local authentification name in entry %d!", i);
				error++;
			}
			if(cep->ppp_expect_password[0] == 0)
			{
				log(LL_ERR, "check_config: no local authentification secret in entry %d!", i);
				error++;
			}
		}
	}
	if(error)
	{
		log(LL_ERR, "check_config: %d error(s) in configuration file, exit!", error);
		do_exit(1);
	}
}

/*---------------------------------------------------------------------------*
 *	print the configuration
 *---------------------------------------------------------------------------*/
static void
print_config(void)
{
#define PFILE stdout

#ifdef I4B_EXTERNAL_MONITOR
	extern struct monitor_rights * monitor_next_rights(const struct monitor_rights *r);
	struct monitor_rights *m_rights;
#endif
	cfg_entry_t *cep = &cfg_entry_tab[0];	/* ptr to config entry */
	int i, j;
	time_t clock;
	char mytime[64];

	time(&clock);
	strcpy(mytime, ctime(&clock));
	mytime[strlen(mytime)-1] = '\0';

	fprintf(PFILE, "#---------------------------------------------------------------------------\n");
	fprintf(PFILE, "# system section (generated %s)\n", mytime);
	fprintf(PFILE, "#---------------------------------------------------------------------------\n");
	fprintf(PFILE, "system\n");
	fprintf(PFILE, "useacctfile     = %s\n", useacctfile ? "on\t\t\t\t# update accounting information file" : "off\t\t\t\t# don't update accounting information file");
	fprintf(PFILE, "acctall         = %s\n", acct_all ? "on\t\t\t\t# put all events into accounting file" : "off\t\t\t\t# put only charged events into accounting file");
	fprintf(PFILE, "acctfile        = %s\t\t# accounting information file\n", acctfile);
	fprintf(PFILE, "ratesfile       = %s\t\t# charging rates database file\n", ratesfile);

#ifdef USE_RTPRIO
	if(rt_prio == RTPRIO_NOTUSED)
		fprintf(PFILE, "# rtprio is unused\n");
	else
		fprintf(PFILE, "rtprio          = %d\t\t\t\t# isdnd runs at realtime priority\n", rt_prio);
#endif

	/* regular expression table */
	
	for(i=0; i < MAX_RE; i++)
	{
		if(rarr[i].re_expr != NULL)
		{
			fprintf(PFILE, "regexpr         = \"%s\"\t\t# scan logfile for this expression\n", rarr[i].re_expr);
		}
		if(rarr[i].re_prog != NULL)
		{
			fprintf(PFILE, "regprog         = %s\t\t# program to run when expression is matched\n", rarr[i].re_prog);
		}
	}

#ifdef I4B_EXTERNAL_MONITOR

	fprintf(PFILE, "monitor-allowed = %s\n", do_monitor ? "on\t\t\t\t# remote isdnd monitoring allowed" : "off\t\t\t\t# remote isdnd monitoring disabled");
	fprintf(PFILE, "monitor-port    = %d\t\t\t\t# TCP/IP port number used for remote monitoring\n", monitorport);

	m_rights = monitor_next_rights(NULL);
	if(m_rights != NULL)
	{
		char *s = "error\n";
		char b[512];

		for ( ; m_rights != NULL; m_rights = monitor_next_rights(m_rights))
		{
			if(m_rights->local)
			{
				fprintf(PFILE, "monitor         = \"%s\"\t\t# local socket name for monitoring\n", m_rights->name);
			}
			else
			{
				struct in_addr ia;
				ia.s_addr = ntohl(m_rights->net);

				switch(m_rights->mask)
				{
					case 0xffffffff:
						s = "32";
						break;
					case 0xfffffffe:
						s = "31";
						break;
					case 0xfffffffc:
						s = "30";
						break;
					case 0xfffffff8:
						s = "29";
						break;
					case 0xfffffff0:
						s = "28";
						break;
					case 0xffffffe0:
						s = "27";
						break;
					case 0xffffffc0:
						s = "26";
						break;
					case 0xffffff80:
						s = "25";
						break;
					case 0xffffff00:
						s = "24";
						break;
					case 0xfffffe00:
						s = "23";
						break;
					case 0xfffffc00:
						s = "22";
						break;
					case 0xfffff800:
						s = "21";
						break;
					case 0xfffff000:
						s = "20";
						break;
					case 0xffffe000:
						s = "19";
						break;
					case 0xffffc000:
						s = "18";
						break;
					case 0xffff8000:
						s = "17";
						break;
					case 0xffff0000:
						s = "16";
						break;
					case 0xfffe0000:
						s = "15";
						break;
					case 0xfffc0000:
						s = "14";
						break;
					case 0xfff80000:
						s = "13";
						break;
					case 0xfff00000:
						s = "12";
						break;
					case 0xffe00000:
						s = "11";
						break;
					case 0xffc00000:
						s = "10";
						break;
					case 0xff800000:
						s = "9";
						break;
					case 0xff000000:
						s = "8";
						break;
					case 0xfe000000:
						s = "7";
						break;
					case 0xfc000000:
						s = "6";
						break;
					case 0xf8000000:
						s = "5";
						break;
					case 0xf0000000:
						s = "4";
						break;
					case 0xe0000000:
						s = "3";
						break;
					case 0xc0000000:
						s = "2";
						break;
					case 0x80000000:
						s = "1";
						break;
					case 0x00000000:
						s = "0";
						break;
				}
				fprintf(PFILE, "monitor         = \"%s/%s\"\t\t# host (net/mask) allowed to connect for monitoring\n", inet_ntoa(ia), s);
			}
			b[0] = '\0';
			
			if((m_rights->rights) & I4B_CA_COMMAND_FULL)
				strcat(b, "fullcmd,");
			if((m_rights->rights) & I4B_CA_COMMAND_RESTRICTED)
				strcat(b, "restrictedcmd,");
			if((m_rights->rights) & I4B_CA_EVNT_CHANSTATE)
				strcat(b, "channelstate,");
			if((m_rights->rights) & I4B_CA_EVNT_CALLIN)
				strcat(b, "callin,");
			if((m_rights->rights) & I4B_CA_EVNT_CALLOUT)
				strcat(b, "callout,");
			if((m_rights->rights) & I4B_CA_EVNT_I4B)
				strcat(b, "logevents,");

			if(b[strlen(b)-1] == ',')
				b[strlen(b)-1] = '\0';
				
			fprintf(PFILE, "monitor-access  = %s\t\t# monitor access rights\n", b);
		}
	}
	
#endif
	/* entry sections */
	
	for(i=0; i <= entrycount; i++, cep++)
	{
		fprintf(PFILE, "\n");
		fprintf(PFILE, "#---------------------------------------------------------------------------\n");
		fprintf(PFILE, "# entry section %d\n", i);
		fprintf(PFILE, "#---------------------------------------------------------------------------\n");
		fprintf(PFILE, "entry\n");

		fprintf(PFILE, "name                  = %s\t\t# name for this entry section\n", cep->name);

		fprintf(PFILE, "isdncontroller        = %d\t\t# ISDN card number used for this entry\n", cep->isdncontroller);
		fprintf(PFILE, "isdnchannel           = ");
		switch(cep->isdnchannel)
		{
				case CHAN_ANY:
					fprintf(PFILE, "-1\t\t# any ISDN B-channel may be used\n");
					break;
				case CHAN_B1:
					fprintf(PFILE, "1\t\t# only ISDN B-channel 1 may be used\n");
					break;
				case CHAN_B2:
					fprintf(PFILE, "2\t\t# only ISDN B-channel 2 ay be used\n");
					break;
		}

		fprintf(PFILE, "usrdevicename         = %s\t\t# name of userland ISDN B-channel device\n", bdrivername(cep->usrdevicename));
		fprintf(PFILE, "usrdeviceunit         = %d\t\t# unit number of userland ISDN B-channel device\n", cep->usrdeviceunit);

		fprintf(PFILE, "b1protocol            = %s\n", cep->b1protocol ? "hdlc\t\t# B-channel layer 1 protocol is HDLC" : "raw\t\t# No B-channel layer 1 protocol used");

		if(!(cep->usrdevicename == BDRV_TEL))
		{
			fprintf(PFILE, "direction             = ");
			switch(cep->inout)
			{
				case DIR_INONLY:
					fprintf(PFILE, "in\t\t# only incoming connections allowed\n");
					break;
				case DIR_OUTONLY:
					fprintf(PFILE, "out\t\t# only outgoing connections allowed\n");
					break;
				case DIR_INOUT:
					fprintf(PFILE, "inout\t\t# incoming and outgoing connections allowed\n");
					break;
			}
		}
		
		if(!((cep->usrdevicename == BDRV_TEL) || (cep->inout == DIR_INONLY)))
		{
			if(cep->remote_numbers_count > 1)
			{
				for(j=0; j<cep->remote_numbers_count; j++)
					fprintf(PFILE, "remote-phone-dialout  = %s\t\t# telephone number %d for dialing out to remote\n", cep->remote_numbers[j].number, j+1);

				fprintf(PFILE, "remdial-handling      = ");
		
				switch(cep->remote_numbers_handling)
				{
					case RNH_NEXT:
						fprintf(PFILE, "next\t\t# use next number after last successfull for new dial\n");
						break;
					case RNH_LAST:
						fprintf(PFILE, "last\t\t# use last successfull number for new dial\n");
						break;
					case RNH_FIRST:
						fprintf(PFILE, "first\t\t# always start with first number for new dial\n");
						break;
				}
			}
			else
			{
				fprintf(PFILE, "remote-phone-dialout  = %s\t\t# telephone number for dialing out to remote\n", cep->remote_numbers[0].number);
			}

			fprintf(PFILE, "local-phone-dialout   = %s\t\t# show this number to remote when dialling out\n", cep->local_phone_dialout);
			fprintf(PFILE, "dialout-type          = %s\n", cep->dialouttype ? "calledback\t\t# i am called back by remote" : "normal\t\t# i am not called back by remote");
		}

		if(!(cep->inout == DIR_OUTONLY))
		{
			int n;
			
			fprintf(PFILE, "local-phone-incoming  = %s\t\t# incoming calls must match this (mine) telephone number\n", cep->local_phone_incoming);
			for (n = 0; n < cep->incoming_numbers_count; n++)
				fprintf(PFILE, "remote-phone-incoming = %s\t\t# this is a valid remote number to call me\n",
					cep->remote_phone_incoming[n].number);

			fprintf(PFILE, "dialin-reaction       = ");
			switch(cep->dialin_reaction)
			{
				case REACT_ACCEPT:
					fprintf(PFILE, "accept\t\t# i accept a call from remote and connect\n");
					break;
				case REACT_REJECT:
					fprintf(PFILE, "reject\t\t# i reject the call from remote\n");
					break;
				case REACT_IGNORE:
					fprintf(PFILE, "ignore\t\t# i ignore the call from remote\n");
					break;
				case REACT_ANSWER:
					fprintf(PFILE, "answer\t\t# i will start telephone answering when remote calls in\n");
					break;
				case REACT_CALLBACK:
					fprintf(PFILE, "callback\t\t# when remote calls in, i will hangup and call back\n");
					break;
			}
		}

		if(cep->usrdevicename == BDRV_ISPPP)
		{
			char *s;
			switch(cep->ppp_expect_auth)
			{
				case AUTH_NONE:
					s = "none";
					break;
				case AUTH_PAP:
					s = "pap";
					break;
				case AUTH_CHAP:
					s = "chap";
					break;
				default:
					s = NULL;
					break;
			}
			if(s != NULL)
			{
				fprintf(PFILE, "ppp-expect-auth       = %s\t\t# the auth protocol we expect to receive on dial-in (none,pap,chap)\n", s);
				if(cep->ppp_expect_auth != AUTH_NONE)
				{
					fprintf(PFILE, "ppp-expect-name       = %s\t\t# the user name allowed in\n", cep->ppp_expect_name);
					fprintf(PFILE, "ppp-expect-password   = %s\t\t# the key expected from the other side\n", cep->ppp_expect_password);
					fprintf(PFILE, "ppp-auth-paranoid     = %s\t\t# do we require remote to authenticate even if we dial out\n", cep->ppp_auth_flags & AUTH_REQUIRED ? "yes" : "no");
				}
			}
			switch(cep->ppp_send_auth)
			{
				case AUTH_NONE:
					s = "none";
					break;
				case AUTH_PAP:
					s = "pap";
					break;
				case AUTH_CHAP:
					s = "chap";
					break;
				default:
					s = NULL;
					break;
			}
			if(s != NULL)
			{
				fprintf(PFILE, "ppp-send-auth         = %s\t\t# the auth protocol we use when dialing out (none,pap,chap)\n", s);
				if(cep->ppp_send_auth != AUTH_NONE)
				{
					fprintf(PFILE, "ppp-send-name         = %s\t\t# our PPP account used for dial-out\n", cep->ppp_send_name);
					fprintf(PFILE, "ppp-send-password     = %s\t\t# the key sent to the other side\n", cep->ppp_send_password);
				}
			}
			if(cep->ppp_send_auth == AUTH_CHAP ||
			   cep->ppp_expect_auth == AUTH_CHAP) {
				fprintf(PFILE, "ppp-auth-rechallenge   = %s\t\t# rechallenge CHAP connections once in a while\n", cep->ppp_auth_flags & AUTH_RECHALLENGE ? "yes" : "no");
			}
		}

		if(!((cep->inout == DIR_INONLY) || (cep->usrdevicename == BDRV_TEL)))
		{
			char *s;
			fprintf(PFILE, "idletime-outgoing     = %d\t\t# outgoing call idle timeout\n", cep->idle_time_out);

			switch( cep->shorthold_algorithm )
			{
				case SHA_FIXU:
					s = "fix-unit-size";
					break;
				case SHA_VARU:
					s = "var-unit-size";
					break;
				default:
					s = "error!!!";
					break;
			}

			fprintf(PFILE, "idle-algorithm-outgoing     = %s\t\t# outgoing call idle algorithm\n", s);
		}

		if(!(cep->inout == DIR_OUTONLY))
			fprintf(PFILE, "idletime-incoming     = %d\t\t# incoming call idle timeout\n", cep->idle_time_in);

		if(!(cep->usrdevicename == BDRV_TEL))
		{		
	 		fprintf(PFILE, "unitlengthsrc         = ");
			switch(cep->unitlengthsrc)
			{
				case ULSRC_NONE:
					fprintf(PFILE, "none\t\t# no unit length specified, using default\n");
					break;
				case ULSRC_CMDL:
					fprintf(PFILE, "cmdl\t\t# using unit length specified on commandline\n");
					break;
				case ULSRC_CONF:
					fprintf(PFILE, "conf\t\t# using unitlength specified by unitlength-keyword\n");
					fprintf(PFILE, "unitlength            = %d\t\t# fixed unitlength\n", cep->unitlength);
					break;
				case ULSRC_RATE:
					fprintf(PFILE, "rate\t\t# using unitlength specified in rate database\n");
					fprintf(PFILE, "ratetype              = %d\t\t# type of rate from rate database\n", cep->ratetype);
					break;
				case ULSRC_DYN:
					fprintf(PFILE, "aocd\t\t# using dynamically calculated unitlength based on AOCD subscription\n");
					fprintf(PFILE, "ratetype              = %d\t\t# type of rate from rate database\n", cep->ratetype);
					break;
			}

			fprintf(PFILE, "earlyhangup           = %d\t\t# early hangup safety time\n", cep->earlyhangup);

		}
		
		if(cep->usrdevicename == BDRV_TEL)
		{
			fprintf(PFILE, "answerprog            = %s\t\t# program used to answer incoming telephone calls\n", cep->answerprog);
			fprintf(PFILE, "alert                 = %d\t\t# number of seconds to wait before accepting a call\n", cep->alert);
		}

		if(!(cep->usrdevicename == BDRV_TEL))
		{		
			if(cep->dialin_reaction == REACT_CALLBACK)
				fprintf(PFILE, "callbackwait          = %d\t\t# i am waiting this time before calling back remote\n", cep->callbackwait);
	
			if(cep->dialouttype == DIALOUT_CALLEDBACK)
				fprintf(PFILE, "calledbackwait        = %d\t\t# i am waiting this time for a call back from remote\n", cep->calledbackwait);
	
			if(!(cep->inout == DIR_INONLY))
			{
				fprintf(PFILE, "dialretries           = %d\t\t# number of dialing retries\n", cep->dialretries);
				fprintf(PFILE, "recoverytime          = %d\t\t# time to wait between dialling retries\n", cep->recoverytime);
				fprintf(PFILE, "dialrandincr          = %s\t\t# use random dialing time addon\n", cep->dialrandincr ? "on" : "off");

				fprintf(PFILE, "usedown               = %s\n", cep->usedown ? "on\t\t# ISDN device switched off on excessive dial failures" : "off\t\t# no device switchoff on excessive dial failures");
				if(cep->usedown)
				{
					fprintf(PFILE, "downtries             = %d\t\t# number of dialretries failures before switching off\n", cep->downtries);
					fprintf(PFILE, "downtime              = %d\t\t# time device is switched off\n", cep->downtime);
				}
			}
		}		
	}
	fprintf(PFILE, "\n");	
}

/* EOF */
