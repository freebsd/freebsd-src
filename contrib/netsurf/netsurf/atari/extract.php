#!/usr/bin/php
<?
$lines = file("deskmenu.c");

foreach($lines as $line){
	if(stripos($line, "static void __CDECL menu_") === 0){
		echo $line;
	}
}
?>
