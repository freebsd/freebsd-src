echo Setting up the environment for debugging gdb.\n

set complaints 1

b fatal

b info_command
commands
	silent
	return
end

dir ../mmalloc
dir ../libiberty
dir ../bfd
set prompt (top-gdb) 
