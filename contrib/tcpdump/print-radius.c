/*
 * Copyright (C) 2000 Alfredo Andres Omella.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *   3. The names of the authors may not be used to endorse or promote
 *      products derived from this software without specific prior
 *      written permission.
 *  
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
/*
 * Radius printer routines as specified on:
 *
 * RFC 2865:
 *      "Remote Authentication Dial In User Service (RADIUS)"
 *
 * RFC 2866:
 *      "RADIUS Accounting"
 *
 * RFC 2867:
 *      "RADIUS Accounting Modifications for Tunnel Protocol Support"
 *
 * RFC 2868:
 *      "RADIUS Attributes for Tunnel Protocol Support"
 *
 * RFC 2869:
 *      "RADIUS Extensions"
 *
 * Alfredo Andres Omella (aandres@s21sec.com) v0.1 2000/09/15
 *
 * TODO: Among other things to print ok MacIntosh and Vendor values 
 */

#ifndef lint
static const char rcsid[] =
    "$Id: print-radius.c,v 1.10.2.2 2002/07/03 16:35:04 fenner Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <sys/param.h>

#include <netinet/in.h>

#include <stdio.h>

#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

#define TAM_SIZE(x) (sizeof(x)/sizeof(x[0]) )

#define PRINT_HEX(bytes_len, ptr_data)                               \
           while(bytes_len)                                          \
           {                                                         \
              printf("%02X", *ptr_data );                            \
              ptr_data++;                                            \
              bytes_len--;                                           \
           }


/* Radius packet codes */
#define RADCMD_ACCESS_REQ   1 /* Access-Request      */
#define RADCMD_ACCESS_ACC   2 /* Access-Accept       */
#define RADCMD_ACCESS_REJ   3 /* Access-Reject       */
#define RADCMD_ACCOUN_REQ   4 /* Accounting-Request  */
#define RADCMD_ACCOUN_RES   5 /* Accounting-Response */
#define RADCMD_ACCESS_CHA  11 /* Access-Challenge    */
#define RADCMD_STATUS_SER  12 /* Status-Server       */
#define RADCMD_STATUS_CLI  13 /* Status-Client       */
#define RADCMD_RESERVED   255 /* Reserved            */


/********************************/
/* Begin Radius Attribute types */
/********************************/
#define SERV_TYPE    6
#define FRM_IPADDR   8
#define LOG_IPHOST  14
#define LOG_SERVICE 15
#define FRM_IPX     23
#define SESSION_TIMEOUT   27
#define IDLE_TIMEOUT      28
#define FRM_ATALK_LINK    37
#define FRM_ATALK_NETWORK 38

#define ACCT_DELAY        41
#define ACCT_SESSION_TIME 46

#define TUNNEL_TYPE        64
#define TUNNEL_MEDIUM      65
#define TUNNEL_CLIENT_END  66
#define TUNNEL_SERVER_END  67
#define TUNNEL_PASS        69

#define ARAP_PASS          70
#define ARAP_FEATURES      71

#define TUNNEL_PRIV_GROUP  81
#define TUNNEL_ASSIGN_ID   82
#define TUNNEL_PREFERENCE  83

#define ARAP_CHALLENGE_RESP 84
#define ACCT_INT_INTERVAL   85

#define TUNNEL_CLIENT_AUTH 90
#define TUNNEL_SERVER_AUTH 91
/********************************/
/* End Radius Attribute types */
/********************************/


static void print_attr_string(register u_char *, u_int, u_short );
static void print_attr_num(register u_char *, u_int, u_short );
static void print_attr_address(register u_char *, u_int, u_short);
static void print_attr_time(register u_char *, u_int, u_short);
static void print_attr_strange(register u_char *, u_int, u_short);


struct radius_hdr { u_int8_t  code; /* Radius packet code  */
                    u_int8_t  id;   /* Radius packet id    */
                    u_int16_t len;  /* Radius total length */
                    u_int8_t  auth[16]; /* Authenticator   */
                  };

#define MIN_RADIUS_LEN	20

