#!/bin/sh

line="---------------------------------------------------"

git log --no-merges --decorate -M --stat --pretty=format:"$line%n%ai %an <%ae>%d%n%n%s%n%n%b" |
uniq | fold -s
echo
echo $line
