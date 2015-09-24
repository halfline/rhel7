#!/bin/bash
if [ -z "$1" -o -z "$2" ]; then
	echo "$(basename $0) <redhat path> <zstream? no/yes/branch>" >&2;
	exit 1;
fi

RHPATH="$1";
ZSTREAM_FLAG="$2";

if [ -s "$RHPATH/linux-kernel-test.patch" ]; then
	echo "linux-kernel-test.patch is not empty, aborting" >&2;
	exit 1;
fi

RELEASE=$(sed -n -e 's/^BUILD:=\(.*\)/\1/p' $RHPATH/Makefile.common);

YVER=$(echo $RELEASE | sed -n 's/^\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)$/\1/p')
YVER=${YVER:=$RELEASE}
ZMAJ=$(echo $RELEASE | sed -n 's/^\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)$/\2/p')
ZMAJ=${ZMAJ:=0}
ZMIN=$(echo $RELEASE | sed -n 's/^\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)$/\3/p')
ZMIN=${ZMIN:=0}

if [ "$ZSTREAM_FLAG" == "no" ]; then
	NEW_RELEASE="$[RELEASE + 1]";
elif [ "$ZSTREAM_FLAG" == "yes" ]; then
	NEW_RELEASE=$YVER.$((ZMAJ+1)).1;
elif [ "$ZSTREAM_FLAG" == "branch" ]; then
	NEW_RELEASE=$YVER.$ZMAJ.$((ZMIN+1));
else
	echo "$(basename $0) invalid <zstream> value, allowed [no|yes|branch]" >&2;
	exit 1;
fi

sed -i -e "s/BUILD:=$RELEASE/BUILD:=$NEW_RELEASE/" $RHPATH/Makefile.common;

# update top Makefile
sed -i -e "s/RHEL_RELEASE\ =.*/RHEL_RELEASE\ =\ $NEW_RELEASE/" $RHPATH/../Makefile;