struct radius_attr { u_int8_t type; /* Attribute type   */
                     u_int8_t len;  /* Attribute length */
                   };


/* Service-Type Attribute standard values */                 
static const char *serv_type[]={ NULL,
                                "Login",
                                "Framed",  
                                "Callback Login",
                                "Callback Framed",
                                "Outbound",
                                "Administrative",
                                "NAS Prompt",                            
                                "Authenticate Only",
                                "Callback NAS Prompt",
                                "Call Check",
                                "Callback Administrative",
                               };                               

/* Framed-Protocol Attribute standard values */
static const char *frm_proto[]={ NULL,
                                 "PPP",
                                 "SLIP",
                                 "ARAP",
                                 "Gandalf proprietary",
                                 "Xylogics IPX/SLIP",
                                 "X.75 Synchronous",
                               };                               

/* Framed-Routing Attribute standard values */
static const char *frm_routing[]={ "None",
                                   "Send",
                                   "Listen",
                                   "Send&Listen",
                                 };                               

/* Framed-Compression Attribute standard values */
static const char *frm_comp[]={ "None",
                                "VJ TCP/IP",
                                "IPX",
                                "Stac-LZS",
                              };

/* Login-Service Attribute standard values */
static const char *login_serv[]={ "Telnet",
                                  "Rlogin",
                                  "TCP Clear",
                                  "PortMaster(proprietary)",
                                  "LAT",
                                  "X.25-PAD",
                                  "X.25-T3POS",
                                  "Unassigned",
                                  "TCP Clear Quiet",
                                };


/* Termination-Action Attribute standard values */
static const char *term_action[]={ "Default",
                                   "RADIUS-Request",
                                 };

/* NAS-Port-Type Attribute standard values */
static const char *nas_port_type[]={ "Async",
                                     "Sync",
                                     "ISDN Sync",
                                     "ISDN Async V.120",
                                     "ISDN Async V.110",
                                     "Virtual",
                                     "PIAFS",
                                     "HDLC Clear Channel",
                                     "X.25",
                                     "X.75",
                                     "G.3 Fax",
                                     "SDSL",
                                     "ADSL-CAP",
                                     "ADSL-DMT",
                                     "ISDN-DSL",
                                     "Ethernet",
                                     "xDSL",
                                     "Cable",
                                     "Wireless - Other",
                                     "Wireless - IEEE 802.11",
                                   };         

/* Acct-Status-Type Accounting Attribute standard values */
static const char *acct_status[]={ NULL,
                                   "Start",
                                   "Stop",
                                   "Interim-Update",
                                   "Unassigned",
                                   "Unassigned",
                                   "Unassigned",
                                   "Accounting-On",
                                   "Accounting-Off",
                                   "Tunnel-Start",
                                   "Tunnel-Stop",
                                   "Tunnel-Reject",
                                   "Tunnel-Link-Start",
                                   "Tunnel-Link-Stop",
                                   "Tunnel-Link-Reject",
                                   "Failed",
                                 };

/* Acct-Authentic Accounting Attribute standard values */
static const char *acct_auth[]={ NULL,
                                 "RADIUS",
                                 "Local",
                                 "Remote",
                               };

/* Acct-Terminate-Cause Accounting Attribute standard values */
static const char *acct_term[]={ NULL,
                                 "User Request",
                                 "Lost Carrier",
                                 "Lost Service",
                                 "Idle Timeout",
                                 "Session Timeout",
                                 "Admin Reset",
                                 "Admin Reboot",
                                 "Port Error",
                                 "NAS Error",
                                 "NAS Request",
                                 "NAS Reboot",
                                 "Port Unneeded",
                                 "Port Preempted",
                                 "Port Suspended",
                                 "Service Unavailable",
                                 "Callback",
                                 "User Error",
                                 "Host Request",
                               };

/* Tunnel-Type Attribute standard values */
static const char *tunnel_type[]={ NULL,
                                   "PPTP",
                                   "L2F",
                                   "L2TP",
                                   "ATMP",
                                   "VTP",
                                   "AH",
                                   "IP-IP",
                                   "MIN-IP-IP",
                                   "ESP",
                                   "GRE",
                                   "DVS",
                                   "IP-in-IP Tunneling",
                                 };
                                   
