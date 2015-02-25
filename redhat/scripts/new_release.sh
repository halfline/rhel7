#!/bin/bash
if [ -z "$1" ]; then
	echo "$(basename $0) <redhat path>" >&2;
	exit 1;
fi

RHPATH="$1";

if [ -s "$RHPATH/linux-kernel-test.patch" ]; then
	echo "linux-kernel-test.patch is not empty, aborting" >&2;
	exit 1;
fi

RELEASE=$(sed -n -e 's/^BUILD:=\(.*\)/\1/p' $RHPATH/Makefile.common);
NEW_RELEASE="$[RELEASE + 1]";

sed -i -e "s/BUILD:=$RELEASE/BUILD:=$NEW_RELEASE/" $RHPATH/Makefile.common;

# update top Makefile
sed -i -e "s/RHEL_RELEASE\ =.*/RHEL_RELEASE\ =\ $NEW_RELEASE/" $RHPATH/../Makefile;

