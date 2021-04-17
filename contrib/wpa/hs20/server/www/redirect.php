<?php

require('config.php');

$db = new PDO($osu_db);
if (!$db) {
   die($sqliteerror);
}

if (isset($_GET["id"]))
	$id = preg_replace("/[^a-fA-F0-9]/", "", $_GET["id"]);
else
	$id = 0;

$row = $db->query("SELECT rowid,* FROM sessions WHERE id='$id'")->fetch();
if ($row == false) {
   die("Session not found");
}

$uri = $row['redirect_uri'];

header("Location: $uri", true, 302);

$user = $row['user'];
$realm = $row['realm'];

$db->exec("INSERT INTO eventlog(user,realm,sessionid,timestamp,notes) " .
	  "VALUES ('$user', '$realm', '$id', " .
	  "strftime('%Y-%m-%d %H:%M:%f','now'), " .
	  "'redirected after user input')");

?>
