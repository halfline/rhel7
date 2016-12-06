include Makefile.common
include Makefile.rhpkg
include Makefile.test

LANG=C
BUILD_DEFAULT_TARGET = rhel-${RHELMAJOR}.${RHELMINOR}-test-pesign
BUILD_SCRATCH_TARGET = rhel-${RHELMAJOR}.${RHELMINOR}-test-pesign

ifeq ("$(ZSTREAM)", "yes")
  ifeq ("$(origin RHDISTGIT_BRANCH)", "command line")
    __ZSTREAM = branch
  else
    __ZSTREAM = yes
  endif
else
  __ZSTREAM = no
endif

#ifeq ($(shell git diff --quiet HEAD && git describe --exact-match 2>/dev/null && echo ok),)
BUILD_TARGET ?= --scratch $(BUILD_SCRATCH_TARGET)
#else
#BUILD_TARGET ?= $(BUILD_DEFAULT_TARGET)
#endif

RHGITURL:=$(shell git config rhg.url || git config remote.origin.pushurl || git config remote.origin.url)
RHGITCOMMIT?=$(shell git log -1 --pretty=format:%H)

# this section is needed in order to make O= to work
_OUTPUT := ..
ifeq ("$(origin O)", "command line")
  _OUTPUT := $(O)
  _EXTRA_ARGS := O=$(_OUTPUT)
endif

ARCHCONFIG := $(shell uname -m | sed -e s/x86_64/X86_64/ \
				     -e s/s390x/S390/ -e s/ppc.*/PPC/ )

include Makefile.cross

default: rh-help

rh-kabi:
	@for KABIARCH in "ppc64" "ppc64le" "x86_64" "s390x"; do \
		$(REDHAT)/kabi/show-kabi -k $(REDHAT)/kabi/kabi-module/ -m \
			-a $$KABIARCH -r $(RHELMAJOR).$(RHELMINOR) > $(REDHAT)/kabi/Module.kabi_$$KABIARCH;\
		for i in {0..$(RHELMINOR)}; do \
			mkdir -p $(REDHAT)/kabi/kabi-rhel$(RHELMAJOR)$$i/;\
			$(REDHAT)/kabi/show-kabi -k $(REDHAT)/kabi/kabi-module/ -s -a $$KABIARCH \
			-r $(RHELMAJOR).$$i > $(REDHAT)/kabi/kabi-rhel$(RHELMAJOR)$$i/kabi_whitelist_$$KABIARCH;\
		done;\
	done;
	@(cd $(REDHAT)/kabi/ && ln -Tsf kabi-rhel$(RHELMAJOR)$(RHELMINOR) kabi-current)

rh-check-kabi: rh-kabi
	@if [ ! -e $(_OUTPUT)/Module.symvers ]; then \
		echo "ERROR: You must compile the kernel and modules first";\
		exit 1;\
	fi
	@$(REDHAT)/kabi/check-kabi -k $(REDHAT)/kabi/Module.kabi_$(MACH) \
	 -s $(_OUTPUT)/Module.symvers

rh-configs:
	cd $(REDHAT)/configs; ./build_configs.sh $(RPMVERSION)

rh-configs-prep: rh-configs

rh-configs-arch-prep:
	cd $(REDHAT)/configs; ./build_configs.sh $(RPMVERSION) $(MACH)

rh-clean-configs:
	cd $(REDHAT)/configs; rm -f kernel-*.config

