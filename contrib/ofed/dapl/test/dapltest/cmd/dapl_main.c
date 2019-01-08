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

#include "dapl_proto.h"

/* Main Entry Point */
int main(int argc, char *argv[])
{
	return (dapltest(argc, argv));
}

/*
 * dapltest main program
 */
int dapltest(int argc, char *argv[])
{
	Params_t *params_ptr;
	DAT_RETURN rc = DAT_SUCCESS;

	/* check memory leaking */
	/*
	 * DT_Mdep_LockInit(&Alloc_Count_Lock); alloc_count = 0;
	 */

#if defined(_WIN32) || defined(_WIN64)
	{
		/* Cannot be done from DT_Mdep_Init as dapl_init makes some socket
		 * calls....So need to do this before calling dapl_init */
		WSADATA wsaData;
		int i;

		i = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (i != 0) {
			printf("%s WSAStartup(2.2) failed? (0x%x)\n", argv[0],
			       i);
			exit(1);
		}
	}
#endif

#ifdef GPROF
	{
		extern void dapl_init(void);
		dapl_init();
	}
#endif
	DT_dapltest_debug = 0;

	params_ptr = (Params_t *) DT_Mdep_Malloc(sizeof(Params_t));
	if (!params_ptr) {
		DT_Mdep_printf("Cannot allocate memory for Params structure\n");
		return (1);
	}

	DT_Tdep_Init();		/* init (kdapl/udapl)test-dependent code */
	DT_Endian_Init();	/* init endian of local machine */
	DT_Mdep_Init();		/* init OS, libraries, etc.     */

	params_ptr->local_is_little_endian = DT_local_is_little_endian;

	/*
	 * parse command line arguments
	 */
	if (!DT_Params_Parse(argc, argv, params_ptr)) {
		DT_Mdep_printf("Command line syntax error\n");
		return 1;
	}
	params_ptr->cpu_mhz = DT_Mdep_GetCpuMhz();
	/* call the test-dependent code for invoking the actual test */
	rc = DT_Tdep_Execute_Test(params_ptr);

	/* cleanup */

	DT_Mdep_End();
	DT_Mdep_Free(params_ptr);
	DT_Tdep_End();
#ifdef GPROF
	{
		extern void dapl_fini(void);
		dapl_fini();
	}
#endif

	/*
	 * check memory leaking DT_Mdep_printf("App allocated Memory left: %d\n",
	 * alloc_count); DT_Mdep_LockDestroy(&Alloc_Count_Lock);
	 */

	return (rc);
}

void Dapltest_Main_Usage(void)
{
	DT_Mdep_printf("USAGE:\n");
	DT_Mdep_printf
	    ("USAGE:     dapltest -T <Test_Type> [-D IA_name] [test-specific args]\n");
	DT_Mdep_printf("USAGE:         where <Test_Type>\n");
	DT_Mdep_printf("USAGE:         S = Run as a server\n");
	DT_Mdep_printf("USAGE:         T = Transaction Test\n");
	DT_Mdep_printf("USAGE:         Q = Quit Test\n");
	DT_Mdep_printf("USAGE:         P = Performance Test\n");
	DT_Mdep_printf("USAGE:         L = Limit Test\n");
	DT_Mdep_printf("USAGE:         F = FFT Test\n");
	DT_Mdep_printf("USAGE:\n");
	DT_Mdep_printf
	    ("USAGE:         -D Interface_Adapter {default ibnic0v2}\n");
	DT_Mdep_printf("USAGE:\n");
	DT_Mdep_printf
	    ("NOTE:\tRun as server taking defaults (dapltest -T S [-D ibnic0v2])\n");
	DT_Mdep_printf("NOTE:         dapltest\n");
	DT_Mdep_printf("NOTE:\n");
	DT_Mdep_printf
	    ("NOTE:\tdapltest arguments may be supplied in a script file\n");
	DT_Mdep_printf("NOTE:\tdapltest -f <script_file>\n");
	DT_Mdep_printf("USAGE:\n");
}
