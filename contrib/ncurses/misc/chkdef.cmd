/*
 * $Id: chkdef.cmd,v 1.2 1998/08/29 21:45:58 tom Exp $
 *
 * Author:  Juan Jose Garcia Ripoll <worm@arrakis.es>.
 * Webpage: http://www.arrakis.es/~worm/
 *
 * chkdef.cmd - checks that a .def file has no conflicts and is properly
 *		formatted.
 *
 * returns nonzero if two symbols have the same code or a line has a wrong
 * format.
 *
 * returns 0 otherwise
 *
 * the standard output shows conflicts.
 */
parse arg def_file

def_file = translate(def_file,'\','/')

call CleanQueue

/*
 * `cmp' is zero when the file is valid
 * `codes' associates a name to a code
 * `names' associates a code to a name
 */
cmp    = 0
codes. = 0
names. = ''

/*
 * This sed expression cleans empty lines, comments and special .DEF
 * commands, such as LIBRARY..., EXPORTS..., etc
 */
tidy_up  = '"s/[ 	][ 	]*/ /g;s/;.*//g;/^[ ]*$/d;/^[a-zA-Z]/d;"'

/*
 * First we find all public symbols from the original DLL. All this
 * information is pushed into a REXX private list with the RXQUEUE
 * utility program.
 */
'@echo off'
'type' def_file '| sed' tidy_up '| sort | rxqueue'

do while queued() > 0
   /*
    * We retrieve the symbol name (NEW_NAME) and its code (NEW_CODE)
    */
   parse pull '"' new_name '"' '@'new_code rest
   select
      when (new_code = '') | (new_name = '') then
         /* The input was not properly formatted */
         do
         say 'Error: symbol "'new_name'" has no export code or is empty'
         cmp = 1
         end
      when codes.new_name \= 0 then
         /* This symbol was already defined */
         if codes.new_name \= new_code then
            do
	    cmp = 2
 	    say 'Symbol "'new_name'" multiply defined'
	    end
      when names.new_code \= '' then
         /* This code was already assigned to a symbol */
         if names.new_code \= new_name then
            do
            cmp = 3
	    say 'Conflict with "'names.new_code'" & "'new_name'" being @'new_code
            end
      otherwise
         do
         codes.new_name = new_code
         names.new_code = new_name
         end
   end  /* select */
end

exit cmp

CleanQueue: procedure
	do while queued() > 0
	   parse pull foo
	end
return
