
/*
 * \file save.c
 *
 *  This module's routines will take the currently set options and
 *  store them into an ".rc" file for re-interpretation the next
 *  time the invoking program is run.
 *
 * @addtogroup autoopts
 * @{
 */
/*
 *  This file is part of AutoOpts, a companion to AutoGen.
 *  AutoOpts is free software.
 *  AutoOpts is Copyright (C) 1992-2018 by Bruce Korb - all rights reserved
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
 *  These files have the following sha256 sums:
 *
 *  8584710e9b04216a394078dc156b781d0b47e1729104d666658aecef8ee32e95  COPYING.gplv3
 *  4379e7444a0e2ce2b12dd6f5a52a27a4d02d39d247901d3285c88cf0d37f477b  COPYING.lgplv3
 *  13aa749a5b0a454917a944ed8fffc530b784f5ead522b1aacaf4ec8aa55a6239  COPYING.mbsd
 */
#include "save-flags.h"

/**
 * find the config file directory name
 *
 * @param opts    the options descriptor
 * @param p_free  tell caller if name was allocated or not
 */
static char const *
find_dir_name(tOptions * opts, int * p_free)
{
    char const * dir;

    if (  (opts->specOptIdx.save_opts == NO_EQUIVALENT)
       || (opts->specOptIdx.save_opts == 0))
        return NULL;

    dir = opts->pOptDesc[ opts->specOptIdx.save_opts ].optArg.argString;
    if ((dir != NULL) && (*dir != NUL)) {
        char const * pz = strchr(dir, '>');
        if (pz == NULL)
            return dir;
        while (*(++pz) == '>')  ;
        pz += strspn(pz, " \t");
        dir = pz;
        if (*dir != NUL)
            return dir;
    }

    if (opts->papzHomeList == NULL)
        return NULL;

    /*
     *  This function only works if there is a directory where
     *  we can stash the RC (INI) file.
     */
    for (int idx = 0;; idx++) {
        char f_name[ AG_PATH_MAX+1 ];

        dir = opts->papzHomeList[idx];

        switch (*dir) {
        case '$':
            break;
        case NUL:
            continue;
        default:
            return dir;
        }
        if (optionMakePath(f_name, (int)sizeof(f_name), dir, opts->pzProgPath)) {
            *p_free = true;
            AGDUPSTR(dir, f_name, "homerc");
            return dir;
        }
    }
    return NULL;
}

/**
 * Find the name of the save-the-options file
 *
 * @param opts         the options descriptor
 * @param p_free_name  tell caller if name was allocated or not
 */
