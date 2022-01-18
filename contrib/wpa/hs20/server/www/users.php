<?php

require('config.php');

$db = new PDO($osu_db);
if (!$db) {
   die($sqliteerror);
}

if (isset($_GET["id"])) {
	$id = $_GET["id"];
	if (!is_numeric($id))
		$id = 0;
} else
	$id = 0;
if (isset($_GET["cmd"]))
	$cmd = $_GET["cmd"];
else
	$cmd = '';

if ($cmd == 'eventlog' && $id > 0) {
	$row = $db->query("SELECT dump FROM eventlog WHERE rowid=$id")->fetch();
	$dump = $row['dump'];
	if ($dump[0] == '<') {
	  header("Content-type: text/xml");
	  echo "<?xml version=\"1.0\"?>\n";
	  echo $dump;
	} else {
	  header("Content-type: text/plain");
	  echo $dump;
	}
	exit;
}

if ($cmd == 'mo' && $id > 0) {
	$mo = $_GET["mo"];
	if (!isset($mo))
		exit;
	if ($mo != "devinfo" && $mo != "devdetail" && $mo != "pps")
		exit;
	$row = $db->query("SELECT $mo FROM users WHERE rowid=$id")->fetch();
	header("Content-type: text/xml");
	echo "<?xml version=\"1.0\"?>\n";
	echo $row[$mo];
	exit;
}

if ($cmd == 'cert' && $id > 0) {
	$row = $db->query("SELECT cert_pem FROM users WHERE rowid=$id")->fetch();
	header("Content-type: text/plain");
	echo $row['cert_pem'];
	exit;
}

?>

<html>
<head><title>HS 2.0 users</title></head>
<body>

<?php

if ($cmd == 'subrem-clear' && $id > 0) {
	$db->exec("UPDATE users SET remediation='' WHERE rowid=$id");
}
if ($cmd == 'subrem-add-user' && $id > 0) {
	$db->exec("UPDATE users SET remediation='user' WHERE rowid=$id");
}
if ($cmd == 'subrem-add-machine' && $id > 0) {
	$db->exec("UPDATE users SET remediation='machine' WHERE rowid=$id");
}
if ($cmd == 'subrem-add-reenroll' && $id > 0) {
	$db->exec("UPDATE users SET remediation='reenroll' WHERE rowid=$id");
}
if ($cmd == 'subrem-add-policy' && $id > 0) {
	$db->exec("UPDATE users SET remediation='policy' WHERE rowid=$id");
}
if ($cmd == 'subrem-add-free' && $id > 0) {
	$db->exec("UPDATE users SET remediation='free' WHERE rowid=$id");
}
if ($cmd == 'fetch-pps-on' && $id > 0) {
	$db->exec("UPDATE users SET fetch_pps=1 WHERE rowid=$id");
}
if ($cmd == 'fetch-pps-off' && $id > 0) {
	$db->exec("UPDATE users SET fetch_pps=0 WHERE rowid=$id");
}
if ($cmd == 'reset-pw' && $id > 0) {
	$db->exec("UPDATE users SET password='ChangeMe' WHERE rowid=$id");
}
if ($cmd == "policy" && $id > 0 && isset($_GET["policy"])) {
	$policy = $_GET["policy"];
	if ($policy == "no-policy" ||
	    is_readable("$osu_root/spp/policy/$policy.xml")) {
		$db->exec("UPDATE users SET policy='$policy' WHERE rowid=$id");
	}
}
if ($cmd == "account-type" && $id > 0 && isset($_GET["type"])) {
	$type = $_GET["type"];
	if ($type == "shared")
		$db->exec("UPDATE users SET shared=1 WHERE rowid=$id");
	if ($type == "default")
		$db->exec("UPDATE users SET shared=0 WHERE rowid=$id");
}

if ($cmd == "set-osu-cred" && $id > 0) {
  $osu_user = $_POST["osu_user"];
  $osu_password = $_POST["osu_password"];
  if (strlen($osu_user) == 0)
    $osu_password = "";
  $db->exec("UPDATE users SET osu_user='$osu_user', osu_password='$osu_password' WHERE rowid=$id");
}

if ($cmd == 'clear-t-c' && $id > 0) {
	$db->exec("UPDATE users SET t_c_timestamp=NULL WHERE rowid=$id");
}

$dump = 0;

