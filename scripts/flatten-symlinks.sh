#!/bin/bash

set -e

while read mode object stage path ; do
    case "$mode" in
	120000)
	    # symbolic link
	    deref=$(echo $GIT_COMMIT:$path | git cat-file --batch-check='deref-ok %(objectname)' --follow-symlinks)
	    case "$deref" in
		deref-ok*)
		    echo -e "100644 ${deref#deref-ok } $stage\t$path"
		    ;;
		dangling*) # skip
		    ;;
		*) # the rest, missing etc
		    echo >&2 "Failed to parse symlink $GIT_COMMIT:$path $deref"
		    exit 1
		    ;;
	    esac
	    ;;
	100*)
	    # Regular file, just pass through
	    echo -e "$mode $object $stage\t$path"
	    ;;
	*)
	    echo >&2 "Unhandled ls-tree entry: $line"
	    exit 1
	    ;;
    esac
done