static char const *
find_file_name(tOptions * opts, int * p_free_name)
{
    struct stat stBuf;
    int    free_dir_name = 0;

    char const * res = find_dir_name(opts, &free_dir_name);
    if (res == NULL)
        return res;

    /*
     *  See if we can find the specified directory.  We use a once-only loop
     *  structure so we can bail out early.
     */
    if (stat(res, &stBuf) != 0) do {
        char z[AG_PATH_MAX];
        char * dirchp;

        /*
         *  IF we could not, check to see if we got a full
         *  path to a file name that has not been created yet.
         */
        if (errno != ENOENT) {
        bogus_name:
            fprintf(stderr, zsave_warn, opts->pzProgName, res);
            fprintf(stderr, zNoStat, errno, strerror(errno), res);
            if (free_dir_name)
                AGFREE(res);
            return NULL;
        }

        /*
         *  Strip off the last component, stat the remaining string and
         *  that string must name a directory
         */
        dirchp = strrchr(res, DIRCH);
        if (dirchp == NULL) {
            stBuf.st_mode = S_IFREG;
            break; /* found directory -- viz.,  "." */
        }

        if ((size_t)(dirchp - res) >= sizeof(z))
            goto bogus_name;

        memcpy(z, res, (size_t)(dirchp - res));
        z[dirchp - res] = NUL;

        if ((stat(z, &stBuf) != 0) || ! S_ISDIR(stBuf.st_mode))
            goto bogus_name;
        stBuf.st_mode = S_IFREG; /* file within this directory */
    } while (false);

    /*
     *  IF what we found was a directory,
     *  THEN tack on the config file name
     */
    if (S_ISDIR(stBuf.st_mode)) {

        {
            size_t sz = strlen(res) + strlen(opts->pzRcName) + 2;
            char * pzPath = (char *)AGALOC(sz, "file name");
            if (   snprintf(pzPath, sz, "%s/%s", res, opts->pzRcName)
                >= (int)sz)
                option_exits(EXIT_FAILURE);

            if (free_dir_name)
                AGFREE(res);
            res = pzPath;
            free_dir_name = 1;
        }

        /*
         *  IF we cannot stat the object for any reason other than
         *     it does not exist, then we bail out
         */
        if (stat(res, &stBuf) != 0) {
            if (errno != ENOENT) {
                fprintf(stderr, zsave_warn, opts->pzProgName, res);
                fprintf(stderr, zNoStat, errno, strerror(errno),
                        res);
                AGFREE(res);
                return NULL;
            }

            /*
             *  It does not exist yet, but it will be a regular file
             */
            stBuf.st_mode = S_IFREG;
        }
    }

    /*
     *  Make sure that whatever we ultimately found, that it either is
     *  or will soon be a file.
     */
    if (! S_ISREG(stBuf.st_mode)) {
        fprintf(stderr, zsave_warn, opts->pzProgName, res);
        if (free_dir_name)
            AGFREE(res);
        return NULL;
    }

    /*
     *  Get rid of the old file
     */
    *p_free_name = free_dir_name;
    return res;
}

/**
 * print one option entry to the save file.
 *
 * @param[in] fp       the file pointer for the save file
 * @param[in] od       the option descriptor to print
 * @param[in] l_arg    the last argument for the option
 * @param[in] save_fl  include usage in comments
 */
static void
prt_entry(FILE * fp, tOptDesc * od, char const * l_arg, save_flags_mask_t save_fl)
{
    int space_ct;

    if (save_fl & SVFL_USAGE)
        fprintf(fp, ao_name_use_fmt, od->pz_Name, od->pzText);
    if (UNUSED_OPT(od) && (save_fl & SVFL_DEFAULT))
        fputs(ao_default_use, fp);

    /*
     *  There is an argument.  Pad the name so values line up.
     *  Not disabled *OR* this got equivalenced to another opt,
     *  then use current option name.
     *  Otherwise, there must be a disablement name.
     */
    {
        char const * pz =
            (od->pz_DisableName == NULL)
            ? od->pz_Name
            : (DISABLED_OPT(od)
               ? od->pz_DisableName
               : ((od->optEquivIndex == NO_EQUIVALENT)
                  ? od->pz_Name : od->pz_DisableName)
              );
        
        space_ct = 17 - strlen(pz);
        fputs(pz, fp);
    }

    if (  (l_arg == NULL)
       && (OPTST_GET_ARGTYPE(od->fOptState) != OPARG_TYPE_NUMERIC))
        goto end_entry;

    fputs(" = ", fp);
    while (space_ct-- > 0)  fputc(' ', fp);

    /*
     *  IF the option is numeric only,
     *  THEN the char pointer is really the number
     */
    if (OPTST_GET_ARGTYPE(od->fOptState) == OPARG_TYPE_NUMERIC)
        fprintf(fp, "%d", (int)(intptr_t)l_arg);

    else {
        for (;;) {
            char const * eol = strchr(l_arg, NL);

            /*
             *  IF this is the last line
             *  THEN bail and print it
             */
            if (eol == NULL)
                break;

            /*
             *  Print the continuation and the text from the current line
             */
            (void)fwrite(l_arg, (size_t)(eol - l_arg), (size_t)1, fp);
            l_arg = eol+1; /* advance the Last Arg pointer */
            fputs("\\\n", fp);
        }

        /*
         *  Terminate the entry
         */
        fputs(l_arg, fp);
    }

end_entry:
    fputc(NL, fp);
}

