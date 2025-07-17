@echo off
:loop
if exist %1 del %1
shift
if not %1.==. goto loop
exit

Rem
Rem rm.bat
Rem
Rem Copyright 1995 by the Massachusetts Institute of Technology.
Rem All Rights Reserved.
Rem
Rem Export of this software from the United States of America may
Rem   require a specific license from the United States Government.
Rem   It is the responsibility of any person or organization contemplating
Rem   export to obtain such a license before exporting.
Rem 
Rem WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
Rem distribute this software and its documentation for any purpose and
Rem without fee is hereby granted, provided that the above copyright
Rem notice appear in all copies and that both that copyright notice and
Rem this permission notice appear in supporting documentation, and that
Rem the name of M.I.T. not be used in advertising or publicity pertaining
Rem to distribution of the software without specific, written prior
Rem permission.  M.I.T. makes no representations about the suitability of
Rem this software for any purpose.  It is provided "as is" without express
Rem or implied warranty.
Rem 
Rem
Rem Batch file to mimic the functionality of the Unix rm command
Rem
