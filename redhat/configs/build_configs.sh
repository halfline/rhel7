#!/bin/bash
#
# This script merges together the hierarchy of CONFIG_* files under generic
# and debug to form the necessary kernel-<version>-<arch>-<variant>.config
# files for building RHEL kernels, based on the contents of a control file

KVERREL=$1 # sets the kenrel version-release string used for config file naming
SUBARCH=$2 # defines a specific arch for use with rh-configs-arch-prep target

set errexit
set nounset

control_file="priority"

function combine_config_layer()
{
	dir=$1
	file="config-$(echo $dir | sed -e 's|/|-|g')"

	if [ $(ls $dir/ | grep -c "^CONFIG_") -eq 0 ]; then
		touch $file
		return
	fi

	cat $dir/CONFIG_* > $file

}

function merge_configs()
{
	archvar=$1
	arch=$(echo "$archvar" | cut -f1 -d"-")
	configs=$2
	name=kernel-$KVERREL-$archvar.config
	echo -n "Building $name ... "
	touch config-merging config-merged
	for config in $(echo $configs | sed -e 's/:/ /g')
	do
		perl merge.pl config-$config config-merging > config-merged
		mv config-merged config-merging
	done
	if [ "x$arch" == "xppc64" ]; then
		echo "# powerpc" > $name
	elif [ "x$arch" == "xppc64le" ]; then
		echo "# powerpc" > $name
	elif [ "x$arch" == "xs390x" ]; then
		echo "# s390" > $name
	else
		echo "# $arch" > $name
	fi
	sort config-merging >> $name
	rm -f config-merged config-merging
	echo "done"
}

function process_configs()
{
	cfg_dir=$(pwd)
	kroot=$(cd ../..; pwd)
	pushd $kroot/ > /dev/null
	for cfg in $cfg_dir/kernel-$KVERREL-$SUBARCH*.config
	do
		arch=$(head -1 $cfg | cut -b 3-)
		echo -n "Processing $cfg ... "
		make ARCH=$arch KCONFIG_CONFIG=$cfg listnewconfig > /dev/null || exit 1
		make ARCH=$arch KCONFIG_CONFIG=$cfg oldnoconfig > /dev/null || exit 1
		echo "# $arch" > ${cfg}.tmp
		cat "${cfg}" >> ${cfg}.tmp
		mv "${cfg}.tmp" "${cfg}"
		echo "done"
	done
	rm "$cfg_dir"/*.config.old
	popd > /dev/null

	echo "Processed config files are in $cfg_dir"
}

glist=$(find generic -type d)
dlist=$(find debug -type d)

for d in $glist $dlist
do
	combine_config_layer $d
done

while read line
do
	if [ $(echo "$line" | grep -c "^#") -ne 0 ]; then
		continue
	elif [ $(echo "$line" | grep -c "^$") -ne 0 ]; then
		continue
	else
		arch=$(echo "$line" | cut -f1 -d"=")
		configs=$(echo "$line" | cut -f2 -d"=")

		if [ -n "$SUBARCH" -a "$SUBARCH" != "$arch" ]; then
			continue
		fi
		merge_configs $arch $configs
	fi
done < $control_file

rm -f config-*

process_configs

