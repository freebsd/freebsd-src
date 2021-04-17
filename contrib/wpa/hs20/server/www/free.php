<html>
<head>
<title>Hotspot 2.0 - public and free hotspot</title>
</head>
<body>

<?php

$id = $_GET["session_id"];

echo "<h3>Hotspot 2.0 - public and free hotspot</h3>\n";

echo "<form action=\"add-free.php\" method=\"POST\">\n";
echo "<input type=\"hidden\" name=\"id\" value=\"$id\">\n";

?>

<p>Terms and conditions..</p>
<input type="submit" value="Accept">
</form>

</body>
</html>
