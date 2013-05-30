#!/bin/bash

# Copy required files to dist-git. Uses redhat/git/files to know which files
# to copy
#
# $1: git source tree directory
# $2: cloned tree

tree="$1";
cloned="$2";
redhat="$1/redhat";
sources="$redhat/rpm/SOURCES";
spec="$sources/kernel.spec";

function die
{
	echo "Error: $1" >&2;
	exit 1;
}

if [ -z "$tree" -o ! -d "$sources" ]; then
	die "\"$tree\" doesn't seem to be a valid kernel source tree";
fi

if [ ! -d "$cloned" ]; then
	die "\"$cloned\" doesn't seem to be a valid directory";
fi

cd $cloned/kernel || die "\"$cloned\" doesn't seem to have a dist-git clone";

# copy the other files
cp $(cat $redhat/git/files | sed -e "s,^,$sources/,") . || die "Unable to copy files";
git add $(cat $redhat/git/files);

exit 0;