/* Tunnel-Medium-Type Attribute standard values */
static const char *tunnel_medium[]={ NULL,
                                     "IPv4",
                                     "IPv6",
                                     "NSAP",
                                     "HDLC",
                                     "BBN 1822",
                                     "802",
                                     "E.163",
                                     "E.164",
                                     "F.69",
                                     "X.121",
                                     "IPX",
                                     "Appletalk",
                                     "Decnet IV",
                                     "Banyan Vines",
                                     "E.164 with NSAP subaddress",
                                   };

/* ARAP-Zone-Access Attribute standard values */
static const char *arap_zone[]={ NULL,
                                 "Only access to dfl zone",
                                 "Use zone filter inc.",
                                 "Not used",
                                 "Use zone filter exc.",
                               };

static const char *prompt[]={ "No Echo",
                              "Echo",
                            };
                            
                                                               
struct attrtype { char *name;            /* Attribute name                 */
                  const char **subtypes; /* Standard Values (if any)       */
                  u_char siz_subtypes;   /* Size of total standard values  */
                  u_char first_subtype;  /* First standard value is 0 or 1 */
                  void (*print_func)(register u_char *, u_int, u_short );
                } attr_type[]=
  {
     { NULL,             NULL, 0, 0, NULL               },
     { "User",           NULL, 0, 0, print_attr_string  },
     { "Pass",           NULL, 0, 0, NULL               },
     { "CHAP-Pass",      NULL, 0, 0, NULL               },
     { "NAS_ipaddr",     NULL, 0, 0, print_attr_address },
     { "NAS_port",       NULL, 0, 0, print_attr_num     },
     { "Service_type",   serv_type, TAM_SIZE(serv_type)-1, 1, print_attr_num },                 
     { "Framed_proto",   frm_proto, TAM_SIZE(frm_proto)-1, 1, print_attr_num },
     { "Framed_ipaddr",  NULL, 0, 0, print_attr_address },
     { "Framed_ipnet",   NULL, 0, 0, print_attr_address },
     { "Framed_routing", frm_routing, TAM_SIZE(frm_routing), 0, 
                                                              print_attr_num }, 
     { "Filter_id",      NULL, 0, 0, print_attr_string  },
     { "Framed_mtu",     NULL, 0, 0, print_attr_num     },
     { "Framed_compress",  frm_comp, TAM_SIZE(frm_comp),   0, print_attr_num },
     { "Login_iphost",   NULL, 0, 0, print_attr_address },
     { "Login_service",  login_serv, TAM_SIZE(login_serv), 0, print_attr_num },
     { "Login_TCP_port", NULL, 0, 0, print_attr_num     },                 
/*17*/ { "Unassigned", NULL, 0, 0, NULL },                 
     { "Reply",           NULL, 0, 0, print_attr_string },
     { "Callback-number", NULL, 0, 0, print_attr_string },
     { "Callback-id",     NULL, 0, 0, print_attr_string },
/*21*/ { "Unassigned", NULL, 0, 0, NULL },   
     { "Framed_route",      NULL, 0, 0, print_attr_string },
     { "Framed_ipx_net",    NULL, 0, 0, print_attr_num    },
     { "State",             NULL, 0, 0, print_attr_string },
     { "Class",             NULL, 0, 0, print_attr_string },
     { "Vendor_specific",   NULL, 0, 0, print_attr_string },
     { "Session_timeout",   NULL, 0, 0, print_attr_num    },
     { "Idle_timeout",      NULL, 0, 0, print_attr_num    },
     { "Term_action", term_action, TAM_SIZE(term_action), 0, print_attr_num },
     { "Called_station",    NULL, 0, 0, print_attr_string },
     { "Calling_station",   NULL, 0, 0, print_attr_string },   
     { "NAS_id",            NULL, 0, 0, print_attr_string },
     { "Proxy_state",       NULL, 0, 0, print_attr_string },
     { "Login_LAT_service", NULL, 0, 0, print_attr_string },
     { "Login_LAT_node",    NULL, 0, 0, print_attr_string },
     { "Login_LAT_group",   NULL, 0, 0, print_attr_string },
     { "Framed_atalk_link", NULL, 0, 0, print_attr_num    },
     { "Framed_atalk_net",  NULL, 0, 0, print_attr_num    },
     { "Framed_atalk_zone", NULL, 0, 0, print_attr_string },
     { "Acct_status", acct_status, TAM_SIZE(acct_status)-1, 1, print_attr_num },
     { "Acct_delay",        NULL, 0, 0, print_attr_num    },
     { "Acct_in_octets",    NULL, 0, 0, print_attr_num    },
     { "Acct_out_octets",   NULL, 0, 0, print_attr_num    },
     { "Acct_session_id",   NULL, 0, 0, print_attr_string },
     { "Acct_authentic",  acct_auth, TAM_SIZE(acct_auth)-1, 1, print_attr_num },
     { "Acct_session_time", NULL, 0, 0, print_attr_num },
     { "Acct_in_packets",   NULL, 0, 0, print_attr_num },
     { "Acct_out_packets",  NULL, 0, 0, print_attr_num },
     { "Acct_term_cause", acct_term, TAM_SIZE(acct_term)-1, 1, print_attr_num },
     { "Acct_multi_session_id", NULL, 0, 0, print_attr_string },
     { "Acct_link_count", NULL, 0, 0, print_attr_num },
     { "Acct_in_giga",    NULL, 0, 0, print_attr_num },
     { "Acct_out_giga",   NULL, 0, 0, print_attr_num },
/*54*/ { "Unassigned", NULL, 0, 0, NULL },
     { "Event_timestamp", NULL, 0, 0, print_attr_time },
/*56*/ { "Unassigned", NULL, 0, 0, NULL },
/*57*/ { "Unassigned", NULL, 0, 0, NULL },
/*58*/ { "Unassigned", NULL, 0, 0, NULL },
/*59*/ { "Unassigned", NULL, 0, 0, NULL },
     { "CHAP_challenge", NULL, 0, 0, print_attr_string },  
     { "NAS_port_type",  nas_port_type, TAM_SIZE(nas_port_type), 0, 
                                                              print_attr_num },
     { "Port_limit",     NULL, 0, 0, print_attr_num },
/*63*/ { "Login_LAT_port", NULL, 0, 0, print_attr_string },
     { "Tunnel_type", tunnel_type, TAM_SIZE(tunnel_type)-1, 1, print_attr_num },
     { "Tunnel_medium", tunnel_medium, TAM_SIZE(tunnel_medium)-1, 1, 
                                                             print_attr_num },
     { "Tunnel_client_end",   NULL, 0, 0, print_attr_string },
     { "Tunnel_server_end",   NULL, 0, 0, print_attr_string },
     { "Acct_tunnel_connect", NULL, 0, 0, print_attr_string },
     { "Tunnel_pass",  NULL, 0, 0, print_attr_string  },
     { "ARAP_pass",    NULL, 0, 0, print_attr_strange },
     { "ARAP_feature", NULL, 0, 0, print_attr_strange },
/*72*/ { "ARAP_zone_acces", arap_zone, TAM_SIZE(arap_zone)-1, 1, 
                                                             print_attr_num },
     { "ARAP_security",      NULL, 0, 0, print_attr_string },
     { "ARAP_security_data", NULL, 0, 0, print_attr_string },
     { "Password_retry",     NULL, 0, 0, print_attr_num    },
     { "Prompt", prompt, TAM_SIZE(prompt), 0, print_attr_num },
     { "Connect_info",       NULL, 0, 0, print_attr_string   },
     { "Config_token",       NULL, 0, 0, print_attr_string   },
     { "EAP_msg",            NULL, 0, 0, print_attr_string   },
/*80*/ { "Message_auth",    NULL, 0, 0, print_attr_string },
     { "Tunnel_priv_group", NULL, 0, 0, print_attr_string },
     { "Tunnel_assign_id",  NULL, 0, 0, print_attr_string },
     { "Tunnel_pref",       NULL, 0, 0, print_attr_num    },
     { "ARAP_challenge_resp",    NULL, 0, 0, print_attr_strange },
     { "Acct_interim_interval",  NULL, 0, 0, print_attr_num     },
/*86*/ { "Acct_tunnel_pack_lost",  NULL, 0, 0, print_attr_num },
     { "NAS_port_id", NULL, 0, 0, print_attr_string },
     { "Framed_pool", NULL, 0, 0, print_attr_string },
     { "Unassigned",  NULL, 0, 0, NULL },
     { "Tunnel_client_auth_id", NULL, 0, 0, print_attr_string },
     { "Tunnel_server_auth_id", NULL, 0, 0, print_attr_string },
/*92*/ { "Unassigned",  NULL, 0, 0, NULL },
/*93*/ { "Unassigned",  NULL, 0, 0, NULL }
  };                    


