
/**
 * \file numeric.c
 *
 *  Time-stamp:      "2011-03-25 16:26:10 bkorb"
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

/*=export_func  optionShowRange
 * private:
 *
 * what:  
 * arg:   + tOptions* + pOpts     + program options descriptor  +
 * arg:   + tOptDesc* + pOptDesc  + the descriptor for this arg +
 * arg:   + void *    + rng_table + the value range tables      +
 * arg:   + int       + rng_count + the number of entries       +
 *
 * doc:
 *   Show information about a numeric option with range constraints.
=*/
void
optionShowRange(tOptions* pOpts, tOptDesc* pOD, void * rng_table, int rng_ct)
{
    static char const bullet[] = "\t\t\t\t- ";
    static char const deepin[] = "\t\t\t\t  ";
    static char const onetab[] = "\t";

    const struct {long const rmin, rmax;} * rng = rng_table;

    char const * pz_indent    = bullet;

    /*
     * The range is shown only for full usage requests and an error
     * in this particular option.
     */
    if (pOpts != OPTPROC_EMIT_USAGE) {
        if (pOpts <= OPTPROC_EMIT_LIMIT)
            return;
        pz_indent = onetab;

        fprintf(option_usage_fp, zRangeErr, pOpts->pzProgName,
                pOD->pz_Name, pOD->optArg.argString);
        pz_indent = "";
    }

    if (pOD->fOptState & OPTST_SCALED_NUM)
        fprintf(option_usage_fp, zRangeScaled, pz_indent);

    fprintf(option_usage_fp, (rng_ct > 1) ? zRangeLie : zRangeOnly, pz_indent);
    pz_indent = (pOpts != OPTPROC_EMIT_USAGE) ? onetab : deepin;

    for (;;) {
        if (rng->rmax == LONG_MIN)
            fprintf(option_usage_fp, zRangeExact, pz_indent, rng->rmin);
        else if (rng->rmin == LONG_MIN)
            fprintf(option_usage_fp, zRangeUpto, pz_indent, rng->rmax);
        else if (rng->rmax == LONG_MAX)
            fprintf(option_usage_fp, zRangeAbove, pz_indent, rng->rmin);
        else
            fprintf(option_usage_fp, zRange, pz_indent, rng->rmin,
                    rng->rmax);

        if  (--rng_ct <= 0) {
            fputc('\n', option_usage_fp);
            break;
        }
        fputs(zRangeOr, option_usage_fp);
        rng++;
    }

    if (pOpts > OPTPROC_EMIT_LIMIT)
        pOpts->pUsageProc(pOpts, EXIT_FAILURE);
}

/*=export_func  optionNumericVal
 * private:
 *
 * what:  process an option with a numeric value.
 * arg:   + tOptions* + pOpts    + program options descriptor +
 * arg:   + tOptDesc* + pOptDesc + the descriptor for this arg +
 *
 * doc:
 *  Decipher a numeric value.
=*/
void
optionNumericVal(tOptions* pOpts, tOptDesc* pOD )
{
    char* pz;
    long  val;

    /*
     *  Numeric options may have a range associated with it.
     *  If it does, the usage procedure requests that it be
     *  emitted by passing a NULL pOD pointer.  Also bail out
     *  if there is no option argument or if we are being reset.
     */
    if (  (pOD == NULL)
       || (pOD->optArg.argString == NULL)
       || ((pOD->fOptState & OPTST_RESET) != 0))
        return;

    errno = 0;
    val = strtol(pOD->optArg.argString, &pz, 0);
    if ((pz == pOD->optArg.argString) || (errno != 0))
        goto bad_number;

    if ((pOD->fOptState & OPTST_SCALED_NUM) != 0)
        switch (*(pz++)) {
        case '\0': pz--; break;
        case 't':  val *= 1000;
        case 'g':  val *= 1000;
        case 'm':  val *= 1000;
        case 'k':  val *= 1000; break;

        case 'T':  val *= 1024;
        case 'G':  val *= 1024;
        case 'M':  val *= 1024;
        case 'K':  val *= 1024; break;

        default:   goto bad_number;
        }

    if (*pz != NUL)
        goto bad_number;

    if (pOD->fOptState & OPTST_ALLOC_ARG) {
        AGFREE(pOD->optArg.argString);
        pOD->fOptState &= ~OPTST_ALLOC_ARG;
    }

    pOD->optArg.argInt = val;
    return;

    bad_number:

    fprintf( stderr, zNotNumber, pOpts->pzProgName, pOD->optArg.argString );
    if ((pOpts->fOptSet & OPTPROC_ERRSTOP) != 0)
        (*(pOpts->pUsageProc))(pOpts, EXIT_FAILURE);

    errno = EINVAL;
    pOD->optArg.argInt = ~0;
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/numeric.c */
