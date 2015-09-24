#!/bin/bash

# clones and updates a dist-git repo
# $1: branch to be used
# $2: local pristine clone of dist-git
# $3: alternate tmp directory (if you have faster storage)
# $4: alternate dist-git server
# $5: kernel source tarball
# $6: kabi whitelists tarball
# $7: zstream build

rhdistgit_branch=$1;
rhdistgit_cache=$2;
rhdistgit_tmp=$3;
rhdistgit_server=$4;
rhdistgit_tarball=$5;
rhdistgit_kabi_tarball=$6;
rhdistgit_zstream_flag=$7;

redhat=$(dirname $0)/..;
topdir=$redhat/..;

function die
{
	echo "Error: $1" >&2;
	exit 1;
}

function upload_kabi_tarball()
{
	echo "Uploading kernel-abi-whitelists tarball"
	sed -i "/kernel-abi-whitelist.*.tar.bz2/d" $tmpdir/kernel/sources;
	sed -i "/kernel-abi-whitelist.*.tar.bz2/d" $tmpdir/kernel/.gitignore;
	rhpkg upload $rhdistgit_kabi_tarball >/dev/null || die "uploading kabi tarball";
}

if [ -z "$rhdistgit_branch" ]; then
	echo "$0 <branch> [local clone] [alternate tmp] [alternate dist-git server]" >&2;
	exit 1;
fi

echo "Cloning the repository"
# clone the dist-git, considering cache
tmpdir=$($redhat/scripts/clone_tree.sh "$rhdistgit_server" "$rhdistgit_cache" "$rhdistgit_tmp");

echo "Switching the branch"
# change in the correct branch
cd $tmpdir/kernel;
rhpkg switch-branch $rhdistgit_branch || die "switching to branch $rhdistgit_branch";

echo "Copying updated files"
# copy the required files (redhat/git/files)
$redhat/scripts/copy_files.sh "$topdir" "$tmpdir"

echo "Uploading new tarballs"
# upload tarballs
sed -i "/linux-3.*.el7.tar.xz/d" $tmpdir/kernel/sources;
sed -i "/linux-3.*.el7.tar.xz/d" $tmpdir/kernel/.gitignore;
rhpkg upload $rhdistgit_tarball >/dev/null || die "uploading kernel tarball";

# Only upload kernel-abi-whitelists tarball if its release counter changed.
if [ "$rhdistgit_zstream_flag" == "no" ]; then
	whitelists_file="$(awk '/kernel-abi-whitelists/ {print $2}' $tmpdir/kernel/sources)"
	grep "$whitelists_file" $rhdistgit_kabi_tarball >/dev/null || upload_kabi_tarball;
fi

echo "Creating diff for review ($tmpdir/diff) and changelog"
# diff the result (redhat/cvs/dontdiff). note: diff reuturns 1 if
# differences were found
diff -X $redhat/git/dontdiff -upr $tmpdir/kernel $redhat/rpm/SOURCES/ > $tmpdir/diff;
# creating the changelog file
$redhat/scripts/create_distgit_changelog.sh $redhat/rpm/SOURCES/kernel.spec "$rhdistgit_zstream_flag" >$tmpdir/changelog

# all done
echo "$tmpdir"