/*****************************/
/* Print an attribute string */
/* value pointed by 'data'   */
/* and 'length' size.        */
/*****************************/
/* Returns nothing.          */
/*****************************/
static void
print_attr_string(register u_char *data, u_int length, u_short attr_code )
{
   register u_int i;
   
   TCHECK2(data[0],length);
   
   printf("{");
   switch(attr_code)
   {
      case TUNNEL_PASS:
           if (*data && (*data <=0x1F) )
              printf("Tag[%d] ",*data);
           data++;
           printf("Salt[%d] ",EXTRACT_16BITS(data) );
           data+=2;
           length-=2;
        break;
      case TUNNEL_CLIENT_END:
      case TUNNEL_SERVER_END:
      case TUNNEL_PRIV_GROUP:
      case TUNNEL_ASSIGN_ID:
      case TUNNEL_CLIENT_AUTH:
      case TUNNEL_SERVER_AUTH:
           if (*data <= 0x1F)
           {
              printf("Tag[%d] ",*data);
              data++;
              length--;
           }
        break;
   }

   for (i=0; i < length ; i++, data++)
       printf("%c",(*data < 32 || *data > 128) ? '.' : *data );

   printf("}");
   
   return;
   
   trunc:
      printf("|radius");
}  


/******************************/
/* Print an attribute numeric */
/* value pointed by 'data'    */
/* and 'length' size.         */
/******************************/
/* Returns nothing.           */
/******************************/
static void
print_attr_num(register u_char *data, u_int length, u_short attr_code )
{
   u_int8_t tag;
   u_int32_t timeout;
   
   if (length != 4)
   {
       printf("{length %u != 4}", length);
       return;
   }

   TCHECK2(data[0],4);
                          /* This attribute has standard values */
   if (attr_type[attr_code].siz_subtypes) 
   {
      static const char **table;
      u_int32_t data_value;
      table = attr_type[attr_code].subtypes;
      
      if ( (attr_code == TUNNEL_TYPE) || (attr_code == TUNNEL_MEDIUM) )
      {
         if (!*data)
            printf("{Tag[Unused]");
         else
            printf("{Tag[%d]", *data);
         data++;
         data_value = EXTRACT_24BITS(data);
      }
      else
      {
         data_value = EXTRACT_32BITS(data);
      }
      if ( data_value <= (attr_type[attr_code].siz_subtypes - 1 +
            attr_type[attr_code].first_subtype) &&
	   data_value >= attr_type[attr_code].first_subtype )
         printf("{%s}",table[data_value]);
      else
         printf("{#%d}",data_value);          
   }
   else
   {
      switch(attr_code) /* Be aware of special cases... */
      {
        case FRM_IPX:
             if (EXTRACT_32BITS( data) == 0xFFFFFFFE )
                printf("{NAS_select}");
             else
                printf("{%d}",EXTRACT_32BITS( data) );          
          break;

        case SESSION_TIMEOUT:
        case IDLE_TIMEOUT:
        case ACCT_DELAY:
        case ACCT_SESSION_TIME:
        case ACCT_INT_INTERVAL:
             timeout = EXTRACT_32BITS( data);
             if ( timeout < 60 )
                printf( "{%02d secs}", timeout);
             else
             {
                if ( timeout < 3600 )
                   printf( "{%02d:%02d min}", 
                          timeout / 60, timeout % 60);
                else
                   printf( "{%02d:%02d:%02d hours}",
                          timeout / 3600, (timeout % 3600) / 60, 
                          timeout % 60);
             }
          break;

        case FRM_ATALK_LINK:
             if (EXTRACT_32BITS(data) )          
                printf("{%d}",EXTRACT_32BITS(data) );
             else
                printf("{Unnumbered}" );
          break;
             
        case FRM_ATALK_NETWORK:
             if (EXTRACT_32BITS(data) )          
                printf("{%d}",EXTRACT_32BITS(data) );
             else
                printf("{NAS_assign}" );          
          break;

        case TUNNEL_PREFERENCE:
            tag = *data;
            data++;
            if (tag == 0)
               printf("{Tag[Unused] %d}",EXTRACT_24BITS(data) );
            else
               printf("{Tag[%d] %d}", tag, EXTRACT_24BITS(data) );
          break;

        default:
             printf("{%d}",EXTRACT_32BITS( data) );
          break;
      
      } /* switch */
   
   } /* if-else */

   return;
   
   trunc:
     printf("|radius}");
}


