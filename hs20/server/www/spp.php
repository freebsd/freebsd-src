<?php

require('config.php');

if (!stristr($_SERVER["CONTENT_TYPE"], "application/soap+xml")) {
  error_log("spp.php - Unexpected Content-Type " . $_SERVER["CONTENT_TYPE"]);
  die("Unexpected Content-Type");
}

if ($_SERVER["REQUEST_METHOD"] != "POST") {
  error_log("spp.php - Unexpected method " . $_SERVER["REQUEST_METHOD"]);
  die("Unexpected method");
}

if (isset($_GET["realm"])) {
  $realm = $_GET["realm"];
  $realm = PREG_REPLACE("/[^0-9a-zA-Z\.\-]/i", '', $realm);
} else {
  error_log("spp.php - Realm not specified");
  die("Realm not specified");
}

if (isset($_GET["test"]))
  $test = PREG_REPLACE("/[^0-9a-zA-Z\_\-]/i", '', $_GET["test"]);
else
  $test = "";

unset($user);
putenv("HS20CERT");

if (!empty($_SERVER['PHP_AUTH_DIGEST'])) {
  $needed = array('nonce'=>1, 'nc'=>1, 'cnonce'=>1, 'qop'=>1, 'username'=>1,
		  'uri'=>1, 'response'=>1);
  $data = array();
  $keys = implode('|', array_keys($needed));
  preg_match_all('@(' . $keys . ')=(?:([\'"])([^\2]+?)\2|([^\s,]+))@',
		 $_SERVER['PHP_AUTH_DIGEST'], $matches, PREG_SET_ORDER);
  foreach ($matches as $m) {
    $data[$m[1]] = $m[3] ? $m[3] : $m[4];
    unset($needed[$m[1]]);
  }
  if ($needed) {
    error_log("spp.php - Authentication failed - missing: " . print_r($needed));
    die('Authentication failed');
  }
  $user = $data['username'];
  if (strlen($user) < 1) {
    error_log("spp.php - Authentication failed - empty username");
    die('Authentication failed');
  }


  $db = new PDO($osu_db);
  if (!$db) {
    error_log("spp.php - Could not access database");
    die("Could not access database");
  }
  $row = $db->query("SELECT password FROM users " .
		    "WHERE identity='$user' AND realm='$realm'")->fetch();
  if (!$row) {
    $row = $db->query("SELECT osu_password FROM users " .
		      "WHERE osu_user='$user' AND realm='$realm'")->fetch();
    $pw = $row['osu_password'];
  } else
    $pw = $row['password'];
  if (!$row) {
    error_log("spp.php - Authentication failed - user '$user' not found");
    die('Authentication failed');
  }
  if (strlen($pw) < 1) {
    error_log("spp.php - Authentication failed - empty password");
    die('Authentication failed');
  }

  $A1 = md5($user . ':' . $realm . ':' . $pw);
  $A2 = md5($_SERVER['REQUEST_METHOD'] . ':' . $data['uri']);
  $resp = md5($A1 . ':' . $data['nonce'] . ':' . $data['nc'] . ':' .
	      $data['cnonce'] . ':' . $data['qop'] . ':' . $A2);
  if ($data['response'] != $resp) {
    error_log("Authentication failure - response mismatch");
    die('Authentication failed');
  }
} else if (isset($_SERVER["SSL_CLIENT_VERIFY"]) &&
	   $_SERVER["SSL_CLIENT_VERIFY"] == "SUCCESS" &&
	   isset($_SERVER["SSL_CLIENT_M_SERIAL"])) {
  $user = "cert-" . $_SERVER["SSL_CLIENT_M_SERIAL"];
  putenv("HS20CERT=yes");
} else if (isset($_GET["hotspot2dot0-mobile-identifier-hash"])) {
  $id_hash = $_GET["hotspot2dot0-mobile-identifier-hash"];
  $id_hash = PREG_REPLACE("/[^0-9a-h]/i", '', $id_hash);

  $db = new PDO($osu_db);
  if (!$db) {
    error_log("spp.php - Could not access database");
    die("Could not access database");
  }

  $row = $db->query("SELECT * FROM sim_provisioning " .
		    "WHERE mobile_identifier_hash='$id_hash'")->fetch();
  if (!$row) {
    error_log("spp.php - SIM provisioning failed - mobile_identifier_hash not found");
    die('SIM provisioning failed - mobile_identifier_hash not found');
  }

  $imsi = $row['imsi'];
  $mac_addr = $row['mac_addr'];
  $eap_method = $row['eap_method'];

  $row = $db->query("SELECT COUNT(*) FROM osu_config " .
		    "WHERE realm='$realm'")->fetch();
  if (!$row || intval($row[0]) < 1) {
    error_log("spp.php - SIM provisioning failed - realm $realm not found");
    die('SIM provisioning failed');
  }

  error_log("spp.php - SIM provisioning for IMSI $imsi");
  putenv("HS20SIMPROV=yes");
  putenv("HS20IMSI=$imsi");
  putenv("HS20MACADDR=$mac_addr");
  putenv("HS20EAPMETHOD=$eap_method");
  putenv("HS20IDHASH=$id_hash");
} else if (!isset($_SERVER["PATH_INFO"]) ||
	   $_SERVER["PATH_INFO"] != "/signup") {
  header('HTTP/1.1 401 Unauthorized');
  header('WWW-Authenticate: Digest realm="'.$realm.
	 '",qop="auth",nonce="'.uniqid().'",opaque="'.md5($realm).'"');
  error_log("spp.php - Authentication required (not signup)");
  die('Authentication required (not signup)');
}


if (isset($user) && strlen($user) > 0)
  putenv("HS20USER=$user");
else
  putenv("HS20USER");

putenv("HS20REALM=$realm");
$postdata = file_get_contents("php://input");
putenv("HS20POST=$postdata");
$addr = $_SERVER["REMOTE_ADDR"];
putenv("HS20ADDR=$addr");
putenv("HS20TEST=$test");

$last = exec("$osu_root/spp/hs20_spp_server -r$osu_root -f/tmp/hs20_spp_server.log", $output, $ret);

if ($ret == 2) {
  if (empty($_SERVER['PHP_AUTH_DIGEST'])) {
    header('HTTP/1.1 401 Unauthorized');
    header('WWW-Authenticate: Digest realm="'.$realm.
           '",qop="auth",nonce="'.uniqid().'",opaque="'.md5($realm).'"');
    error_log("spp.php - Authentication required (ret 2)");
    die('Authentication required');
  } else {
    error_log("spp.php - Unexpected authentication error");
    die("Unexpected authentication error");
  }
}
if ($ret != 0) {
  error_log("spp.php - Failed to process SPP request");
  die("Failed to process SPP request");
}
//error_log("spp.php: Response: " . implode($output));

header("Content-Type: application/soap+xml");

echo implode($output);

?>
