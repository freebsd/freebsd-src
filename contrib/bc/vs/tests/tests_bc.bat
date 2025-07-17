@echo off

set scripts=..\..\tests\bc
set bc=%~dp0\bc.exe
set args=-ql

del /f /q *.txt > NUL


rem excluded: all, errors, read_errors, posix_errors, misc6, misc7, recursive_arrays

for %%i in (
abs
add
arctangent
arrays
assignments
bitfuncs
boolean
comp
cosine
decimal
divide
divmod
engineering
exponent
functions
globals
length
letters
lib2
log
misc
misc1
misc2
misc3
misc4
misc5
modexp
modulus
multiply
pi
places
power
print2
rand
read
scale
scientific
shift
sine
sqrt
stdin
stdin1
stdin2
strings
subtract
trunc
vars
void
leadingzero
) do (
if exist "%scripts%\%%i.txt" (
	"%bc%" "%args%" < "%scripts%\%%i.txt" > "%%i_results.txt"
	
	if errorlevel 1 (
		echo FAIL_RUNTIME: %%i
		goto :eof
	)
	
	fc.exe "%scripts%\%%i_results.txt" "%%i_results.txt" > NUL
	
	if errorlevel 1 (
		echo FAIL_RESULTS: %%i
		goto :eof
	)

	echo PASS: %%i
) else (
	echo FAIL_NOT_EXIST: %%i
	goto :eof
)
)

if exist "%scripts%\leadingzero.txt" (
	"%bc%" "%args%" -z < "%scripts%\leadingzero.txt" > "leadingzero_z_results.txt"

	if errorlevel 1 (
		echo FAIL_RUNTIME: leadingzero_z
		goto :eof
	)

	fc.exe "%scripts%\leadingzero_results.txt" "leadingzero_z_results.txt" > NUL

	if errorlevel 1 (
		echo FAIL_RESULTS: leadingzero_z
		goto :eof
	)

	echo PASS: leadingzero_z
) else (
	echo FAIL_NOT_EXIST: leadingzero_z
	goto :eof
)