#!/bin/sh -e

make unifdef.txt
cp unifdef.txt web

git gc --aggressive
git update-server-info
git push --all github
git push --tags github

# for gitweb
echo "selectively remove C preprocessor conditionals" >.git/description
echo "Homepage: <a href='http://dotat.at/prog/unifdef'>http://dotat.at/prog/unifdef</a>" >.git/README.html

touch .git/git-daemon-export-ok
rsync --recursive --links --delete .git/ chiark:public-git/unifdef.git/
rsync --recursive --links           web/ chiark:public-html/prog/unifdef/

exit
