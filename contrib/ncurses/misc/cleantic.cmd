/*
 * $Id: cleantic.cmd,v 1.3 1998/08/29 21:43:19 tom Exp $
 *
 * Author:  Juan Jose Garcia Ripoll <worm@arrakis.es>.
 * Webpage: http://www.arrakis.es/~worm/
 */
parse arg dir

pause
dir = translate(dir,'\','/');
letters = '0 1 2 3 4 5 6 7 8 9 a b c d e f g h i j k l m n o p q r s t u v w x y z'

if dir = '' then
    dir = '.'
'echo Cleaning 'dir
'for %%1 in ('letters') do @if not exist 'dir'\%%1\* (echo Cleaning ...\%%1 & rd %%1 2>NUL)'