if ($id > 0) {

if (isset($_GET["dump"])) {
	$dump = $_GET["dump"];
	if (!is_numeric($dump))
		$dump = 0;
} else
	$dump = 0;

echo "[<a href=\"users.php\">All users</a>] ";
if ($dump == 0)
	echo "[<a href=\"users.php?id=$id&dump=1\">Include debug dump</a>] ";
else
	echo "[<a href=\"users.php?id=$id\">Without debug dump</a>] ";
echo "<br>\n";

$row = $db->query("SELECT rowid,* FROM users WHERE rowid=$id")->fetch();

echo "<H3>" . $row['identity'] . "@" . $row['realm'] . "</H3>\n";

echo "MO: ";
if (strlen($row['devinfo']) > 0) {
	echo "[<a href=\"users.php?cmd=mo&id=$id&mo=devinfo\">DevInfo</a>]\n";
}
if (strlen($row['devdetail']) > 0) {
	echo "[<a href=\"users.php?cmd=mo&id=$id&mo=devdetail\">DevDetail</a>]\n";
}
if (strlen($row['pps']) > 0) {
	echo "[<a href=\"users.php?cmd=mo&id=$id&mo=pps\">PPS</a>]\n";
}
if (strlen($row['cert_pem']) > 0) {
	echo "[<a href=\"users.php?cmd=cert&id=$id\">Certificate</a>]\n";
}
echo "<BR>\n";

echo "Fetch PPS MO: ";
if ($row['fetch_pps'] == "1") {
	echo "On next connection " .
		"[<a href=\"users.php?cmd=fetch-pps-off&id=$id\">" .
		"do not fetch</a>]<br>\n";
} else {
	echo "Do not fetch " .
		"[<a href=\"users.php?cmd=fetch-pps-on&id=$id\">" .
		"request fetch</a>]<br>\n";
}

$cert = $row['cert'];
if (strlen($cert) > 0) {
  echo "Certificate fingerprint: $cert<br>\n";
}

echo "Remediation: ";
$rem = $row['remediation'];
if ($rem == "") {
	echo "Not required";
	echo " [<a href=\"users.php?cmd=subrem-add-user&id=" .
		   $row['rowid'] . "\">add:user</a>]";
	echo " [<a href=\"users.php?cmd=subrem-add-machine&id=" .
		   $row['rowid'] . "\">add:machine</a>]";
	if ($row['methods'] == 'TLS') {
		echo " [<a href=\"users.php?cmd=subrem-add-reenroll&id=" .
			   $row['rowid'] . "\">add:reenroll</a>]";
	}
	echo " [<a href=\"users.php?cmd=subrem-add-policy&id=" .
		   $row['rowid'] . "\">add:policy</a>]";
	echo " [<a href=\"users.php?cmd=subrem-add-free&id=" .
		   $row['rowid'] . "\">add:free</a>]";
} else if ($rem == "user") {
	echo "User [<a href=\"users.php?cmd=subrem-clear&id=" .
		       $row['rowid'] . "\">clear</a>]";
} else if ($rem == "policy") {
	echo "Policy [<a href=\"users.php?cmd=subrem-clear&id=" .
		       $row['rowid'] . "\">clear</a>]";
} else if ($rem == "free") {
	echo "Free [<a href=\"users.php?cmd=subrem-clear&id=" .
		       $row['rowid'] . "\">clear</a>]";
} else if ($rem == "reenroll") {
	echo "Reenroll [<a href=\"users.php?cmd=subrem-clear&id=" .
		       $row['rowid'] . "\">clear</a>]";
} else  {
	echo "Machine [<a href=\"users.php?cmd=subrem-clear&id=" .
			  $row['rowid'] . "\">clear</a>]";
}
echo "<br>\n";

if (strncmp($row['identity'], "cert-", 5) != 0)
   echo "Machine managed: " . ($row['machine_managed'] == "1" ? "TRUE" : "FALSE") . "<br>\n";

echo "<form>Policy: <select name=\"policy\" " .
	"onChange=\"window.location='users.php?cmd=policy&id=" .
	$row['rowid'] . "&policy=' + this.value;\">\n";
echo "<option value=\"" . $row['policy'] . "\" selected>" . $row['policy'] .
      "</option>\n";
$files = scandir("$osu_root/spp/policy");
foreach ($files as $file) {
	if (!preg_match("/.xml$/", $file))
		continue;
	if ($file == $row['policy'] . ".xml")
		continue;
	$p = substr($file, 0, -4);
	echo "<option value=\"$p\">$p</option>\n";
}
echo "<option value=\"no-policy\">no policy</option>\n";
echo "</select></form>\n";

echo "<form>Account type: <select name=\"type\" " .
	"onChange=\"window.location='users.php?cmd=account-type&id=" .
	$row['rowid'] . "&type=' + this.value;\">\n";
if ($row['shared'] > 0) {
  $default_sel = "";
  $shared_sel = " selected";
} else {
  $default_sel = " selected";
  $shared_sel = "";
}
echo "<option value=\"default\"$default_sel>default</option>\n";
echo "<option value=\"shared\"$shared_sel>shared</option>\n";
echo "</select></form>\n";

echo "Phase 2 method(s): " . $row['methods'] . "<br>\n";

echo "<br>\n";
echo "<a href=\"users.php?cmd=reset-pw&id=" .
	 $row['rowid'] . "\">Reset AAA password</a><br>\n";

echo "<br>\n";
echo "<form action=\"users.php?cmd=set-osu-cred&id=" . $row['rowid'] .
  "\" method=\"POST\">\n";
echo "OSU credentials (if username empty, AAA credentials are used):<br>\n";
echo "username: <input type=\"text\" name=\"osu_user\" value=\"" .
  $row['osu_user'] . "\">\n";
echo "password: <input type=\"password\" name=\"osu_password\">\n";
echo "<input type=\"submit\" value=\"Set OSU credentials\">\n";
echo "</form>\n";

if (strlen($row['t_c_timestamp']) > 0) {
	echo "<br>\n";
	echo "<a href=\"users.php?cmd=clear-t-c&id=" .
		$row['rowid'] .
		"\">Clear Terms and Conditions acceptance</a><br>\n";
}

echo "<hr>\n";

$user = $row['identity'];
$osu_user = $row['osu_user'];
$realm = $row['realm'];
}