/**
 * print an option's value
 *
 * @param[in] fp          the file pointer for the save file
 * @param[in] od          the option descriptor to print
 */
static void
prt_value(FILE * fp, int depth, tOptDesc * od, tOptionValue const * ovp)
{
    while (--depth >= 0)
        putc(' ', fp), putc(' ', fp);

    switch (ovp->valType) {
    default:
    case OPARG_TYPE_NONE:
        fprintf(fp, NULL_ATR_FMT, ovp->pzName);
        break;

    case OPARG_TYPE_STRING:
        prt_string(fp, ovp->pzName, ovp->v.strVal);
        break;

    case OPARG_TYPE_ENUMERATION:
    case OPARG_TYPE_MEMBERSHIP:
        if (od != NULL) {
            uint32_t  opt_state = od->fOptState;
            uintptr_t val = od->optArg.argEnum;
            char const * typ = (ovp->valType == OPARG_TYPE_ENUMERATION)
                ? "keyword" : "set-membership";

            fprintf(fp, TYPE_ATR_FMT, ovp->pzName, typ);

            /*
             *  This is a magic incantation that will convert the
             *  bit flag values back into a string suitable for printing.
             */
            (*(od->pOptProc))(OPTPROC_RETURN_VALNAME, od );
            if (od->optArg.argString != NULL) {
                fputs(od->optArg.argString, fp);

                if (ovp->valType != OPARG_TYPE_ENUMERATION) {
                    /*
                     *  set membership strings get allocated
                     */
                    AGFREE(od->optArg.argString);
                }
            }

            od->optArg.argEnum = val;
            od->fOptState = opt_state;
            fprintf(fp, END_XML_FMT, ovp->pzName);
            break;
        }
        /* FALLTHROUGH */

    case OPARG_TYPE_NUMERIC:
        fprintf(fp, NUMB_ATR_FMT, ovp->pzName, ovp->v.longVal);
        break;

    case OPARG_TYPE_BOOLEAN:
        fprintf(fp, BOOL_ATR_FMT, ovp->pzName,
                ovp->v.boolVal ? "true" : "false");
        break;

    case OPARG_TYPE_HIERARCHY:
        prt_val_list(fp, ovp->pzName, ovp->v.nestVal);
        break;
    }
}

/**
 * Print a string value in XML format
 *
 * @param[in] fp          the file pointer for the save file
 */
static void
prt_string(FILE * fp, char const * name, char const * pz)
{
    fprintf(fp, OPEN_XML_FMT, name);
    for (;;) {
        int ch = ((int)*(pz++)) & 0xFF;

        switch (ch) {
        case NUL: goto string_done;

        case '&':
        case '<':
        case '>':
#if __GNUC__ >= 4
        case 1 ... (' ' - 1):
        case ('~' + 1) ... 0xFF:
#endif
            emit_special_char(fp, ch);
            break;

        default:
#if __GNUC__ < 4
            if (  ((ch >= 1) && (ch <= (' ' - 1)))
               || ((ch >= ('~' + 1)) && (ch <= 0xFF)) ) {
                emit_special_char(fp, ch);
                break;
            }
#endif
            putc(ch, fp);
        }
    } string_done:;
    fprintf(fp, END_XML_FMT, name);
}

/**
 * Print an option that can have multiple values in XML format
 *
 * @param[in] fp          file pointer
 */
static void
prt_val_list(FILE * fp, char const * name, tArgList * al)
{
    static int depth = 1;

    int sp_ct;
    int opt_ct;
    void ** opt_list;

    if (al == NULL)
        return;
    opt_ct   = al->useCt;
    opt_list = (void **)al->apzArgs;

    if (opt_ct <= 0) {
        fprintf(fp, OPEN_CLOSE_FMT, name);
        return;
    }

    fprintf(fp, NESTED_OPT_FMT, name);

    depth++;
    while (--opt_ct >= 0) {
        tOptionValue const * ovp = *(opt_list++);

        prt_value(fp, depth, NULL, ovp);
    }
    depth--;

    for (sp_ct = depth; --sp_ct >= 0;)
        putc(' ', fp), putc(' ', fp);
    fprintf(fp, "</%s>\n", name);
}

