<?php

require('config.php');

$params = explode("/", $_SERVER["PATH_INFO"], 3);
$realm = $params[1];
$cmd = $params[2];
$method = $_SERVER["REQUEST_METHOD"];

unset($user);
unset($rowid);

$db = new PDO($osu_db);
if (!$db) {
  error_log("EST: Could not access database");
  die("Could not access database");
}

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
    error_log("EST: Missing auth parameter");
    die('Authentication failed');
  }
  $user = $data['username'];
  if (strlen($user) < 1) {
    error_log("EST: Empty username");
    die('Authentication failed');
  }

  $sql = "SELECT rowid,password,operation FROM sessions " .
    "WHERE user='$user' AND realm='$realm'";
  $q = $db->query($sql);
  if (!$q) {
    error_log("EST: Session not found for user=$user realm=$realm");
    die("Session not found");
  }
  $row = $q->fetch();
  if (!$row) {
    error_log("EST: Session fetch failed for user=$user realm=$realm");
    die('Session not found');
  }
  $rowid = $row['rowid'];

  $oper = $row['operation'];
  if ($oper != '5') {
    error_log("EST: Unexpected operation $oper for user=$user realm=$realm");
    die("Session not found");
  }
  $pw = $row['password'];
  if (strlen($pw) < 1) {
    error_log("EST: Empty password for user=$user realm=$realm");
    die('Authentication failed');
  }

  $A1 = md5($user . ':' . $realm . ':' . $pw);
  $A2 = md5($method . ':' . $data['uri']);
  $resp = md5($A1 . ':' . $data['nonce'] . ':' . $data['nc'] . ':' .
	      $data['cnonce'] . ':' . $data['qop'] . ':' . $A2);
  if ($data['response'] != $resp) {
    error_log("EST: Incorrect authentication response for user=$user realm=$realm");
    die('Authentication failed');
  }
} else if (isset($_SERVER["SSL_CLIENT_VERIFY"]) &&
	   $_SERVER["SSL_CLIENT_VERIFY"] == "SUCCESS" &&
	   isset($_SERVER["SSL_CLIENT_M_SERIAL"])) {
  $user = "cert-" . $_SERVER["SSL_CLIENT_M_SERIAL"];
  $sql = "SELECT rowid,password,operation FROM sessions " .
    "WHERE user='$user' AND realm='$realm'";
  $q = $db->query($sql);
  if (!$q) {
    error_log("EST: Session not found for user=$user realm=$realm");
    die("Session not found");
  }
  $row = $q->fetch();
  if (!$row) {
    error_log("EST: Session fetch failed for user=$user realm=$realm");
    die('Session not found');
  }
  $rowid = $row['rowid'];

  $oper = $row['operation'];
  if ($oper != '10') {
    error_log("EST: Unexpected operation $oper for user=$user realm=$realm");
    die("Session not found");
  }
}


