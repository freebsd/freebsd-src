<?php

require('config.php');

$db = new PDO($osu_db);
if (!$db) {
   die($sqliteerror);
}

if (isset($_POST["id"]))
  $id = preg_replace("/[^a-fA-F0-9]/", "", $_POST["id"]);
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

$row = $db->query("SELECT value FROM osu_config WHERE realm='$realm' AND field='free_account'")->fetch();
if (!$row || strlen($row['value']) == 0) {
  die("Free account disabled");
}

$user = $row['value'];

$row = $db->query("SELECT password FROM users WHERE identity='$user' AND realm='$realm'")->fetch();
if (!$row)
  die("Free account not found");

$pw = $row['password'];

if (!$db->exec("UPDATE sessions SET user='$user', password='$pw', realm='$realm', machine_managed='1' WHERE rowid=$rowid")) {
  die("Failed to update session database");
}

$db->exec("INSERT INTO eventlog(user,realm,sessionid,timestamp,notes) " .
	"VALUES ('$user', '$realm', '$id', " .
	"strftime('%Y-%m-%d %H:%M:%f','now'), " .
	"'completed user input response for a new PPS MO')");

header("Location: $uri", true, 302);

?>
