<html>
<head>
<title>Hotspot 2.0 subscription remediation</title>
</head>
<body>

<?php

require('config.php');

$db = new PDO($osu_db);
if (!$db) {
   die($sqliteerror);
}

if (isset($_GET["session_id"]))
	$id = preg_replace("/[^a-fA-F0-9]/", "", $_GET["session_id"]);
else
	$id = 0;
echo "SessionID: " . $id . "<br>\n";

$row = $db->query("SELECT * FROM sessions WHERE id='$id'")->fetch();
if ($row == false) {
   die("Session not found");
}

$username = $row['user'];
echo "User: " . $username . "@" . $row['realm'] . "<br>\n";

$user = $db->query("SELECT machine_managed,methods FROM users WHERE identity='$username'")->fetch();
if ($user == false) {
   die("User not found");
}

echo "<hr><br>\n";

$cert = $user['methods'] == "TLS" || strncmp($username, "cert-", 5) == 0;

if ($cert) {
   echo "<a href=\"redirect.php?id=" . $_GET["session_id"] . "\">Complete user subscription remediation</a><br>\n";
} else if ($user['machine_managed'] == "1") {
   echo "<a href=\"redirect.php?id=" . $_GET["session_id"] . "\">Complete user subscription remediation</a><br>\n";
   echo "This will provide a new machine-generated password.<br>\n";
} else {
   echo "<form action=\"remediation-pw.php\" method=\"POST\">\n";
   echo "<input type=\"hidden\" name=\"id\" value=\"$id\">\n";
   echo "New password: <input type=\"password\" name=\"password\"><br>\n";
   echo "<input type=\"submit\" value=\"Change password\">\n";
   echo "</form>\n";
}

?>

</body>
</html>
