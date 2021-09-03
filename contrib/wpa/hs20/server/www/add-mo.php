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

$user = $_POST["user"];
$pw = $_POST["password"];
if (strlen($id) < 32 || !isset($user) || !isset($pw)) {
  die("Invalid POST data");
}

if (strlen($user) < 1 || strncasecmp($user, "cert-", 5) == 0) {
  echo "<html><body><p><red>Invalid username</red></p>\n";
  echo "<a href=\"signup.php?session_id=$id\">Try again</a>\n";
  echo "</body></html>\n";
  exit;
}

$row = $db->query("SELECT rowid,* FROM sessions WHERE id='$id'")->fetch();
if ($row == false) {
   die("Session not found");
}
$realm = $row['realm'];

$userrow = $db->query("SELECT identity FROM users WHERE identity='$user' AND realm='$realm'")->fetch();
if ($userrow) {
  echo "<html><body><p><red>Selected username is not available</red></p>\n";
  echo "<a href=\"signup.php?session_id=$id\">Try again</a>\n";
  echo "</body></html>\n";
  exit;
}

$uri = $row['redirect_uri'];
$rowid = $row['rowid'];

if (!$db->exec("UPDATE sessions SET user='$user', password='$pw', realm='$realm', type='password' WHERE rowid=$rowid")) {
  die("Failed to update session database");
}

$db->exec("INSERT INTO eventlog(user,realm,sessionid,timestamp,notes) " .
	"VALUES ('$user', '$realm', '$id', " .
	"strftime('%Y-%m-%d %H:%M:%f','now'), " .
	"'completed user input response for a new PPS MO')");

header("Location: $uri", true, 302);

?>