/**
 * printed a nested/hierarchical value
 *
 * @param[in] fp       file pointer
 * @param[in] od       option descriptor
 * @param[in] save_fl  include usage in comments
 */
static void
prt_nested(FILE * fp, tOptDesc * od, save_flags_mask_t save_fl)
{
    int opt_ct;
    tArgList * al = od->optCookie;
    void ** opt_list;

    if (save_fl & SVFL_USAGE)
        fprintf(fp, ao_name_use_fmt, od->pz_Name, od->pzText);

    /*
     * Never show a default value if a hierarchical value is empty.
     */
    if (UNUSED_OPT(od) || (al == NULL))
        return;

    opt_ct   = al->useCt;
    opt_list = (void **)al->apzArgs;

    if (opt_ct <= 0)
        return;

    do  {
        tOptionValue const * base = *(opt_list++);
        tOptionValue const * ovp = optionGetValue(base, NULL);

        if (ovp == NULL)
            continue;

        fprintf(fp, NESTED_OPT_FMT, od->pz_Name);

        do  {
            prt_value(fp, 1, od, ovp);

        } while (ovp = optionNextValue(base, ovp),
                 ovp != NULL);

        fprintf(fp, "</%s>\n", od->pz_Name);
    } while (--opt_ct > 0);
}

#ifdef _MSC_VER
/**
 * truncate() emulation for Microsoft C
 *
 * @param[in] fname  the save file name
 * @param[in] newsz  new size of fname in octets
 */
static int
truncate(char const* fname, size_t newsz)
{
    int fd;
    int err;

    fd = open(fname, O_RDWR);
    if (fd < 0)
            return fd;
    err = _chsize_s(fd, newsz);
    close(fd);
    if (0 != err)
            errno = err;
    return err;
}
#endif /* _MSC_VER */

/**
 * remove the current program settings
 *
 * @param[in] opts  the program options structure
 * @param[in] fname the save file name
 */
static void
remove_settings(tOptions * opts, char const * fname)
{
    size_t const name_len = strlen(opts->pzProgName);
    tmap_info_t  map_info;
    char *       text = text_mmap(fname, PROT_READ|PROT_WRITE, MAP_PRIVATE, &map_info);
    char *       scan = text;

    for (;;) {
        char * next = scan = strstr(scan, zCfgProg);
        if (scan == NULL)
            goto leave;

        scan = SPN_WHITESPACE_CHARS(scan + zCfgProg_LEN);
        if (  (strneqvcmp(scan, opts->pzProgName, (int)name_len) == 0)
           && (IS_END_XML_TOKEN_CHAR(scan[name_len])) )  {

            scan = next;
            break;
        }
    }

    /*
     * If not NULL, "scan" points to the "<?program" string introducing
     * the program segment we are to remove. See if another segment follows.
     * If so, copy text. If not se trim off this segment.
     */
    {
        char * next = strstr(scan + zCfgProg_LEN, zCfgProg);
        size_t new_sz;

        if (next == NULL)
            new_sz = map_info.txt_size - strlen(scan);
        else {
            int fd = open(fname, O_RDWR);
            if (fd < 0) return;
            if (lseek(fd, (scan - text), SEEK_SET) < 0)
                scan = next;
            else if (write(fd, next, strlen(next)) < 0)
                scan = next;
            if (close(fd) < 0)
                scan = next;
            new_sz = map_info.txt_size - (next - scan);
        }
        if (new_sz != map_info.txt_size)
            if (truncate(fname, new_sz) < 0)
                scan = next; // we removed it, so shorten file
    }

 leave:
    text_munmap(&map_info);
}

