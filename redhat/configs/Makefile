# Make rules for configuration files.

OUTPUT := $(abspath ../..)

ifeq ("$(origin O)", "command line")
  _EXTRA_ARGS := O=$(O)
  OUTPUT := $(O)
endif

CFG		= kernel-$(VERSION)
CFG_DIR		= $(OUTPUT)/configs

X86_64CONFIGFILES	=	$(CFG)-x86_64.config $(CFG)-x86_64-debug.config
S390CONFIGFILES	=	$(CFG)-s390x.config $(CFG)-s390x-kdump.config
PPCCONFIGFILES	=	$(CFG)-ppc64.config $(CFG)-ppc64-debug.config

CONFIGFILES	= $(X86_64CONFIGFILES) $(S390CONFIGFILES) $(PPCCONFIGFILES)

ARCHCONFIG	=

PLATFORMS	= x86_64 powerpc64 s390x 
TEMPFILES	= $(addprefix temp-, $(addsuffix -generic, $(PLATFORMS)))

configs: $($(ARCHCONFIG)CONFIGFILES)
	@rm -f kernel-*-config
	@rm -f temp-* $(TEMPFILES) *-merged

# Augment the clean target to clean up our own cruft
clean ::
	@rm -fv $(CONFIGFILES) $(TEMPFILES) temp-generic kernel-$(VERSION)*config *-merged

configs-prep: clean configs
	@mkdir -p $(CFG_DIR)
	@rm -f $(CFG_DIR)/*
	@cp $(CFG)* $(CFG_DIR)
	@cd ../../ && \
	for i in $(CFG_DIR)/*.config; do \
		mv $$i $(OUTPUT)/.config ; \
		Arch=$$(head -1 $(OUTPUT)/.config | cut -b 3-) ; \
		echo "Configuring $$i" ; \
		make ARCH=$$Arch listnewconfig $(_EXTRA_ARGS) > /dev/null || exit 1 ; \
		make ARCH=$$Arch oldnoconfig $(_EXTRA_ARGS) > /dev/null || exit 1 ; \
		echo "# $$Arch" > $$i ; \
		cat $(OUTPUT)/.config >> $$i ; \
		rm $(OUTPUT)/.config; \
	done

config-%-merged:
	@if [ -f $$(basename $@ -merged)-rhel ]; then \
		perl merge.pl $$(basename $@ -merged)-rhel $$(basename $@ -merged) >$@; \
	else \
		cp $$(basename $@ -merged) $@; \
	fi

temp-generic: config-generic-merged config-nodebug-merged
	perl merge.pl $^  > $@

temp-debug-generic: config-generic-merged config-debug-merged
	perl merge.pl $^  > $@

temp-x86-64: config-x86_64-generic-merged config-x86-generic-merged
	perl merge.pl $^  > $@

temp-x86_64-generic: temp-x86-64 temp-generic
	perl merge.pl $^  > $@

temp-x86_64-debug-generic: temp-x86-64 temp-debug-generic
	perl merge.pl $^  > $@

temp-powerpc-generic: config-powerpc-generic-merged temp-generic
	perl merge.pl $^  > $@

temp-powerpc-debug-generic: config-powerpc-generic-merged temp-debug-generic
	perl merge.pl $^  > $@

temp-s390-generic: config-s390x-merged temp-generic
	perl merge.pl $^ > $@

kernel-$(VERSION)-x86_64.config: /dev/null temp-x86_64-generic
	perl merge.pl $^ x86_64 > $@

kernel-$(VERSION)-x86_64-debug.config: /dev/null temp-x86_64-debug-generic
	perl merge.pl $^ x86_64 > $@

kernel-$(VERSION)-ppc64.config: config-powerpc64-merged temp-powerpc-generic
	perl merge.pl $^ powerpc > $@

kernel-$(VERSION)-ppc64-debug.config: config-powerpc64-merged temp-powerpc-debug-generic
	perl merge.pl $^ powerpc > $@

kernel-$(VERSION)-s390x.config: config-s390x-merged temp-s390-generic
	perl merge.pl $^ s390 > $@

kernel-$(VERSION)-s390x-kdump.config: config-s390x-kdump-merged kernel-$(VERSION)-s390x.config
	perl merge.pl $^ s390 > $@