/*****************************/
/* Print an attribute IPv4   */
/* address value pointed by  */
/* 'data' and 'length' size. */
/*****************************/
/* Returns nothing.          */
/*****************************/
static void
print_attr_address(register u_char *data, u_int length, u_short attr_code )
{
   if (length != 4)
   {
       printf("{length %u != 4}", length);
       return;
   }

   TCHECK2(data[0],4);
   
   switch(attr_code)
   {
      case FRM_IPADDR:
      case LOG_IPHOST:
           if (EXTRACT_32BITS(data) == 0xFFFFFFFF )
              printf("{User_select}");
           else
              if (EXTRACT_32BITS(data) == 0xFFFFFFFE )
                 printf("{NAS_select}");
              else
                 printf("{%s}",ipaddr_string(data));
      break;
      
      default:
          printf("{%s}",ipaddr_string(data) );
      break;
   }
   
   return;
   
   trunc:
     printf("{|radius}");
}


/*************************************/
/* Print an attribute of 'secs since */
/* January 1, 1970 00:00 UTC' value  */
/* pointed by 'data' and 'length'    */
/* size.                             */
/*************************************/
/* Returns nothing.                  */
/*************************************/
static void print_attr_time(register u_char *data, u_int length, u_short attr_code)
{
   time_t attr_time;
   char string[26];

   if (length != 4)
   {
       printf("{length %u != 4}", length);
       return;
   }

   TCHECK2(data[0],4);
   
   attr_time = EXTRACT_32BITS(data);
   strlcpy(string, ctime(&attr_time), sizeof(string));
   /* Get rid of the newline */
   string[24] = '\0';
   printf("{%.24s}", string);
   return;
   
   trunc:
     printf("{|radius}");
}

           
/***********************************/
/* Print an attribute of 'strange' */
/* data format pointed by 'data'   */
/* and 'length' size.              */
/***********************************/
/* Returns nothing.                */
/***********************************/
static void print_attr_strange(register u_char *data, u_int length, u_short attr_code)
{
   u_short len_data;
   
   switch(attr_code)
   {
      case ARAP_PASS:
           if (length != 16)
           {
               printf("{length %u != 16}", length);
               return;
           }
           printf("{User_challenge[");
           TCHECK2(data[0],8);
           len_data = 8;
           PRINT_HEX(len_data, data);
           printf("] User_resp[");
           TCHECK2(data[0],8);
           len_data = 8;
           PRINT_HEX(len_data, data);
           printf("]}");
        break;
        
      case ARAP_FEATURES:
           if (length != 14)
           {
               printf("{length %u != 14}", length);
               return;
           }
           TCHECK2(data[0],1);
           if (*data)
              printf("{User_can_change_pass");
           else
              printf("{User_cant_change_pass");
           data++;
           TCHECK2(data[0],1);
           printf(" Min_pass_len[%d]",*data);
           data++;
           printf(" Pass_created_at[");
           TCHECK2(data[0],4);
           len_data = 4;
           PRINT_HEX(len_data, data);
           printf("] Pass_expired_in[");
           TCHECK2(data[0],4);
           len_data = 4;
           PRINT_HEX(len_data, data);
           printf("] Current_time[");
           len_data = 4;
           TCHECK2(data[0],4);
           PRINT_HEX(len_data, data);
           printf("]}");
        break;

      case ARAP_CHALLENGE_RESP:
           if (length < 8)
           {
               printf("{length %u != 8}", length);
               return;
           }
           printf("{");
           TCHECK2(data[0],8);
           len_data = 8;
           PRINT_HEX(len_data, data);
           printf("}");
        break;
   }
   
   trunc:
     printf("|radius}");
}



