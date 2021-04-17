<html>
<head>
<title>Hotspot 2.0 signup</title>
</head>
<body>

<?php

$id = $_GET["session_id"];

require('config.php');

$db = new PDO($osu_db);
if (!$db) {
   die($sqliteerror);
}

$row = $db->query("SELECT realm,test FROM sessions WHERE id='$id'")->fetch();
if ($row == false) {
   die("Session not found for id: $id");
}
$realm = $row['realm'];
$test = $row['test'];

if (strlen($test) > 0) {
  echo "<p style=\"color:#FF0000\">Special test functionality: $test</red></big></p>\n";
}

echo "<h3>Sign up for a subscription - $realm</h3>\n";

echo "<p>This page can be used to select between three different types of subscriptions for testing purposes.</p>\n";

echo "<h4>Option 1 - shared free access credential</h4>\n";

$row = $db->query("SELECT value FROM osu_config WHERE realm='$realm' AND field='free_account'")->fetch();
if ($row && strlen($row['value']) > 0) {
  echo "<p><a href=\"free.php?session_id=$id\">Sign up for free access</a></p>\n";
}

echo "<h4>Option 2 - username/password credential</h4>\n";

echo "<form action=\"add-mo.php\" method=\"POST\">\n";
echo "<input type=\"hidden\" name=\"id\" value=\"$id\">\n";
?>
Select a username and password. Leave password empty to get automatically
generated and machine managed password.<br>
Username: <input type="text" name="user"><br>
Password: <input type="password" name="password"><br>
<input type="submit" value="Complete subscription registration">
</form>

<?php
echo "<h4>Option 3 - client certificate credential</h4>\n";

echo "<p><a href=\"cert-enroll.php?id=$id\">Enroll a client certificate</a></p>\n"
?>

</body>
</html>
