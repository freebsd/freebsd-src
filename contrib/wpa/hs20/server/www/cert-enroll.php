<?php

require('config.php');

$db = new PDO($osu_db);
if (!$db) {
   die($sqliteerror);
}

if (isset($_GET["id"]))
  $id = preg_replace("/[^a-fA-F0-9]/", "", $_GET["id"]);
else
  die("Missing session id");
if (strlen($id) < 32)
  die("Invalid session id");

$row = $db->query("SELECT rowid,* FROM sessions WHERE id='$id'")->fetch();
if ($row == false) {
   die("Session not found");
}

$uri = $row['redirect_uri'];
$rowid = $row['rowid'];
$realm = $row['realm'];

$user = sha1(mt_rand());

if (!$db->exec("UPDATE sessions SET user='$user', type='cert' WHERE rowid=$rowid")) {
  die("Failed to update session database");
}

$db->exec("INSERT INTO eventlog(user,realm,sessionid,timestamp,notes) " .
	"VALUES ('', '$realm', '$id', " .
	"strftime('%Y-%m-%d %H:%M:%f','now'), " .
	"'completed user input response for client certificate enrollment')");

header("Location: $uri", true, 302);

?>
