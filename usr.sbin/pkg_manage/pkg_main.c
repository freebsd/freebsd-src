/***************************************************************
 * 
 * Program:	pkg_main.c
 * Author:	Marc van Kempen
 * Desc:	main routine for pkg_manage
 *
 * Copyright (c) 1995, Marc van Kempen
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 * 
 ***************************************************************/

#include "pkg_manage.h"
#include "ui_objects.h"

extern PKG_info	p_inf;

/*
 * Main
 */

void
main(void)
{
    init_dialog();

    p_inf.Nitems = 0;	/* Initialize p_inf */

    get_pkginfo();
    run_menu();

    if (p_inf.Nitems > 0) {
	FreeInfo();
    }
 
    clear();
    dialog_update();
    end_dialog();

    return;
} /* main() */