if ($id > 0 || ($id == 0 && $cmd == 'eventlog')) {

  if ($id == 0) {
    echo "[<a href=\"users.php\">All users</a>] ";
    echo "<br>\n";
  }

echo "<table border=1>\n";
echo "<tr>";
if ($id == 0) {
  echo "<th>user<th>realm";
}
echo "<th>time<th>address<th>sessionID<th>notes";
if ($dump > 0)
	echo "<th>dump";
echo "\n";
if (isset($_GET["limit"])) {
	$limit = $_GET["limit"];
	if (!is_numeric($limit))
		$limit = 20;
} else
	$limit = 20;
if ($id == 0)
  $res = $db->query("SELECT rowid,* FROM eventlog ORDER BY timestamp DESC LIMIT $limit");
else if (strlen($osu_user) > 0)
  $res = $db->query("SELECT rowid,* FROM eventlog WHERE (user='$user' OR user='$osu_user') AND realm='$realm' ORDER BY timestamp DESC LIMIT $limit");
else
  $res = $db->query("SELECT rowid,* FROM eventlog WHERE user='$user' AND realm='$realm' ORDER BY timestamp DESC LIMIT $limit");
foreach ($res as $row) {
	echo "<tr>";
	if ($id == 0) {
	  echo "<td>" . $row['user'] . "\n";
	  echo "<td>" . $row['realm'] . "\n";
	}
	echo "<td>" . $row['timestamp'] . "\n";
	echo "<td>" . $row['addr'] . "\n";
	echo "<td>" . $row['sessionid'] . "\n";
	echo "<td>" . $row['notes'] . "\n";
	$d = $row['dump'];
	if (strlen($d) > 0) {
		echo "[<a href=\"users.php?cmd=eventlog&id=" . $row['rowid'] .
		  "\">";
		if ($d[0] == '<')
		  echo "XML";
		else
		  echo "txt";
		echo "</a>]\n";
		if ($dump > 0)
			echo "<td>" . htmlspecialchars($d) . "\n";
	}
}
echo "</table>\n";

}


if ($id == 0 && $cmd != 'eventlog') {

echo "[<a href=\"users.php?cmd=eventlog&limit=50\">Eventlog</a>] ";
echo "<br>\n";

echo "<table border=1 cellspacing=0 cellpadding=0>\n";
echo "<tr><th>User<th>Realm<th><small>Remediation</small><th>Policy<th><small>Account type</small><th><small>Phase 2 method(s)</small><th>DevId<th>MAC Address<th>T&C\n";

$res = $db->query('SELECT rowid,* FROM users WHERE (phase2=1 OR methods=\'TLS\') ORDER BY identity');
foreach ($res as $row) {
	echo "<tr><td><a href=\"users.php?id=" . $row['rowid'] . "\"> " .
	    $row['identity'] . " </a>";
	echo "<td>" . $row['realm'];
	$rem = $row['remediation'];
	echo "<td>";
	if ($rem == "") {
		echo "-";
	} else if ($rem == "user") {
		echo "User";
	} else if ($rem == "policy") {
		echo "Policy";
	} else if ($rem == "free") {
		echo "Free";
	} else if ($rem == "reenroll") {
		echo "Reenroll";
	} else  {
		echo "Machine";
	}
	echo "<td>" . $row['policy'];
	if ($row['shared'] > 0)
	  echo "<td>shared";
	else
	  echo "<td>default";
	echo "<td><small>" . $row['methods'] . "</small>";
	echo "<td>";
	$xml = xml_parser_create();
	xml_parse_into_struct($xml, $row['devinfo'], $devinfo);
	foreach($devinfo as $k) {
	  if ($k['tag'] == 'DEVID') {
	    echo "<small>" . $k['value'] . "</small>";
	    break;
	  }
	}
	echo "<td><small>" . $row['mac_addr'] . "</small>";
	echo "<td><small>" . $row['t_c_timestamp'] . "</small>";
	echo "\n";
}
echo "</table>\n";

}

?>

</html>