/**
 * open the file for saving option state.
 *
 * @param[in] opts     the program options structure
 * @param[in] save_fl  flags for saving data
 * @returns the open file pointer.  It may be NULL.
 */
static FILE *
open_sv_file(tOptions * opts, save_flags_mask_t save_fl)
{
    FILE * fp;

    {
        int   free_name = 0;
        char const * fname = find_file_name(opts, &free_name);
        if (fname == NULL)
            return NULL;

        if (save_fl == 0)
            unlink(fname);
        else
            remove_settings(opts, fname);

        fp = fopen(fname, "a" FOPEN_BINARY_FLAG);
        if (fp == NULL) {
            fprintf(stderr, zsave_warn, opts->pzProgName, fname);
            fprintf(stderr, zNoCreat, errno, strerror(errno), fname);
            if (free_name)
                AGFREE(fname);
            return fp;
        }

        if (free_name)
            AGFREE(fname);
    }

    do {
        struct stat sbuf;
        if (fstat(fileno(fp), &sbuf) < 0)
            break;

        if (sbuf.st_size > zPresetFile_LEN) {
            /* non-zero size implies save_fl is non-zero */
            fprintf(fp, zFmtProg, opts->pzProgName);
            return fp;
        }
    } while (false);

    /*
     * We have a new file. Insert a header
     */
    fputs("#  ", fp);
    {
        char const * e = strchr(opts->pzUsageTitle, NL);
        if (e++ != NULL)
            fwrite(opts->pzUsageTitle, 1, e - opts->pzUsageTitle, fp);
    }

    {
        time_t  cur_time = time(NULL);
        char *  time_str = ctime(&cur_time);

        fprintf(fp, zPresetFile, time_str);
#ifdef HAVE_ALLOCATED_CTIME
        /*
         *  The return values for ctime(), localtime(), and gmtime()
         *  normally point to static data that is overwritten by each call.
         *  The test to detect allocated ctime, so we leak the memory.
         */
        AGFREE(time_str);
#endif
    }
    if (save_fl != 0)
        fprintf(fp, zFmtProg, opts->pzProgName);
    return fp;
}

/**
 * print option without an arg
 *
 * @param[in] fp       file pointer
 * @param[in] vod      value option descriptor
 * @param[in] pod      primary option descriptor
 * @param[in] save_fl  include usage in comments
 */
static void
prt_no_arg_opt(FILE * fp, tOptDesc * vod, tOptDesc * pod, save_flags_mask_t save_fl)
{
    /*
     * The aliased to argument indicates whether or not the option
     * is "disabled".  However, the original option has the name
     * string, so we get that there, not with "vod".
     */
    char const * pznm =
        (DISABLED_OPT(vod)) ? pod->pz_DisableName : pod->pz_Name;
    /*
     *  If the option was disabled and the disablement name is NULL,
     *  then the disablement was caused by aliasing.
     *  Use the name as the string to emit.
     */
    if (pznm == NULL)
        pznm = pod->pz_Name;

    if (save_fl & SVFL_USAGE)
        fprintf(fp, ao_name_use_fmt, pod->pz_Name, pod->pzText);
    if (UNUSED_OPT(pod) && (save_fl & SVFL_DEFAULT))
        fputs(ao_default_use, fp);

    fprintf(fp, "%s\n", pznm);
}

/**
 * print the string valued argument(s).
 *
 * @param[in] fp       file pointer
 * @param[in] od       value option descriptor
 * @param[in] save_fl  include usage in comments
 */
