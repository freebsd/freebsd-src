#!/bin/sh
#
# Look up specified version of Github Actions an pin to that specific
# revision.
#

set -e

github=https://github.com

for workflow in workflows/*.yml; do
	sed 's/ - /   /' ${workflow} | grep -v '^#' | awk '/uses:/ {print}' | \
	    while read line; do
		action_ver=$(awk '{print $2}' <<<${line})
		action=$(cut -f1 -d@ <<<${action_ver})
		ver=$(cut -f2 -d@ <<<${action_ver})
		intendedver=$(awk '{print $4}' <<<${line})
		if [ -z "${intendedver}" ]; then
			intendedver=${ver}
		fi
		case "${action}" in
		google/oss-fuzz/*)	actiondir=google/oss-fuzz ;;
		*)			actiondir="${action}" ;;
		esac
		if [ ! -d /tmp/${actiondir} ]; then
			git clone ${github}/${actiondir} /tmp/${actiondir}
		fi
		hash=$(cd /tmp/${actiondir} && git rev-parse ${intendedver})
		sed -i -e "s|uses: ${action}@.*|uses: ${action}@${hash} # ${intendedver}|" \
		     ${workflow}
	done
done

# Output actions for allowlist.
awk 'BEGIN{IFS=":"} /^ +uses:.*@/{print $2","}' workflows/*.yml | sort -u