rh-clean-sources:
	@rm -f $(RPM)/SPECS/*
	@for i in $(SOURCES)/*; do \
		rm -f $$i; \
	done;

rh-clean-rpmdirs:
	@for i in $(RPM)/{BUILD,SRPMS,RPMS,SPECS}/*; do \
		rm -rf $$i; \
	done;

rh-clean: rh-clean-sources rh-clean-configs rh-clean-rpmdirs

rh-stub-key:
	@echo "Copying pre-generated keys";
	@echo "*** THIS IS NOT RECOMMENDED ***";
	@echo "To be safe, keys should be created once for every build";
	@echo "Use this option only for development builds";
	@cp keys/stub_key.x509 $(_OUTPUT)/;
	@cp keys/stub_key.priv $(_OUTPUT)/;

# force tarball to be regenerated if HEAD changes
.PHONY:	$(TARBALL)
$(TARBALL):
	@scripts/create-tarball.sh $(GITID) $(TARBALL) linux-$(KVERSION)-$(PKGRELEASE)

.PHONY: $(KABI_TARBALL)
$(KABI_TARBALL):
	@(cd kabi && tar cjvf $(SOURCES)/$(KABI_TARFILE) kabi-rhel$(RHELMAJOR)* kabi-current)

setup-source: rh-clean-sources
	@cp $(REDHAT)/$(SPECFILE).template $(SOURCES)/$(SPECFILE)
	@$(REDHAT)/genspec.sh $(SOURCES) $(SOURCES)/$(SPECFILE) $(PKGRELEASE) $(RPMVERSION) $(RELEASED_KERNEL) $(SPECRELEASE) $(DISTRO_BUILD) $(__ZSTREAM)
	@cp $(SOURCES)/$(SPECFILE) $(SOURCES)/../SPECS/

sources-rh: $(TARBALL) rh-configs rh-kabi
	@cp -l $(TARBALL) $(SOURCES)/ || cp $(TARBALL) $(SOURCES)/
	@touch $(TESTPATCH)
	@git diff --no-renames HEAD > $(TESTPATCH).tmp
	@# 1) filterdiff will return crap from the patches it just filtered,
	@#    that's why egrep is needed so if there're changes under redhat/
	@#    but not everywhere else, it will be empty just like
	@#    linux-kernel-test.patch
	@# 2) egrep -v will return "1" if it sucessfully removed index and diff
	@#    lines, which will be considered an error
	@($(FILTERDIFF) $(TESTPATCH).tmp | egrep -v "^index|^diff" >$(TESTPATCH).tmp2; true)
	@mv $(TESTPATCH).tmp2 $(TESTPATCH).tmp
	@diff $(TESTPATCH).tmp $(TESTPATCH) > /dev/null || \
		echo "WARNING: There are uncommitted changes in your tree or the changes are not in sync with linux-kernel-test.patch.  Either commit the changes or run 'make rh-test-patch'"
	@rm $(TESTPATCH).tmp
	@cp $(TESTPATCH) $(SOURCES)/linux-kernel-test.patch
	@# if the extra_certificates doesn't exist, create a empty one
	@(if [ ! -f keys/extra_certificates ]; then \
		touch keys/extra_certificates; \
	fi)
	@cp cpupower.* x509.genkey keys/rhel*.x509 keys/extra_certificates Makefile.common kabi/check-kabi kabi/Module.kabi_{ppc64,ppc64le,s390x,x86_64} configs/kernel-*.config rhpkg/Makefile scripts/sign-modules keys/*.cer $(SOURCES)/
	@(cd kabi && tar cjvf $(SOURCES)/$(KABI_TARFILE) kabi-rhel$(RHELMAJOR)* kabi-current)

rh-sources: setup-source sources-rh

rh-test-patch:
	@git diff --no-renames HEAD > $(TESTPATCH);
	@($(FILTERDIFF) $(TESTPATCH) | egrep -v "^index|^diff" >$(TESTPATCH).tmp; true)
	@mv $(TESTPATCH).tmp $(TESTPATCH);

rh-all-rpms: rh-sources
	$(RPMBUILD) --define "_sourcedir $(SOURCES)" --define "_builddir $(RPM)/BUILD" --define "_srcrpmdir $(RPM)/SRPMS" --define "_rpmdir $(RPM)/RPMS" --define "_specdir $(RPM)/SPECS" --define "dist $(DIST)" --target $(MACH) -ba $(RPM)/SOURCES/kernel.spec

rh-srpm: rh-sources
	$(RPMBUILD) --define "_sourcedir $(SOURCES)" --define "_builddir $(RPM)/BUILD" --define "_srcrpmdir $(RPM)/SRPMS" --define "_rpmdir $(RPM)/RPMS" --define "_specdir $(RPM)/SPECS" --define "dist $(DIST)" --nodeps -bs $(RPM)/SOURCES/kernel.spec

rh-srpm-gcov:
	make rh-sources BUILDID=".gcov"
	sed -i 's/%define with_gcov.*/%define with_gcov 1/' $(RPM)/SOURCES/kernel.spec
	$(RPMBUILD) --define "_sourcedir $(SOURCES)" --define "_builddir $(RPM)/BUILD" --define "_srcrpmdir $(RPM)/SRPMS" --define "_rpmdir $(RPM)/RPMS" --define "_specdir $(RPM)/SPECS" --define "dist $(DIST)" --nodeps -bs $(RPM)/SOURCES/kernel.spec

rh-rpms: rh-sources
	$(RPMBUILD) --define "_sourcedir $(SOURCES)" --define "_builddir $(RPM)/BUILD" --define "_srcrpmdir $(RPM)/SRPMS" --define "_rpmdir $(RPM)/RPMS" --define "_specdir $(RPM)/SPECS" --define "dist $(DIST)" --target $(MACH) --target noarch -bb $(RPM)/SOURCES/kernel.spec

