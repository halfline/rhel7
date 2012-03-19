#! /bin/sh

MARKER=$1
SOURCES=$2
SPECFILE=$3
PKGRELEASE=$4
RPMVERSION=$5
BUILDID=$6
clogf="$SOURCES/changelog"
# hide [redhat] entries from changelog
HIDE_REDHAT=0;
# override LC_TIME to avoid date conflicts when building the srpm
LC_TIME=
LASTCOMMIT=$(cat lastcommit);
STAMP=$(echo $MARKER | cut -f 1 -d '-' | sed -e "s/v//");
RELEASED_KERNEL="1";
RPM_VERSION="$RPMVERSION-$PKGRELEASE";

echo >$clogf

git format-patch --first-parent --no-renames -k --stdout $MARKER..|awk '
BEGIN{TYPE="PATCHJUNK"; dolog=0; }
	# add an entry to changelog
	function changelog(subjectline, nameline)
	{
		subj = substr(subjectline, 10);
		gsub(/%/, "", subj)
		name = substr(nameline, 7);
		pos=match(name, /</);
		name=substr(name,1,pos-2);
		bz=substr(BZ,11);
		meta = "";
		if (bz != "") {
			meta = " [" bz "]";
		}
		cve = substr(CVE, 6);
		if (cve != "") {
			if (meta != "") {
				meta = meta " {" cve "}";
			} else {
				meta = " {" cve "}";
			}
		}
		if ( COMMIT == LASTCOMMIT ) {
			dolog=1;
		} else {
			if (dolog == 1) {
				clog = "- " subj " (" name ")" meta;
				print clog >> CLOGF;
			}
		}
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
	/^CVE: / { if (TYPE=="META") {CVE=$0; } }

	#blank line triggers end of header and to begin processing
	/^$/ { 
	    if (TYPE=="META") {
		#create the dynamic changelog entry
		changelog(SUBJECTLINE, NAMELINE);
		#reset cve values because they do not always exist
		CVE="";
		BZ="";
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
			changelog(SUBJECTLINE, NAMELINE);
			#reset cve values because they do not always exist
			CVE="";
			BZ="";
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

' SOURCES=$SOURCES SPECFILE=$SPECFILE \
	CLOGF=$clogf LASTCOMMIT=$LASTCOMMIT

CONFIGS=configs/config.include
CONFIGS2=configs/config2.include
find configs/ -mindepth 1 -maxdepth 1 -name config-\* | grep -v merged | cut -f 2 -d '/' >$CONFIGS;
# Set this to a nice high starting point
count=50;
rm -f $CONFIGS2;
for i in $(cat $CONFIGS); do
	echo "Source$count: $i" >>$CONFIGS2;
	count=$((count+1));
done

#the changelog was created in reverse order
#also remove the blank on top, if it exists
#left by the 'print version\n' logic above
cname="$(git var GIT_COMMITTER_IDENT |sed 's/>.*/>/')"
cdate="$(date +"%a %b %d %Y")"
cversion="[$RPM_VERSION]";
tac $clogf | sed "1{/^$/d; /^- /i\
* $cdate $cname $cversion
	}" > $clogf.rev

cat $clogf.rev | grep -v "updating lastcommit for" |
	grep -v "tagging $RPM_VERSION" > $clogf.rev.stripped
cp $clogf.rev.stripped $clogf.rev

if [ $HIDE_REDHAT = 1 ]; then
	cat $clogf.rev | grep -v -e "^ \[redhat\]" |
		grep -v "Revert" |
		sed -e 's!\[Fedora\]!!g' > $clogf.rev.stripped
	cp $clogf.rev.stripped $clogf.rev
fi

test -n "$SPECFILE" &&
        sed -i -e "
	/%%CONFIGS%%/r $CONFIGS2
	/%%CONFIGS%%/d
	/%%CHANGELOG%%/r $clogf.rev
	/%%CHANGELOG%%/d
	s/%%RPMVERSION%%/$RPMVERSION/
	s/%%PKGRELEASE%%/$PKGRELEASE/
	s/%%RELEASED_KERNEL%%/$RELEASED_KERNEL/" $SPECFILE
if [ -n "$BUILDID" ]; then
	sed -i -e "s/# % define buildid .local/%define buildid $BUILDID/" $SPECFILE;
fi

rm -f $clogf $clogf.rev{,.stripped} $CONFIGS $CONFIGS2;

