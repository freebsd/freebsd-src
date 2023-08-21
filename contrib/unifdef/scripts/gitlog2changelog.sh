#!/bin/sh

line="---------------------------------------------------"

git log --no-merges -M --stat \
    --pretty=format:"$line%n%ai %an <%ae>%n%n%s%n%n%b" |
uniq | fold -s
echo
echo $line