rh-kernel-%: rh-sources
	$(RPMBUILD) --define "_sourcedir $(SOURCES)" --define "_builddir $(RPM)/BUILD" --define "_srcrpmdir $(RPM)/SRPMS" --define "_rpmdir $(RPM)/RPMS" --define "_specdir $(RPM)/SPECS" --define "dist $(DIST)" --target $(MACH) --with $* --without vdso_install --without perf --without tools -bb $(RPM)/SOURCES/kernel.spec

rh-prep: rh-sources
	$(RPMBUILD) --define "_sourcedir $(SOURCES)" --define "_builddir $(RPM)/BUILD" --define "_srcrpmdir $(RPM)/SRPMS" --define "_rpmdir $(RPM)/RPMS" --define "_specdir $(RPM)/SPECS" --define "dist $(DIST)" --nodeps --target noarch -bp $(RPM)/SOURCES/kernel.spec

rh-perf: rh-sources
	$(RPMBUILD) --define "_sourcedir $(SOURCES)" --define "_builddir $(RPM)/BUILD" --define "_srcrpmdir $(RPM)/SRPMS" --define "_rpmdir $(RPM)/RPMS" --define "_specdir $(RPM)/SPECS" --define "dist $(DIST)" --without up --without smp --without kdump --without debug --without doc --without headers --without  --without doc --without debuginfo --target $(MACH) -bb $(RPM)/SOURCES/kernel.spec

# unless you know what you're doing, you don't want to use the next three ones
rh-release-finish: setup-source
	@$(REDHAT)/scripts/update_changelog.sh $(REDHAT) "$(STAMP_VERSION)-$(PREBUILD)$(BUILD)$(DIST)$(BUILDID)"
	@git commit -s ../Makefile Makefile.common kernel.spec.template -m "[redhat] kernel-$(STAMP_VERSION)-$(PREBUILD)$(BUILD)$(DIST)$(BUILDID)"
	@$(MAKE) rh-configs
	@$(MAKE) rh-kabi
	@$(MAKE) sources-rh
rh-release: rh-clean-sources
	@$(REDHAT)/scripts/new_release.sh $(REDHAT) $(__ZSTREAM)
	@$(MAKE) rh-release-finish
rh-release-tag:
	@git tag -a -m "kernel-$(STAMP_VERSION)-$(PKGRELEASE)" kernel-$(STAMP_VERSION)-$(PKGRELEASE)

.PHONY: rh-brew rh-koji
rh-brew : BUILD_FLAGS ?= $(BREW_FLAGS) $(TEST_FLAGS)
rh-koji : BUILD_FLAGS ?= $(KOJI_FLAGS) $(TEST_FLAGS)
rhg-brew: BUILD_FLAGS ?= $(BREW_FLAGS) $(TEST_FLAGS)
rhg-koji: BUILD_FLAGS ?= $(KOJI_FLAGS) $(TEST_FLAGS)

rh-brew rh-koji: rh-%: rh-srpm
	$* build $(BUILD_FLAGS) $(BUILD_TARGET) $(SRPMS)/kernel-$(KVERSION)-$(PKGRELEASE).src.rpm $(OUTPUT_FILE)

rhg-brew rhg-koji: rhg-%:
	$* build $(BUILD_FLAGS) $(BUILD_TARGET) "$(RHGITURL)?redhat/koji#$(RHGITCOMMIT)"

$(REDHAT)/rpm/SOURCES/kernel.spec:
	@echo "rh-sources"
	@$(MAKE) rh-sources

rh-dist-git: $(REDHAT)/rpm/SOURCES/kernel.spec $(TARBALL) $(KABI_TARBALL)
ifeq ("$(RHDISTGIT_BRANCH)", "")
 $(error RHDISTGIT_BRANCH unset)
endif
	$(REDHAT)/scripts/rh-dist-git.sh "$(RHDISTGIT_BRANCH)" "$(RHDISTGIT_CACHE)" "$(RHDISTGIT_TMP)" "$(RHDISTGIT)" "$(TARBALL)" "$(KABI_TARBALL)" "$(__ZSTREAM)"

rh-rtg: rh-release
	@$(MAKE) rh-release-tag
	@$(MAKE) rh-dist-git

# RH_LATEST returns the value of the latest "known good" kernel from brew.
# This should not be confused with the latest top-of-tree development tag.
RH_LATEST:=$(shell brew latest-pkg --quiet rhel-${RHELMAJOR}.${RHELMINOR}-candidate kernel | awk ' { print $$1 } ')

