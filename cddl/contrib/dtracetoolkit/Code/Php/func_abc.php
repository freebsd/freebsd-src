<?php
function func_c()
{
	echo "Function C\n";
	sleep(1);
}

function func_b()
{
	echo "Function B\n";
	sleep(1);
	func_c();
}

function func_a()
{
	echo "Function A\n";
	sleep(1);
	func_b();
}

func_a();
?>