if ($method == "GET" && $cmd == "cacerts") {
  $fname = "$osu_root/est/$realm-cacerts.pkcs7";
  if (!file_exists($fname)) {
    error_log("EST: cacerts - unknown realm $realm");
    die("Unknown realm");
  }

  header("Content-Transfer-Encoding: base64");
  header("Content-Type: application/pkcs7-mime");

  $data = file_get_contents($fname);
  echo wordwrap(base64_encode($data), 72, "\n", true);
  echo "\n";
  error_log("EST: cacerts");
} else if ($method == "GET" && $cmd == "csrattrs") {
  header("Content-Transfer-Encoding: base64");
  header("Content-Type: application/csrattrs");
  readfile("$osu_root/est/est-attrs.b64");
  error_log("EST: csrattrs");
} else if ($method == "POST" &&
           ($cmd == "simpleenroll" || $cmd == "simplereenroll")) {
  $reenroll = $cmd == "simplereenroll";
  if (!$reenroll && (!isset($user) || strlen($user) == 0)) {
    header('HTTP/1.1 401 Unauthorized');
    header('WWW-Authenticate: Digest realm="'.$realm.
	   '",qop="auth",nonce="'.uniqid().'",opaque="'.md5($realm).'"');
    error_log("EST: simpleenroll - require authentication");
    die('Authentication required');
  }
  if ($reenroll &&
      (!isset($user) ||
       !isset($_SERVER["SSL_CLIENT_VERIFY"]) ||
       $_SERVER["SSL_CLIENT_VERIFY"] != "SUCCESS")) {
    header('HTTP/1.1 403 Forbidden');
    error_log("EST: simplereenroll - require certificate authentication");
    die('Authentication required');
  }
  if (!isset($_SERVER["CONTENT_TYPE"])) {
    error_log("EST: simpleenroll without Content-Type");
    die("Missing Content-Type");
  }
  if (!stristr($_SERVER["CONTENT_TYPE"], "application/pkcs10")) {
    error_log("EST: simpleenroll - unexpected Content-Type: " .
	      $_SERVER["CONTENT_TYPE"]);
    die("Unexpected Content-Type");
  }

  $data = file_get_contents("php://input");
  error_log("EST: simpleenroll - POST data from php://input: " . $data);
  $req = base64_decode($data);
  if ($req == FALSE) {
    error_log("EST: simpleenroll - Invalid base64-encoded PKCS#10 data");
    die("Invalid base64-encoded PKCS#10 data");
  }
  $cadir = "$osu_root/est";
  $reqfile = "$cadir/tmp/cert-req.pkcs10";
  $f = fopen($reqfile, "wb");
  fwrite($f, $req);
  fclose($f);

  $req_pem = "$reqfile.pem";
  if (file_exists($req_pem))
    unlink($req_pem);
  exec("openssl req -in $reqfile -inform DER -out $req_pem -outform PEM");
  if (!file_exists($req_pem)) {
    error_log("EST: simpleenroll - Failed to parse certificate request");
    die("Failed to parse certificate request");
  }

  /* FIX: validate request and add HS 2.0 extensions to cert */
  $cert_pem = "$cadir/tmp/req-signed.pem";
  if (file_exists($cert_pem))
    unlink($cert_pem);
  exec("openssl x509 -req -in $req_pem -CAkey $cadir/cakey.pem -out $cert_pem -CA $cadir/cacert.pem -CAserial $cadir/serial -days 365 -text");
  if (!file_exists($cert_pem)) {
    error_log("EST: simpleenroll - Failed to sign certificate");
    die("Failed to sign certificate");
  }

  $cert = file_get_contents($cert_pem);
  $handle = popen("openssl x509 -in $cert_pem -serial -noout", "r");
  $serial = fread($handle, 200);
  pclose($handle);
  $pattern = "/serial=(?P<snhex>[0-9a-fA-F:]*)/m";
  preg_match($pattern, $serial, $matches);
  if (!isset($matches['snhex']) || strlen($matches['snhex']) < 1) {
    error_log("EST: simpleenroll - Could not get serial number");
    die("Could not get serial number");
  }
  $sn = str_replace(":", "", strtoupper($matches['snhex']));

  $user = "cert-$sn";
  error_log("EST: user = $user");

  $cert_der = "$cadir/tmp/req-signed.der";
  if (file_exists($cert_der))
    unlink($cert_der);
  exec("openssl x509 -in $cert_pem -inform PEM -out $cert_der -outform DER");
  if (!file_exists($cert_der)) {
    error_log("EST: simpleenroll - Failed to convert certificate");
    die("Failed to convert certificate");
  }
  $der = file_get_contents($cert_der);
  $fingerprint = hash("sha256", $der);
  error_log("EST: sha256(DER cert): $fingerprint");

  $pkcs7 = "$cadir/tmp/est-client.pkcs7";
  if (file_exists($pkcs7))
    unlink($pkcs7);
  exec("openssl crl2pkcs7 -nocrl -certfile $cert_pem -out $pkcs7 -outform DER");
  if (!file_exists($pkcs7)) {
    error_log("EST: simpleenroll - Failed to prepare PKCS#7 file");
    die("Failed to prepare PKCS#7 file");
  }

  if (!$db->exec("UPDATE sessions SET user='$user', cert='$fingerprint', cert_pem='$cert' WHERE rowid=$rowid")) {
    error_log("EST: simpleenroll - Failed to update session database");
    die("Failed to update session database");
  }

  header("Content-Transfer-Encoding: base64");
  header("Content-Type: application/pkcs7-mime");

  $data = file_get_contents($pkcs7);
  $resp = wordwrap(base64_encode($data), 72, "\n", true);
  echo $resp . "\n";
  error_log("EST: simpleenroll - PKCS#7 response: " . $resp);
} else {
  header("HTTP/1.0 404 Not Found");
  error_log("EST: Unexpected method or path");
  die("Unexpected method or path");
}

?>