rh-get-latest:
	@echo "The latest kernel package tag is ${RH_LATEST}."

rh-help:
	@echo  'Cleaning targets:'
	@echo  '  rh-clean            - Do rh-clean-sources, rh-clean-configs, & rh-clean-rpmdirs'
	@echo  '  rh-clean-sources    - Clean the redhat/rpm/SOURCES/ directory'
	@echo  '  rh-clean-configs    - Clean the redhat/configs/ directory'
	@echo  '  rh-clean-rpmdirs    - Clean the redhat/rpm/{BUILD,SRPMS,RPMS,SPECS}/ directories'
	@echo  ''
	@echo  'Building targets:'
	@echo  ' All RPM/SRPM files will be put under the redhat/rpm/ directory'
	@echo  ''
	@echo  '  rh-srpm	- Create a source RPM and put it into the redhat/rpm/SRPMS/ directory'
	@echo  '  rh-srpm-gcov	- Create a source RPM with gcov enabled and put it into the redhat/rpm/SRPMS/ directory'
	@echo  '  rh-brew	- Create a kernel SRPM and then call brew to build the created SRPM'
	@echo  '  rh-koji	- Create a kernel SRPM and then call koji to build the created SRPM'
	@echo  '  rhg-brew	- Pass HEAD of the current git branch to brew to build an RPM set.'
	@echo  '                  Preceed make command by RHGITCOMMIT=<commitID> to specify commit ID to use.'
	@echo  '  rhg-koji	- Pass HEAD of the current git branch to koji to build an RPM set.'
	@echo  '                  Preceed make command by RHGITCOMMIT=<commitID> to specify commit ID to use.'
	@echo  '  rh-test	- Pass in brew task number, TASK=1234 make rh-test, to use for an RHTS test'
	@echo  '		  setup ./tests to use a test from redhat/tests (minus the .xml)'
	@echo  '		  uses a known stable RHEL6 distro as the default'
	@echo  '		  use TASK=None to use a non-scratch build based on RELEASE string'
	@echo  '		  see output for possible override options (USER_ID,DISTRO,FAMILY,..)'
	@echo  '  rh-brew-test  - Calls rh-brew and passes the task number into an RHTS test. See rh-test for more'
	@echo  '  rh-rpms	- Create the binary RPMS for the kernel'
	@echo  '  rh-kernel-<type> - Create  binary RPMS for a particular kernel type'
	@echo  '                   - <type> can be: baseonly or dbgonly'

	@echo  '  rh-all-rpms	- Create the binary RPMS and the SRPM for the kernel'
	@echo  '  rh-prep	- Setup the redhat/rpm/BUILD/ directory with the kernel source'
	@echo  '  rh-test-patch - Create a diff against HEAD and put it in linux-kernel-test.patch.'
	@echo  '                  Then linux-kernel-test.patch will be added to the kernel build'
	@echo  '  rh-kabi - Merge kabi structure into simple files'
	@echo  '  rh-check-kabi - Run check-kabi script on a pre-compiled tree.'
	@echo  '  rh-stub-key	- Use pre generated keys to speed local test builds'
	@echo  '  rh-cross-download - [x86_64 only] download cross compiler rpms'
	@echo  '  rh-cross-all-builds - [x86_64 only] execute "rpmbuild -bc" for i686, s390x,'
	@echo  '			ppc64, x86_64 using RHEL cross compiler'
	@echo  '  rh-cross-[ARCH]-build - [x86_64 only] execute "rpmbuild -bc" for specified'
	@echo  '			  ARCH using RHEL cross compiler'
	@echo  '  rh-cross-all-rpms - [x86_64 only] execute rpm builds for i686, s390x,'
	@echo  '		      ppc64, x86_64 using RHEL cross compiler'
	@echo  '  rh-cross-[ARCH]-rpm - [x86_64 only] execute rpm builds for specified'
	@echo  '			ARCH using RHEL cross compiler'
	@echo  '  rh-get-latest - returns latest-pkg tag from brew'

	@echo  ''
	@echo  'Configuration targets:'
	@echo
	@echo  '  rh-configs  - Creates config files for RHEL 7 architectures, cleans them'
	@echo  '		by running make nonint_oldconfig, and copies them to'
	@echo  '		redhat/configs/ directory. This is the target to use for a config!'
	@echo  '		Copy the config file you want from the redhat/configs/ directory'
	@echo  '		to .config'
	@echo  ''