static void
prt_str_arg(FILE * fp, tOptDesc * od, save_flags_mask_t save_fl)
{
    if (UNUSED_OPT(od) || ((od->fOptState & OPTST_STACKED) == 0)) {
        char const * arg = od->optArg.argString;
        if (arg == NULL)
            arg = "''";
        prt_entry(fp, od, arg, save_fl);

    } else {
        tArgList * pAL = (tArgList *)od->optCookie;
        int        uct = pAL->useCt;
        char const ** ppz = pAL->apzArgs;

        /*
         *  un-disable multiple copies of disabled options.
         */
        if (uct > 1)
            od->fOptState &= ~OPTST_DISABLED;

        while (uct-- > 0) {
            prt_entry(fp, od, *(ppz++), save_fl);
            save_fl &= ~SVFL_USAGE;
        }
    }
}

/**
 * print the string value of an enumeration.
 *
 * @param[in] fp       the file pointer to write to
 * @param[in] od       the option descriptor with the enumerated value
 * @param[in] save_fl  include usage in comments
 */
static void
prt_enum_arg(FILE * fp, tOptDesc * od, save_flags_mask_t save_fl)
{
    uintptr_t val = od->optArg.argEnum;

    /*
     *  This is a magic incantation that will convert the
     *  bit flag values back into a string suitable for printing.
     */
    (*(od->pOptProc))(OPTPROC_RETURN_VALNAME, od);
    prt_entry(fp, od, VOIDP(od->optArg.argString), save_fl);

    od->optArg.argEnum = val;
}

/**
 * Print the bits set in a bit mask option.
 *
 * We call the option handling function with a magic value for
 * the options pointer and it allocates and fills in the string.
 * We print that with a call to prt_entry().
 *
 * @param[in] fp       the file pointer to write to
 * @param[in] od       the option descriptor with a bit mask value type
 * @param[in] save_fl  include usage in comments
 */
static void
prt_set_arg(FILE * fp, tOptDesc * od, save_flags_mask_t save_fl)
{
    char * list = optionMemberList(od);
    size_t len  = strlen(list);
    char * buf  = (char *)AGALOC(len + 3, "dir name");
    *buf= '=';
    memcpy(buf+1, list, len + 1);
    prt_entry(fp, od, buf, save_fl);
    AGFREE(buf);
    AGFREE(list);
}

/**
 * figure out what the option file name argument is.
 * If one can be found, call prt_entry() to emit it.
 *
 * @param[in] fp       the file pointer to write to.
 * @param[in] od       the option descriptor with a bit mask value type
 * @param[in] opts     the program options descriptor
 * @param[in] save_fl  include usage in comments
 */
static void
prt_file_arg(FILE * fp, tOptDesc * od, tOptions * opts, save_flags_mask_t save_fl)
{
    /*
     *  If the cookie is not NULL, then it has the file name, period.
     *  Otherwise, if we have a non-NULL string argument, then....
     */
    if (od->optCookie != NULL)
        prt_entry(fp, od, od->optCookie, save_fl);

    else if (HAS_originalOptArgArray(opts)) {
        char const * orig =
            opts->originalOptArgArray[od->optIndex].argString;

        if (od->optArg.argString == orig) {
            if (save_fl)
                fprintf(fp, ao_name_use_fmt, od->pz_Name, od->pzText);
            return;
        }

        prt_entry(fp, od, od->optArg.argString, save_fl);

    } else if (save_fl)
        fprintf(fp, ao_name_use_fmt, od->pz_Name, od->pzText);
}

