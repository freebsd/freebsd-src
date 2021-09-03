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

$pw = $_POST["password"];
if (strlen($id) < 32 || !isset($pw)) {
  die("Invalid POST data");
}

$row = $db->query("SELECT rowid,* FROM sessions WHERE id='$id'")->fetch();
if ($row == false) {
   die("Session not found");
}
$user = $row['user'];
$realm = $row['realm'];

$uri = $row['redirect_uri'];
$rowid = $row['rowid'];

if (!$db->exec("UPDATE sessions SET password='$pw' WHERE rowid=$rowid")) {
  die("Failed to update session database");
}

$db->exec("INSERT INTO eventlog(user,realm,sessionid,timestamp,notes) " .
	"VALUES ('$user', '$realm', '$id', " .
	"strftime('%Y-%m-%d %H:%M:%f','now'), " .
	"'completed user input response for subscription remediation')");

header("Location: $uri", true, 302);

?>
