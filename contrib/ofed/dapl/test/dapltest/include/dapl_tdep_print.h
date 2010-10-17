/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#ifndef _DAPL_TDEP_PRINT_H_
#define _DAPL_TDEP_PRINT_H_

#define DT_Tdep_PT_Debug(N, _X_) \
do { \
      if (DT_dapltest_debug >= (N)) \
        { \
          DT_Tdep_PT_Printf _X_; \
        } \
} while (0)

#ifdef __KERNEL__
typedef struct Tdep_Print_Entry_Tag
{
    struct Tdep_Print_Entry_Tag *next;
    char                         buffer[PRINT_MAX];
} Tdep_Print_Entry;

typedef struct DT_Tdep_Print_Head_Tag
{
    int			 	   instance;
    struct DT_Tdep_Print_Head_Tag *next;
    Tdep_Print_Entry    	   *head;
    Tdep_Print_Entry    	   *tail;
    DT_Mdep_LockType       	   lock;
    DT_WAIT_OBJECT       	   wait_object;
} DT_Tdep_Print_Head;


void DT_Tdep_PT_Printf (DT_Tdep_Print_Head *phead, const char * fmt, ...);

#else

typedef void * DT_Tdep_Print_Head;
void DT_Tdep_PT_Printf (DT_Tdep_Print_Head *phead, const char * fmt, ...);

#endif
#endif
