#! /bin/awk -f

# Correct Intellifont-based height metrics for several glyphs in
# special font for TrueType CG Times (LaserJet 4000 and later).

function scale(num)
{
  return int(num * em + 0.5)
}

BEGIN {
  FS = "\t"
  OFS = "\t"
  em = 26346	# Intellifont (8782 DU/em) and hpftodit(1) multiplier of 3

  ascent["integralcrvmid"] = scale(0.84358)
  descent["integralcrvmid"] = scale(0.25006)
  ascent["integralbt"] = scale(0.84358)
  descent["integralbt"] = scale(0.15164)
  ascent["lt"] = scale(0.84358)
  descent["lt"] = scale(0.15164)
  ascent["parenlefttp"] = scale(0.84358)
  descent["parenlefttp"] = scale(0.15164)
  ascent["bracelefttp"] = scale(0.84358)
  descent["bracelefttp"] = scale(0.15164)
  ascent["lk"] = scale(0.84358)
  descent["lk"] = scale(0.15164)
  ascent["braceleftmid"] = scale(0.84358)
  descent["braceleftmid"] = scale(0.15164)
  ascent["lb"] = scale(0.84358)
  descent["lb"] = scale(0.15164)
  ascent["parenleftbt"] = scale(0.84358)
  descent["parenleftbt"] = scale(0.15164)
  ascent["braceleftbt"] = scale(0.84358)
  descent["braceleftbt"] = scale(0.15164)
  ascent["rt"] = scale(0.84358)
  descent["rt"] = scale(0.15164)
  ascent["parenrighttp"] = scale(0.84358)
  descent["parenrighttp"] = scale(0.15164)
  ascent["bracerighttp"] = scale(0.84358)
  descent["bracerighttp"] = scale(0.15164)
  ascent["rk"] = scale(0.84358)
  descent["rk"] = scale(0.15164)
  ascent["bracerightmid"] = scale(0.84358)
  descent["bracerightmid"] = scale(0.15164)
  ascent["rb"] = scale(0.84358)
  descent["rb"] = scale(0.15164)
  ascent["parenrightbt"] = scale(0.84358)
  descent["parenrightbt"] = scale(0.15164)
  ascent["bracerightbt"] = scale(0.84358)
  descent["bracerightbt"] = scale(0.15164)
  ascent["parenrightex"] = scale(0.84358)
  descent["parenrightex"] = scale(0.15164)
  ascent["parenleftex"] = scale(0.84358)
  descent["parenleftex"] = scale(0.15164)
  ascent["bv"] = scale(0.84358)
  descent["bv"] = scale(0.15164)
  ascent["bracerightex"] = scale(0.84358)
  descent["bracerightex"] = scale(0.15164)
  ascent["braceleftex"] = scale(0.84358)
  descent["braceleftex"] = scale(0.15164)
  ascent["integralex"] = scale(0.84358)
  descent["integralex"] = scale(0.15164)
  ascent["bracketrightex"] = scale(0.84358)
  descent["bracketrightex"] = scale(0.15164)
  ascent["bracketleftex"] = scale(0.84358)
  descent["bracketleftex"] = scale(0.15164)
  ascent["barex"] = scale(0.84358)
  descent["barex"] = scale(0.15164)
}
{
  if ($2 != "\"" && ascent[$1]) {
    n = split($2, temp, ",")
    $2 = sprintf("%d,%d,%d", temp[1], ascent[$1], descent[$1])
    # just in case there are additional metrics
    for (i = 4; i <= n; i++)
      $2 = $2 "," temp[i]
  }
  print $0
}

# EOF