/*=export_func  optionSaveFile
 *
 * what:  saves the option state to a file
 *
 * arg:   tOptions *,   opts,  program options descriptor
 *
 * doc:
 *
 * This routine will save the state of option processing to a file.  The name
 * of that file can be specified with the argument to the @code{--save-opts}
 * option, or by appending the @code{rcfile} attribute to the last
 * @code{homerc} attribute.  If no @code{rcfile} attribute was specified, it
 * will default to @code{.@i{programname}rc}.  If you wish to specify another
 * file, you should invoke the @code{SET_OPT_SAVE_OPTS(@i{filename})} macro.
 *
 * The recommend usage is as follows:
 * @example
 *    optionProcess(&progOptions, argc, argv);
 *    if (i_want_a_non_standard_place_for_this)
 *        SET_OPT_SAVE_OPTS("myfilename");
 *    optionSaveFile(&progOptions);
 * @end example
 *
 * err:
 *
 * If no @code{homerc} file was specified, this routine will silently return
 * and do nothing.  If the output file cannot be created or updated, a message
 * will be printed to @code{stderr} and the routine will return.
=*/
void
optionSaveFile(tOptions * opts)
{
    tOptDesc *  od;
    int         ct;
    FILE *      fp;
    save_flags_mask_t save_flags = SVFL_NONE;

    do {
        char * temp_str;
        char const * dir = opts->pOptDesc[ opts->specOptIdx.save_opts ].optArg.argString;
        size_t flen;

        if (dir == NULL)
            break;
        temp_str = strchr(dir, '>');
        if (temp_str == NULL)
            break;
        if (temp_str[1] == '>')
            save_flags = SVFL_UPDATE;
        flen = (temp_str - dir);
        if (flen == 0)
            break;
        temp_str = AGALOC(flen + 1, "flag search str");
        memcpy(temp_str, dir, flen);
        temp_str[flen] = NUL;
        save_flags |= save_flags_str2mask(temp_str, SVFL_NONE);
        AGFREE(temp_str);
    } while (false);

    fp = open_sv_file(opts, save_flags & SVFL_UPDATE);
    if (fp == NULL)
        return;

    /*
     *  FOR each of the defined options, ...
     */
    ct = opts->presetOptCt;
    od = opts->pOptDesc;
    do  {
        tOptDesc * vod;

        /*
         *  Equivalenced options get picked up when the equivalenced-to
         *  option is processed. And do not save options with any state
         *  bits in the DO_NOT_SAVE collection
         *
         * ** option cannot be preset
         * #define OPTST_NO_INIT          0x0000100U
         * ** disable from cmd line
         * #define OPTST_NO_COMMAND       0x2000000U
         * ** alias for other option
         * #define OPTST_ALIAS            0x8000000U
         */
        if ((od->fOptState & OPTST_DO_NOT_SAVE_MASK) != 0)
            continue;

        if (  (od->optEquivIndex != NO_EQUIVALENT)
           && (od->optEquivIndex != od->optIndex))
            continue;

        if (UNUSED_OPT(od) && ((save_flags & SVFL_USAGE_DEFAULT_MASK) == SVFL_NONE))
            continue;

        /*
         *  The option argument data are found at the equivalenced-to option,
         *  but the actual option argument type comes from the original
         *  option descriptor.  Be careful!
         */
        vod = ((od->fOptState & OPTST_EQUIVALENCE) != 0)
              ? (opts->pOptDesc + od->optActualIndex) : od;

        switch (OPTST_GET_ARGTYPE(od->fOptState)) {
        case OPARG_TYPE_NONE:
            prt_no_arg_opt(fp, vod, od, save_flags);
            break;

        case OPARG_TYPE_NUMERIC:
            prt_entry(fp, vod, VOIDP(vod->optArg.argInt), save_flags);
            break;

        case OPARG_TYPE_STRING:
            prt_str_arg(fp, vod, save_flags);
            break;

        case OPARG_TYPE_ENUMERATION:
            prt_enum_arg(fp, vod, save_flags);
            break;

        case OPARG_TYPE_MEMBERSHIP:
            prt_set_arg(fp, vod, save_flags);
            break;

        case OPARG_TYPE_BOOLEAN:
            prt_entry(fp, vod, vod->optArg.argBool ? "true" : "false", save_flags);
            break;

        case OPARG_TYPE_HIERARCHY:
            prt_nested(fp, vod, save_flags);
            break;

        case OPARG_TYPE_FILE:
            prt_file_arg(fp, vod, opts, save_flags);
            break;

        default:
            break; /* cannot handle - skip it */
        }
    } while (od++, (--ct > 0));

    fclose(fp);
}
/** @}
 *
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/save.c */
