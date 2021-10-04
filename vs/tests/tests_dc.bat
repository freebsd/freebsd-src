@echo off

set scripts=..\..\tests\dc
set dc=%~dp0\dc.exe
set args=-x

del /f /q *.txt > NUL


rem excluded: all, errors, read_errors

for %%i in (
abs
add
boolean
decimal
divide
divmod
engineering
exec_stack_len
length
misc
modexp
modulus
multiply
negate
places
power
rand
read
scientific
shift
sqrt
stack_len
stdin
strings
subtract
trunc
vars
) do (
if exist "%scripts%\%%i.txt" (
	"%dc%" "%args%" < "%scripts%\%%i.txt" > "%%i_results.txt"
	
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