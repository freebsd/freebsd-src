# Convert tzdata source into a smaller version of itself.

# Contributed by Paul Eggert.  This file is in the public domain.

# This is not a general-purpose converter; it is designed for current tzdata.
# 'zic' should treat this script's output as if it were identical to
# this script's input.


# Return a new rule name.
# N_RULE_NAMES keeps track of how many rule names have been generated.

function gen_rule_name(alphabet, base, rule_name, n, digit)
{
  alphabet = ""
  alphabet = alphabet "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  alphabet = alphabet "abcdefghijklmnopqrstuvwxyz"
  alphabet = alphabet "!$%&'()*+,./:;<=>?@[\\]^_`{|}~"
  base = length(alphabet)
  rule_name = ""
  n = n_rule_names++

  do {
    n -= rule_name && n <= base
    digit = n % base
    rule_name = substr(alphabet, digit + 1, 1) rule_name
    n = (n - digit) / base
  } while (n);

  return rule_name
}

# Process an input line and save it for later output.

function process_input_line(line, field, end, i, n, startdef)
{
  # Remove comments, normalize spaces, and append a space to each line.
  sub(/#.*/, "", line)
  line = line " "
  gsub(/[[:space:]]+/, " ", line)

  # Abbreviate keywords.  Do not abbreviate "Link" to just "L",
  # as pre-2017c zic erroneously diagnoses "Li" as ambiguous.
  sub(/^Link /, "Li ", line)
  sub(/^Rule /, "R ", line)
  sub(/^Zone /, "Z ", line)

  # SystemV rules are not needed.
  if (line ~ /^R SystemV /) return

  # Replace FooAsia rules with the same rules without "Asia", as they
  # are duplicates.
  if (match(line, /[^ ]Asia /)) {
    if (line ~ /^R /) return
    line = substr(line, 1, RSTART) substr(line, RSTART + 5)
  }

  # Abbreviate times.
  while (match(line, /[: ]0+[0-9]/))
    line = substr(line, 1, RSTART) substr(line, RSTART + RLENGTH - 1)
  while (match(line, /:0[^:]/))
    line = substr(line, 1, RSTART - 1) substr(line, RSTART + 2)

  # Abbreviate weekday names.  Do not abbreviate "Sun" and "Sat", as
  # pre-2017c zic erroneously diagnoses "Su" and "Sa" as ambiguous.
  while (match(line, / (last)?(Mon|Wed|Fri)[ <>]/)) {
    end = RSTART + RLENGTH
    line = substr(line, 1, end - 4) substr(line, end - 1)
  }
  while (match(line, / (last)?(Tue|Thu)[ <>]/)) {
    end = RSTART + RLENGTH
    line = substr(line, 1, end - 3) substr(line, end - 1)
  }

  # Abbreviate "max", "only" and month names.
  # Do not abbreviate "min", as pre-2017c zic erroneously diagnoses "mi"
  # as ambiguous.
  gsub(/ max /, " ma ", line)
  gsub(/ only /, " o ", line)
  gsub(/ Jan /, " Ja ", line)
  gsub(/ Feb /, " F ", line)
  gsub(/ Apr /, " Ap ", line)
  gsub(/ Aug /, " Au ", line)
  gsub(/ Sep /, " S ", line)
  gsub(/ Oct /, " O ", line)
  gsub(/ Nov /, " N ", line)
  gsub(/ Dec /, " D ", line)

  # Strip leading and trailing space.
  sub(/^ /, "", line)
  sub(/ $/, "", line)

  # Remove unnecessary trailing zero fields.
  sub(/ 0+$/, "", line)

  # Remove unnecessary trailing days-of-month "1".
  if (match(line, /[[:alpha:]] 1$/))
    line = substr(line, 1, RSTART)

  # Remove unnecessary trailing " Ja" (for January).
  sub(/ Ja$/, "", line)

  n = split(line, field)

  # Abbreviate rule names.
  i = field[1] == "Z" ? 4 : field[1] == "Li" ? 0 : 2
  if (i && field[i] ~ /^[^-+0-9]/) {
    if (!rule[field[i]])
      rule[field[i]] = gen_rule_name()
    field[i] = rule[field[i]]
  }

  # If this zone supersedes an earlier one, delete the earlier one
  # from the saved output lines.
  startdef = ""
  if (field[1] == "Z")
    zonename = startdef = field[2]
  else if (field[1] == "Li")
    zonename = startdef = field[3]
  else if (field[1] == "R")
    zonename = ""
  if (startdef) {
    i = zonedef[startdef]
    if (i) {
      do
	output_line[i - 1] = ""
      while (output_line[i++] ~ /^[-+0-9]/);
    }
  }
  zonedef[zonename] = nout + 1

  # Save the line for later output.
  line = field[1]
  for (i = 2; i <= n; i++)
    line = line " " field[i]
  output_line[nout++] = line
}

function output_saved_lines(i)
{
  for (i = 0; i < nout; i++)
    if (output_line[i])
      print output_line[i]
}

BEGIN {
  print "# This zic input file is in the public domain."
}

/^[[:space:]]*[^#[:space:]]/ {
  process_input_line($0)
}

END {
  output_saved_lines()
}
