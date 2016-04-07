#!/bin/sh

SOURCES=$1
SPECFILE=$2
PKGRELEASE=$3
RPMVERSION=$4
RELEASED_KERNEL=$5
SPECRELEASE=$6
DISTRO_BUILD=$7
ZSTREAM_FLAG=$8
clogf="$SOURCES/changelog"
# hide [redhat] entries from changelog
HIDE_REDHAT=1;
# hide entries for unsupported arches
HIDE_UNSUPPORTED_ARCH=1;
# override LC_TIME to avoid date conflicts when building the srpm
LC_TIME=
STAMP=$(echo $MARKER | cut -f 1 -d '-' | sed -e "s/v//");
RPM_VERSION="$RPMVERSION-$PKGRELEASE";

echo >$clogf

lasttag=$(git describe --match="kernel-${RPMVERSION}-*" --abbrev=0)
echo "Gathering new log entries since $lasttag"
git format-patch --first-parent --no-renames -k --stdout ${lasttag}.. | awk '
BEGIN{TYPE="PATCHJUNK"; }
	# add an entry to changelog
	function changelog(subjectline, nameline, zstream)
	{
		subj = substr(subjectline, 10);
		gsub(/%/, "", subj)
		name = substr(nameline, 7);
		pos=match(name, /</);
		name=substr(name,1,pos-2);
		bz=substr(BZ,11);
		zbz=substr(ZBZ,13);
		meta = "";
		if (zstream == "no") {
			if (bz != "") {
				meta = " [" bz "]";
			}
		} else {
			if (zbz != "") {
				if (bz != "") {
					meta = " [" zbz " " bz "]";
				} else {
					meta = " [" zbz "]";
				}
			}
		}
		cve = substr(CVE, 6);
		if (cve != "") {
			if (meta != "") {
				meta = meta " {" cve "}";
			} else {
				meta = " {" cve "}";
			}
		}

		print "- " subj " (" name ")" meta >> CLOGF;
	}

	#special separator, close previous patch
	/^From / { if (TYPE=="PATCHJUNK") {
			COMMIT=substr($0, 6, 40);
			TYPE="HEADER";
			LASTHDR="NEW";
			next;
		} }

	#interesting header stuff
	/^From: / { if (TYPE=="HEADER") {
			namestr=$0;
			#check for mime encoding on the email headers
			#git uses utf-8 q encoding
			if ( $0 ~ /=\?utf-8\?q/ ) {
				#get rid of the meta utf-8 junk
				gsub(/=\?utf-8\?q\?/, "");
				gsub(/\?=/, "");

				#translate each char
				n=split($0, a, "=");
				namestr = sprintf("%s", a[1]);
				for (i = 2; i <= n; ++i) {
					utf = substr(a[i], 0, 2);
					c = strtonum("0x" utf);
					namestr = sprintf("%s%c%s", namestr, c, substr(a[i],3));
				}
			}
			NAMELINE=namestr; next;
		    }
	    }
	/^Date: / {if (TYPE=="HEADER") {DATELINE=$0; next; } }
	/^Subject: / { if (TYPE=="HEADER") {SUBJECTLINE=$0; LASTHDR="SUBJ"; next; } }
	# partially attempt to deal with RFC2822 continuation lines in headers
	/^\ / { if (TYPE=="HEADER") { if (LASTHDR=="SUBJ") { SUBJECTLINE=(SUBJECTLINE $0); } next; } }
	/^Bugzilla: / { if (TYPE=="META") {BZ=$0; } }
	/^Z-Bugzilla: / { if (TYPE=="META") {ZBZ=$0; } }
	/^CVE: / { if (TYPE=="META") {CVE=$0; } }

	#blank line triggers end of header and to begin processing
	/^$/ { 
	    if (TYPE=="META") {
		#create the dynamic changelog entry
		changelog(SUBJECTLINE, NAMELINE, ZSTREAM);
		#reset cve values because they do not always exist
		CVE="";
		BZ="";
		ZBZ="";
		TYPE="BODY";
	    }
	    if (TYPE=="HEADER") {
		TYPE="META"; next;
	    }
	}

	#in order to handle overlapping keywords, we keep track of each
	#section of the patchfile and only process keywords in the correct section
	/^---$/ {
		if (TYPE=="META") {
			# no meta data found, just use the subject line to fill
			# the changelog
			changelog(SUBJECTLINE, NAMELINE, ZSTREAM);
			#reset cve values because they do not always exist
			CVE="";
			BZ="";
			ZBZ="";
			TYPE="BODY";
		}
		if (TYPE=="BODY") {
			TYPE="PATCHSEP";
		}
	}
	/^diff --git/ { if (TYPE=="PATCHSEP") { TYPE="PATCH"; } }
	/^-- $/ { if (TYPE=="PATCH") { TYPE="PATCHJUNK"; } }

	#filter out stuff we do not care about
	{ if (TYPE == "PATCHSEP") { next; } }
	{ if (TYPE == "PATCHJUNK") { next; } }
	{ if (TYPE == "HEADER") { next; } }

' SOURCES=$SOURCES SPECFILE=$SPECFILE CLOGF=$clogf ZSTREAM=$ZSTREAM_FLAG

cat $clogf | grep -v "tagging $RPM_VERSION" > $clogf.stripped
cp $clogf.stripped $clogf

if [ "x$HIDE_REDHAT" == "x1" ]; then
	cat $clogf | grep -v -e "^- \[redhat\]" |
		sed -e 's!\[Fedora\]!!g' > $clogf.stripped
	cp $clogf.stripped $clogf
fi

if [ "x$HIDE_UNSUPPORTED_ARCH" == "x1" ]; then
	cat $clogf | egrep -v "^- \[(alpha|arc|arm|arm64|avr32|blackfin|c6x|cris|frv|h8300|hexagon|ia64|m32r|m68k|metag|microblaze|mips|mn10300|openrisc|parisc|score|sh|sparc|tile|um|unicore32|xtensa)\]" > $clogf.stripped
	cp $clogf.stripped $clogf
fi

LENGTH=$(wc -l $clogf | awk '{print $1}')

#the changelog was created in reverse order
#also remove the blank on top, if it exists
#left by the 'print version\n' logic above
cname="$(git var GIT_COMMITTER_IDENT |sed 's/>.*/>/')"
cdate="$(LC_ALL=C date +"%a %b %d %Y")"
cversion="[$RPM_VERSION]";
tac $clogf | sed "1{/^$/d; /^- /i\
* $cdate $cname $cversion
	}" > $clogf.rev

if [ "$LENGTH" = 0 ]; then
	rm -f $clogf.rev; touch $clogf.rev
fi

# add extra description if localdesc file is found. useful for
# test builds that go to customer (added disclaimer)
EXTRA_DESC=../localdesc
if [ -f "$EXTRA_DESC" ]; then
       sed -i -e "/%%EXTRA_DESC/r $EXTRA_DESC" $SPECFILE
fi

test -n "$SPECFILE" &&
        sed -i -e "
	/%%CHANGELOG%%/r $clogf.rev
	/%%CHANGELOG%%/d
	/%%EXTRA_DESC%%/d
	s/%%RPMVERSION%%/$RPMVERSION/
	s/%%PKGRELEASE%%/$PKGRELEASE/
	s/%%SPECRELEASE%%/$SPECRELEASE/
	s/%%DISTRO_BUILD%%/$DISTRO_BUILD/
	s/%%RELEASED_KERNEL%%/$RELEASED_KERNEL/" $SPECFILE

rm -f $clogf{,.rev,.stripped};

