
/**
 * \file pgusage.c
 *
 * Time-stamp:      "2011-03-25 17:54:41 bkorb"
 *
 *   Automated Options Paged Usage module.
 *
 *  This routine will run run-on options through a pager so the
 *  user may examine, print or edit them at their leisure.
 *
 *  This file is part of AutoOpts, a companion to AutoGen.
 *  AutoOpts is free software.
 *  AutoOpts is Copyright (c) 1992-2011 by Bruce Korb - all rights reserved
 *
 *  AutoOpts is available under any one of two licenses.  The license
 *  in use must be one of these two and the choice is under the control
 *  of the user of the license.
 *
 *   The GNU Lesser General Public License, version 3 or later
 *      See the files "COPYING.lgplv3" and "COPYING.gplv3"
 *
 *   The Modified Berkeley Software Distribution License
 *      See the file "COPYING.mbsd"
 *
 *  These files have the following md5sums:
 *
 *  43b91e8ca915626ed3818ffb1b71248b pkg/libopts/COPYING.gplv3
 *  06a1a2e4760c90ea5e1dad8dfaac4d39 pkg/libopts/COPYING.lgplv3
 *  66a5cedaf62c4b2637025f049f9b826f pkg/libopts/COPYING.mbsd
 */

/*=export_func  optionPagedUsage
 * private:
 *
 * what:  Decipher a boolean value
 * arg:   + tOptions* + pOpts    + program options descriptor +
 * arg:   + tOptDesc* + pOptDesc + the descriptor for this arg +
 *
 * doc:
 *  Run the usage output through a pager.
 *  This is very handy if it is very long.
 *  This is disabled on platforms without a working fork() function.
=*/
void
optionPagedUsage(tOptions* pOptions, tOptDesc* pOD)
{
#if ! defined(HAVE_WORKING_FORK)
    if ((pOD->fOptState & OPTST_RESET) != 0)
        return;

    (*pOptions->pUsageProc)(pOptions, EXIT_SUCCESS);
#else
    static pid_t     my_pid;
    char zPageUsage[ 1024 ];

    /*
     *  IF we are being called after the usage proc is done
     *     (and thus has called "exit(2)")
     *  THEN invoke the pager to page through the usage file we created.
     */
    switch (pagerState) {
    case PAGER_STATE_INITIAL:
    {
        if ((pOD->fOptState & OPTST_RESET) != 0)
            return;

        my_pid  = getpid();
#ifdef HAVE_SNPRINTF
        snprintf(zPageUsage, sizeof(zPageUsage), "/tmp/use.%lu", (tAoUL)my_pid);
#else
        sprintf(zPageUsage, "/tmp/use.%lu", (tAoUL)my_pid);
#endif
        unlink(zPageUsage);

        /*
         *  Set usage output to this temporary file
         */
        option_usage_fp = fopen(zPageUsage, "w" FOPEN_BINARY_FLAG);
        if (option_usage_fp == NULL)
            _exit(EXIT_FAILURE);

        pagerState = PAGER_STATE_READY;

        /*
         *  Set up so this routine gets called during the exit logic
         */
        atexit((void(*)(void))optionPagedUsage);

        /*
         *  The usage procedure will now put the usage information into
         *  the temporary file we created above.
         */
        (*pOptions->pUsageProc)(pOptions, EXIT_SUCCESS);

        /* NOTREACHED */
        _exit(EXIT_FAILURE);
    }

    case PAGER_STATE_READY:
    {
        tSCC zPage[]  = "%1$s /tmp/use.%2$lu ; rm -f /tmp/use.%2$lu";
        tCC* pzPager  = (tCC*)getenv("PAGER");

        /*
         *  Use the "more(1)" program if "PAGER" has not been defined
         */
        if (pzPager == NULL)
            pzPager = "more";

        /*
         *  Page the file and remove it when done.
         */
#ifdef HAVE_SNPRINTF
        snprintf(zPageUsage, sizeof(zPageUsage), zPage, pzPager, (tAoUL)my_pid);
#else
        sprintf(zPageUsage, zPage, pzPager, (tAoUL)my_pid);
#endif
        fclose(stderr);
        dup2(STDOUT_FILENO, STDERR_FILENO);

        (void)system(zPageUsage);
    }

    case PAGER_STATE_CHILD:
        /*
         *  This is a child process used in creating shell script usage.
         */
        break;
    }
#endif
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/pgusage.c */