static void
radius_attr_print(register const u_char *attr, u_int length)
{
   register const struct radius_attr *rad_attr = (struct radius_attr *)attr;
   
   if (length < 3)
   {
      printf(" [|radius]");
      return;
   }
 
   printf(" Attr[ ");
   while (length > 0)
   {
     if (rad_attr->len == 0)
     {
     	printf("(zero-length attribute)");
     	return;
     }
     if ( rad_attr->len <= length )
     {
        if ( !rad_attr->type || (rad_attr->type > (TAM_SIZE(attr_type)-1))  )
           printf("#%d",rad_attr->type);
        else
        {
           printf(" %s",attr_type[rad_attr->type].name);

           if (rad_attr->len > 2)
           {
               if ( attr_type[rad_attr->type].print_func )
                  (*attr_type[rad_attr->type].print_func)( 
		                           ((u_char *)(rad_attr+1)),
                                           rad_attr->len - 2, rad_attr->type);
           }
        }
     }
     else
     {
        printf(" [|radius]");
        return;
     }
     length-=(rad_attr->len);
     rad_attr = (struct radius_attr *)( ((char *)(rad_attr))+rad_attr->len);
   }
   
   printf(" ]");
}


void
radius_print(const u_char *dat, u_int length)
{
   register const struct radius_hdr *rad;
   register int i;
   int len;
   
   i = min(length, snapend - dat);

   if (i < MIN_RADIUS_LEN)
   {
	  printf(" [|radius]");
	  return;
   }

   rad = (struct radius_hdr *)dat;
   len = ntohs(rad->len);

   if (len < MIN_RADIUS_LEN)
   {
	  printf(" [|radius]");
	  return;
   }

   if (len < i)
	  i = len;
   
   i -= MIN_RADIUS_LEN;

   switch (rad->code) 
   {
     case RADCMD_ACCESS_REQ:
         printf(" rad-access-req %d", length);
         break;

     case RADCMD_ACCESS_ACC:
         printf(" rad-access-accept %d", length);
         break;

     case RADCMD_ACCESS_REJ:
         printf(" rad-access-reject %d", length);
         break;

     case RADCMD_ACCOUN_REQ:
         printf(" rad-account-req %d", length);
         break;

     case RADCMD_ACCOUN_RES:
         printf(" rad-account-resp %d", length);
         break;

     case RADCMD_ACCESS_CHA:
         printf(" rad-access-cha %d", length);
         break;

     case RADCMD_STATUS_SER:
         printf(" rad-status-serv %d", length);
         break;

     case RADCMD_STATUS_CLI:
         printf(" rad-status-cli %d", length);
         break;

     case RADCMD_RESERVED:
         printf(" rad-reserved %d", length);
         break;

     default:
         printf(" rad-#%d %d", rad->code, length);
         break;
   }
   printf(" [id %d]", rad->id);
 
   if (i)
      radius_attr_print( dat + MIN_RADIUS_LEN, i);  
}
