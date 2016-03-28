#!/bin/bash
# Download & install the latest EPEL7 repo information
# Argument(s) to this script is a list of rpm names to install

# if everything is installed then exit successfully
rpm -q $@ && exit 0

# install epel-release if necessary
rpm -q epel-release >& /dev/null
if [ $? -ne 0 ]; then
	wget -nd -r -l1 --no-parent -A "epel-release*.rpm" http://dl.fedoraproject.org/pub/epel/7/x86_64/e/
	rpm -ivh epel-release*.rpm
	# clean up
	rm -f epel-release*.rpm
fi

# install list of rpms for cross compile
yum -y install $@

exit 0
