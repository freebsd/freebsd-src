/*
 * $Id: libpamc.h,v 1.2 2000/11/19 23:54:03 agmorgan Exp $
 *
 * Copyright (c) Andrew G. Morgan <morgan@ftp.kernel.org>
 *
 */

#ifndef LIBPAMC_H
#define LIBPAMC_H

#include <security/pam_client.h>
#include <security/_pam_macros.h>

#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#define _PAMC_DEFAULT_TOP_FD  10

struct pamc_handle_s {
    struct pamc_agent_s *current;
    struct pamc_agent_s *chain;
    struct pamc_blocked_s *blocked_agents;
    int max_path;
    char **agent_paths;
    int combined_status;
    int highest_fd_to_close;
};

typedef struct pamc_blocked_s {
    char *id;                       /* <NUL> terminated */
    struct pamc_blocked_s *next;
} pamc_blocked_t;

typedef struct pamc_agent_s {
    char *id;
    int id_length;
    struct pamc_agent_s *next;
    int writer;   /* write to agent */
    int reader;   /* read from agent */
    pid_t pid;    /* agent process id */
} pamc_agent_t;

/* used to build a tree of unique, sorted agent ids */

typedef struct pamc_id_node {
    struct pamc_id_node *left, *right;
    int child_count;
    char *agent_id;
} pamc_id_node_t;

/* internal function */
int __pamc_valid_agent_id(int id_length, const char *id);

#define PAMC_SYSTEM_AGENT_PATH        "/lib/pamc:/usr/lib/pamc"
#define PAMC_SYSTEM_AGENT_SEPARATOR   ':'

#endif /* LIBPAMC_H */
