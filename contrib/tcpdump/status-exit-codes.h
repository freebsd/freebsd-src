/*
 * Copyright (c) 2018 The TCPDUMP project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 */

#ifndef status_exit_codes_h
#define status_exit_codes_h

/* S_ERR_ND_* are libnetdissect status */

typedef enum {
	S_SUCCESS           = 0, /* not a libnetdissect status */
	S_ERR_HOST_PROGRAM  = 1, /* not a libnetdissect status */
	S_ERR_ND_MEM_ALLOC  = 12,
	S_ERR_ND_OPEN_FILE  = 13,
	S_ERR_ND_WRITE_FILE = 14,
	S_ERR_ND_ESP_SECRET = 15
} status_exit_codes_t;

#endif /* status_exit_codes_h */
