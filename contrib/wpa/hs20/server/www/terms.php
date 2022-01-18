<?php

require('config.php');

function print_header()
{
   echo "<html>\n";
   echo "<head><title>HS 2.0 Terms and Conditions</title></head>\n";
   echo "<body>\n";
}

$db = new PDO($osu_db);
if (!$db) {
   die($sqliteerror);
}

if (!isset($_GET["addr"])) {
   die("Missing addr parameter");
}
$addr = $_GET["addr"];

$accept = isset($_GET["accept"]) && $_GET["accept"] == "yes";

$res = $db->prepare("SELECT identity FROM pending_tc WHERE mac_addr=?");
$res->execute(array($addr));
$row = $res->fetch();
if (!$row) {
   die("No pending session for the specified MAC address");
}
$identity = $row[0];

if (!$accept) {
   print_header();

   echo "<p>Accept the following terms and conditions by clicking here: <a href=\"terms.php?addr=$addr&accept=yes\">Accept</a></p>\n<hr>\n";
   readfile($t_c_file);
} else {
   $res = $db->prepare("UPDATE users SET t_c_timestamp=? WHERE identity=?");
   if (!$res->execute(array($t_c_timestamp, $identity))) {
      die("Failed to update user account.");
   }

   $res = $db->prepare("DELETE FROM pending_tc WHERE mac_addr=?");
   $res->execute(array($addr));

   $fp = fsockopen($hostapd_ctrl);
   if (!$fp) {
      die("Could not connect to hostapd(AS)");
   }

   fwrite($fp, "DAC_REQUEST coa $addr t_c_clear");
   fclose($fp);

   $waiting = true;
   $ack = false;
   for ($i = 1; $i <= 10; $i++) {
      $res = $db->prepare("SELECT waiting_coa_ack,coa_ack_received FROM current_sessions WHERE mac_addr=?");
      $res->execute(array($addr));
      $row = $res->fetch();
      if (!$row) {
         die("No current session for the specified MAC address");
      }
      if (strlen($row[0]) > 0)
            $waiting = $row[0] == 1;
      if (strlen($row[1]) > 0)
            $ack = $row[1] == 1;
      $res->closeCursor();
      if (!$waiting)
         break;
      sleep(1);
   }
   if ($ack) {
      header('X-WFA-Hotspot20-Filtering: removed');
      print_header();
      echo "<p>Terms and conditions were accepted.</p>\n";

      echo "<P>Filtering disabled.</P>\n";
   } else {
      print_header();
      echo "<P>Failed to disable filtering.</P>\n";
   }
}

?>

</body>
</html>
