# We don't want make removing intermediary stamps
.SECONDARY :

# Prepare the out-of-tree build directory
ifeq ($(do_full_source),true)
build_cd = cd $(builddir)/build-$*; #
build_O  =
else
build_cd =
build_O  = O=$(builddir)/build-$*
endif

# Typically supplied from the arch makefile, e.g., debian.master/control.d/armhf.mk
ifneq ($(gcc),)
kmake += CC=$(CROSS_COMPILE)$(gcc)
endif

shlibdeps_opts = $(if $(CROSS_COMPILE),-- -l$(CROSS_COMPILE:%-=/usr/%)/lib)

$(stampdir)/stamp-prepare-%: config-prepare-check-%
	@echo Debug: $@
	@touch $@
$(stampdir)/stamp-prepare-tree-%: target_flavour = $*
$(stampdir)/stamp-prepare-tree-%: $(commonconfdir)/config.common.$(family) $(archconfdir)/config.common.$(arch) $(archconfdir)/config.flavour.%
	@echo Debug: $@
	install -d $(builddir)/build-$*
	touch $(builddir)/build-$*/ubuntu-build
	[ "$(do_full_source)" != 'true' ] && true || \
		rsync -a --exclude debian --exclude debian.master --exclude $(DEBIAN) * $(builddir)/build-$*
	cat $^ | sed -e 's/.*CONFIG_VERSION_SIGNATURE.*/CONFIG_VERSION_SIGNATURE="Ubuntu $(release)-$(revision)-$* $(raw_kernelversion)"/' > $(builddir)/build-$*/.config
	find $(builddir)/build-$* -name "*.ko" | xargs rm -f
	$(build_cd) $(kmake) $(build_O) -j1 syncconfig prepare scripts
	touch $@

# Used by developers as a shortcut to prepare a tree for compilation.
prepare-%: $(stampdir)/stamp-prepare-%
	@echo Debug: $@
# Used by developers to allow efficient pre-building without fakeroot.
build-%: $(stampdir)/stamp-build-%
	@echo Debug: $@

define build_zfs =
	#
	# SPL/ZFS wants a fully built kernel before you can configure and build.
	# It seems to be impossible to tease out the application configuration
	# from the modules, but at least one can build just the modules.
	#
	install -d $(builddir)/build-$*/spl
	rsync -a --exclude=dkms.conf --delete spl/ $(builddir)/build-$*/spl/
	cd $(builddir)/build-$*/spl; sh autogen.sh; sh configure $(splopts)
	$(kmake) -C $(builddir)/build-$*/spl/module $(conc_level)

	install -d $(builddir)/build-$*/zfs
	rsync -a --exclude=dkms.conf --delete zfs/ $(builddir)/build-$*/zfs/
	cd $(builddir)/build-$*/zfs; sh autogen.sh; sh configure $(zfsopts)
	$(kmake) -C $(builddir)/build-$*/zfs/module $(conc_level)
endef

# Do the actual build, including image and modules
$(stampdir)/stamp-build-%: target_flavour = $*
$(stampdir)/stamp-build-%: splopts  = --with-linux=$(CURDIR)
$(stampdir)/stamp-build-%: splopts += --with-linux-obj=$(builddir)/build-$*
$(stampdir)/stamp-build-%: zfsopts  = $(splopts)
$(stampdir)/stamp-build-%: zfsopts += --with-spl=$(builddir)/build-$*/spl
$(stampdir)/stamp-build-%: zfsopts += --with-spl-obj=$(builddir)/build-$*/spl
$(stampdir)/stamp-build-%: zfsopts += --prefix=/usr --with-config=kernel
$(stampdir)/stamp-build-%: bldimg = $(call custom_override,build_image,$*)
$(stampdir)/stamp-build-%: enable_zfs = $(call custom_override,do_zfs,$*)
$(stampdir)/stamp-build-%: $(stampdir)/stamp-prepare-%
	@echo Debug: $@ build_image $(build_image) bldimg $(bldimg)
	$(build_cd) $(kmake) $(build_O) $(conc_level) $(bldimg) modules $(if $(filter true,$(do_dtbs)),dtbs)

	$(if $(filter true,$(enable_zfs)),$(call build_zfs))

	@touch $@

define install_zfs =
	cd $(builddir)/build-$*/spl/module; \
		$(kmake) -C $(builddir)/build-$* SUBDIRS=`pwd` modules_install $(splopts)
	cd $(builddir)/build-$*/zfs/module; \
		$(kmake) -C $(builddir)/build-$* SUBDIRS=`pwd` modules_install $(zfsopts)
endef

define install_control =
	for which in $(3);							\
	do									\
		template="$(DROOT)/templates/$(2).$$which.in";			\
		script="$(DROOT)/$(1).$$which";					\
		sed -e 's/@abiname@/$(abi_release)/g'				\
		    -e 's/@localversion@/-$*/g'					\
		    -e 's/@image-stem@/$(instfile)/g'				\
			<"$$template" >"$$script";				\
	done
endef

# Install the finished build
install-%: pkgdir_bin = $(CURDIR)/debian/$(bin_pkg_name)-$*
install-%: pkgdir = $(CURDIR)/debian/$(mods_pkg_name)-$*
install-%: pkgdir_ex = $(CURDIR)/debian/$(mods_extra_pkg_name)-$*
install-%: bindoc = $(pkgdir)/usr/share/doc/$(bin_pkg_name)-$*
install-%: dbgpkgdir = $(CURDIR)/debian/$(bin_pkg_name)-$*-dbgsym
install-%: signingv = $(CURDIR)/debian/$(bin_pkg_name)-signing/$(release)-$(revision)
install-%: toolspkgdir = $(CURDIR)/debian/$(tools_flavour_pkg_name)-$*
install-%: cloudpkgdir = $(CURDIR)/debian/$(cloud_flavour_pkg_name)-$*
install-%: basepkg = $(hdrs_pkg_name)
install-%: indeppkg = $(indep_hdrs_pkg_name)
install-%: kernfile = $(call custom_override,kernel_file,$*)
install-%: instfile = $(call custom_override,install_file,$*)
install-%: hdrdir = $(CURDIR)/debian/$(basepkg)-$*/usr/src/$(basepkg)-$*
install-%: target_flavour = $*
install-%: MODHASHALGO=sha512
install-%: MODSECKEY=$(builddir)/build-$*/certs/signing_key.pem
install-%: MODPUBKEY=$(builddir)/build-$*/certs/signing_key.x509
install-%: build_dir=$(builddir)/build-$*
install-%: enable_zfs = $(call custom_override,do_zfs,$*)
install-%: splopts  = INSTALL_MOD_STRIP=1
install-%: splopts += INSTALL_MOD_PATH=$(pkgdir)/
install-%: splopts += INSTALL_MOD_DIR=kernel/zfs
install-%: splopts += $(conc_level)
install-%: zfsopts  = $(splopts)
install-%: checks-%
	@echo Debug: $@ kernel_file $(kernel_file) kernfile $(kernfile) install_file $(install_file) instfile $(instfile)
	dh_testdir
	dh_testroot
	dh_prep -p$(bin_pkg_name)-$*
	dh_prep -p$(mods_pkg_name)-$*
	dh_prep -p$(hdrs_pkg_name)-$*
ifneq ($(skipdbg),true)
	dh_prep -p$(bin_pkg_name)-$*-dbgsym
endif

	# The main image
	# compress_file logic required because not all architectures
	# generate a zImage automatically out of the box
ifeq ($(compress_file),)
	install -m600 -D $(builddir)/build-$*/$(kernfile) \
		$(pkgdir_bin)/boot/$(instfile)-$(abi_release)-$*
else
	install -d $(pkgdir_bin)/boot
	gzip -c9v $(builddir)/build-$*/$(kernfile) > \
		$(pkgdir_bin)/boot/$(instfile)-$(abi_release)-$*
	chmod 600 $(pkgdir_bin)/boot/$(instfile)-$(abi_release)-$*
endif

ifeq ($(uefi_signed),true)
	install -d $(signingv)
	# Check to see if this supports handoff, if not do not sign it.
	# Check the identification area magic and version >= 0x020b
	handoff=`dd if="$(pkgdir_bin)/boot/$(instfile)-$(abi_release)-$*" bs=1 skip=514 count=6 2>/dev/null | od -s | gawk '($$1 == 0 && $$2 == 25672 && $$3 == 21362 && $$4 >= 523) { print "GOOD" }'`; \
	if [ "$$handoff" = "GOOD" ]; then \
		cp -p $(pkgdir_bin)/boot/$(instfile)-$(abi_release)-$* \
			$(signingv)/$(instfile)-$(abi_release)-$*.efi; \
	fi
endif
ifeq ($(opal_signed),true)
	install -d $(signingv)
	cp -p $(pkgdir_bin)/boot/$(instfile)-$(abi_release)-$* \
		$(signingv)/$(instfile)-$(abi_release)-$*.opal;
endif

	install -d $(pkgdir)/boot
	install -m644 $(builddir)/build-$*/.config \
		$(pkgdir)/boot/config-$(abi_release)-$*
	install -m644 $(abidir)/$* \
		$(pkgdir)/boot/abi-$(abi_release)-$*
	install -m644 $(abidir)/$*.retpoline \
		$(pkgdir)/boot/retpoline-$(abi_release)-$*
	install -m600 $(builddir)/build-$*/System.map \
		$(pkgdir)/boot/System.map-$(abi_release)-$*
	if [ "$(filter true,$(do_dtbs))" ]; then \
		$(build_cd) $(kmake) $(build_O) $(conc_level) dtbs_install \
			INSTALL_DTBS_PATH=$(pkgdir)/lib/firmware/$(abi_release)-$*/device-tree; \
		( cd $(pkgdir)/lib/firmware/$(abi_release)-$*/ && find device-tree -print ) | \
		while read dtb_file; do \
			echo "$$dtb_file ?" >> $(DEBIAN)/d-i/firmware/$(arch)/kernel-image; \
		done; \
	fi
ifeq ($(no_dumpfile),)
	makedumpfile -g $(pkgdir)/boot/vmcoreinfo-$(abi_release)-$* \
		-x $(builddir)/build-$*/vmlinux
	chmod 0600 $(pkgdir)/boot/vmcoreinfo-$(abi_release)-$*
endif

	$(build_cd) $(kmake) $(build_O) $(conc_level) modules_install $(vdso) \
		INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=$(pkgdir)/ \
		INSTALL_FW_PATH=$(pkgdir)/lib/firmware/$(abi_release)-$*

	$(if $(filter true,$(enable_zfs)),$(call install_zfs))

	#
	# Build module blacklists:
	#  - blacklist all watchdog drivers (LP:1432837)
	#
	install -d $(pkgdir)/lib/modprobe.d
	echo "# Kernel supplied blacklist for $(src_pkg_name) $(abi_release)-$* $(arch)" \
		>$(pkgdir)/lib/modprobe.d/blacklist_$(src_pkg_name)_$(abi_release)-$*.conf
	for conf in $(arch)-$* $(arch) common.conf; do \
		if [ -f $(DEBIAN)/modprobe.d/$$conf ]; then \
			echo "# modprobe.d/$$conf"; \
			cat $(DEBIAN)/modprobe.d/$$conf; \
		fi; \
	done >>$(pkgdir)/lib/modprobe.d/blacklist_$(src_pkg_name)_$(abi_release)-$*.conf
	echo "# Autogenerated watchdog blacklist" \
		>>$(pkgdir)/lib/modprobe.d/blacklist_$(src_pkg_name)_$(abi_release)-$*.conf
	ls -1 $(pkgdir)/lib/modules/$(abi_release)-$*/kernel/drivers/watchdog/ | \
		grep -v '^bcm2835_wdt$$' | \
		sed -e 's/^/blacklist /' -e 's/.ko$$//' | \
		sort -u \
		>>$(pkgdir)/lib/modprobe.d/blacklist_$(src_pkg_name)_$(abi_release)-$*.conf

ifeq ($(do_extras_package),true)
	#
	# Remove all modules not in the inclusion list.
	#
	if [ -f $(DEBIAN)/control.d/$(target_flavour).inclusion-list ] ; then \
		/sbin/depmod -v -b $(pkgdir) $(abi_release)-$* | \
			sed -e "s@$(pkgdir)/lib/modules/$(abi_release)-$*/kernel/@@g" | \
			awk '{ print $$1 " " $$NF}' >$(build_dir)/module-inclusion.depmap; \
		mkdir -p $(pkgdir_ex)/lib/modules/$(abi_release)-$*; \
		mv $(pkgdir)/lib/modules/$(abi_release)-$*/kernel \
			$(pkgdir_ex)/lib/modules/$(abi_release)-$*/kernel; \
		$(SHELL) $(DROOT)/scripts/module-inclusion --master \
			$(pkgdir_ex)/lib/modules/$(abi_release)-$*/kernel \
			$(pkgdir)/lib/modules/$(abi_release)-$*/kernel \
			$(DEBIAN)/control.d/$(target_flavour).inclusion-list \
			$(build_dir)/module-inclusion.depmap 2>&1 | \
				tee $(target_flavour).inclusion-list.log; \
		/sbin/depmod -b $(pkgdir) -ea -F $(pkgdir)/boot/System.map-$(abi_release)-$* \
			$(abi_release)-$* 2>&1 |tee $(target_flavour).depmod.log; \
		if [ `grep -c 'unknown symbol' $(target_flavour).depmod.log` -gt 0 ]; then \
			echo "EE: Unresolved module dependencies in base package!"; \
			exit 1; \
		fi \
	fi
endif

ifeq ($(no_dumpfile),)
	makedumpfile -g $(pkgdir)/boot/vmcoreinfo-$(abi_release)-$* \
		-x $(builddir)/build-$*/vmlinux
	chmod 0600 $(pkgdir)/boot/vmcoreinfo-$(abi_release)-$*
endif
	rm -f $(pkgdir)/lib/modules/$(abi_release)-$*/build
	rm -f $(pkgdir)/lib/modules/$(abi_release)-$*/source

	# Some initramfs-tools specific modules
	install -d $(pkgdir)/lib/modules/$(abi_release)-$*/initrd
	if [ -f $(pkgdir)/lib/modules/$(abi_release)-$*/kernel/drivers/video/vesafb.ko ]; then\
	  $(LN) $(pkgdir)/lib/modules/$(abi_release)-$*/kernel/drivers/video/vesafb.ko \
		$(pkgdir)/lib/modules/$(abi_release)-$*/initrd/; \
	fi

	echo "interest linux-update-$(abi_release)-$*" >"$(DROOT)/$(bin_pkg_name)-$*.triggers"
	$(call install_control,$(bin_pkg_name)-$*,image,postinst postrm preinst prerm)
	$(call install_control,$(mods_pkg_name)-$*,extra,postinst postrm)
ifeq ($(do_extras_package),true)
	# Install the postinit/postrm scripts in the extras package.
	if [ -f $(DEBIAN)/control.d/$(target_flavour).inclusion-list ] ; then	\
		$(call install_control,$(mods_extra_pkg_name)-$*,extra,postinst postrm); \
	fi
endif

	# Install the full changelog.
ifeq ($(do_doc_package),true)
	install -d $(bindoc)
	cat $(DEBIAN)/changelog $(DEBIAN)/changelog.historical | \
		gzip -9 >$(bindoc)/changelog.Debian.old.gz
	chmod 644 $(bindoc)/changelog.Debian.old.gz
endif

ifneq ($(skipsub),true)
	for sub in $($(*)_sub); do					\
		if ! (TO=$$sub FROM=$* ABI_RELEASE=$(abi_release) $(SHELL)		\
			$(DROOT)/scripts/sub-flavour); then exit 1; fi;		\
		/sbin/depmod -b debian/$(bin_pkg_name)-$$sub		\
			-ea -F debian/$(bin_pkg_name)-$$sub/boot/System.map-$(abi_release)-$* \
			$(abi_release)-$*;					\
		$(call install_control,$(bin_pkg_name)--$$sub,image,postinst postrm preinst prerm); \
	done
endif

ifneq ($(skipdbg),true)
	# Debug image is simple
	install -m644 -D $(builddir)/build-$*/vmlinux \
		$(dbgpkgdir)/usr/lib/debug/boot/vmlinux-$(abi_release)-$*
	$(build_cd) $(kmake) $(build_O) modules_install $(vdso) \
		INSTALL_MOD_PATH=$(dbgpkgdir)/usr/lib/debug
	# Add .gnu_debuglink sections to each stripped .ko
	# pointing to unstripped verson
	find $(pkgdir) -name '*.ko' | sed 's|$(pkgdir)||'| while read module ; do \
		if [[ -f "$(dbgpkgdir)/usr/lib/debug/$$module" ]] ; then \
			$(CROSS_COMPILE)objcopy \
				--add-gnu-debuglink=$(dbgpkgdir)/usr/lib/debug/$$module \
				$(pkgdir)/$$module; \
			if grep -q CONFIG_MODULE_SIG=y $(builddir)/build-$*/.config; then \
				$(builddir)/build-$*/scripts/sign-file $(MODHASHALGO) \
					$(MODSECKEY) \
					$(MODPUBKEY) \
					$(pkgdir)/$$module; \
			fi; \
		fi; \
	done
	rm -f $(dbgpkgdir)/usr/lib/debug/lib/modules/$(abi_release)-$*/build
	rm -f $(dbgpkgdir)/usr/lib/debug/lib/modules/$(abi_release)-$*/source
	rm -f $(dbgpkgdir)/usr/lib/debug/lib/modules/$(abi_release)-$*/modules.*
	rm -fr $(dbgpkgdir)/usr/lib/debug/lib/firmware
endif

	# The flavour specific headers image
	# TODO: Would be nice if we didn't have to dupe the original builddir
	install -d -m755 $(hdrdir)
	cat $(builddir)/build-$*/.config | \
		sed -e 's/.*CONFIG_DEBUG_INFO=.*/# CONFIG_DEBUG_INFO is not set/g' > \
		$(hdrdir)/.config
	chmod 644 $(hdrdir)/.config
	$(kmake) O=$(hdrdir) -j1 syncconfig prepare scripts
	# We'll symlink this stuff
	rm -f $(hdrdir)/Makefile
	rm -rf $(hdrdir)/include2 $(hdrdir)/source
	# We do not need the retpoline information.
	find $(hdrdir) -name \*.o.ur-\* | xargs rm -f
	# Copy over the compilation version.
	cp "$(builddir)/build-$*/include/generated/compile.h" \
		"$(hdrdir)/include/generated/compile.h"
	# Add UTS_UBUNTU_RELEASE_ABI since UTS_RELEASE is difficult to parse.
	echo "#define UTS_UBUNTU_RELEASE_ABI $(abinum)" >> $(hdrdir)/include/generated/utsrelease.h
	# powerpc kernel arch seems to need some .o files for external module linking. Add them in.
ifeq ($(build_arch),powerpc)
	mkdir -p $(hdrdir)/arch/powerpc/lib
	cp $(builddir)/build-$*/arch/powerpc/lib/*.o $(hdrdir)/arch/powerpc/lib
endif
	# Copy over the new retpoline extractor.
	cp scripts/ubuntu-retpoline-extract-one $(hdrdir)/scripts
	# Script to symlink everything up
	$(SHELL) $(DROOT)/scripts/link-headers "$(hdrdir)" "$(indeppkg)" "$*"
	# The build symlink
	install -d debian/$(basepkg)-$*/lib/modules/$(abi_release)-$*
	$(LN) /usr/src/$(basepkg)-$* \
		debian/$(basepkg)-$*/lib/modules/$(abi_release)-$*/build
	# And finally the symvers
	install -m644 $(builddir)/build-$*/Module.symvers \
		$(hdrdir)/Module.symvers

	# Now the header scripts
	$(call install_control,$(hdrs_pkg_name)-$*,headers,postinst)

	# At the end of the package prep, call the tests
	DPKG_ARCH="$(arch)" KERN_ARCH="$(build_arch)" FLAVOUR="$*"	\
	 VERSION="$(abi_release)" REVISION="$(revision)"		\
	 PREV_REVISION="$(prev_revision)" ABI_NUM="$(abinum)"		\
	 PREV_ABI_NUM="$(prev_abinum)" BUILD_DIR="$(builddir)/build-$*"	\
	 INSTALL_DIR="$(pkgdir)" SOURCE_DIR="$(CURDIR)"			\
	 run-parts -v $(DROOT)/tests-build

	#
	# Remove files which are generated at installation by postinst,
	# except for modules.order and modules.builtin
	# 
	# NOTE: need to keep this list in sync with postrm
	#
	mkdir $(pkgdir)/lib/modules/$(abi_release)-$*/_
	mv $(pkgdir)/lib/modules/$(abi_release)-$*/modules.order \
		$(pkgdir)/lib/modules/$(abi_release)-$*/_
	if [ -f $(pkgdir)/lib/modules/$(abi_release)-$*/modules.builtin ] ; then \
	    mv $(pkgdir)/lib/modules/$(abi_release)-$*/modules.builtin \
		$(pkgdir)/lib/modules/$(abi_release)-$*/_; \
	fi
	rm -f $(pkgdir)/lib/modules/$(abi_release)-$*/modules.*
	mv $(pkgdir)/lib/modules/$(abi_release)-$*/_/* \
		$(pkgdir)/lib/modules/$(abi_release)-$*
	rmdir $(pkgdir)/lib/modules/$(abi_release)-$*/_

ifeq ($(do_linux_tools),true)
	# Create the linux-tools tool links
	install -d $(toolspkgdir)/usr/lib/linux-tools/$(abi_release)-$*
ifeq ($(do_tools_usbip),true)
	$(LN) ../../$(src_pkg_name)-tools-$(abi_release)/usbip $(toolspkgdir)/usr/lib/linux-tools/$(abi_release)-$*
	$(LN) ../../$(src_pkg_name)-tools-$(abi_release)/usbipd $(toolspkgdir)/usr/lib/linux-tools/$(abi_release)-$*
endif
ifeq ($(do_tools_acpidbg),true)
	$(LN) ../../$(src_pkg_name)-tools-$(abi_release)/acpidbg $(toolspkgdir)/usr/lib/linux-tools/$(abi_release)-$*
endif
ifeq ($(do_tools_cpupower),true)
	$(LN) ../../$(src_pkg_name)-tools-$(abi_release)/cpupower $(toolspkgdir)/usr/lib/linux-tools/$(abi_release)-$*
endif
ifeq ($(do_tools_perf),true)
	$(LN) ../../$(src_pkg_name)-tools-$(abi_release)/perf $(toolspkgdir)/usr/lib/linux-tools/$(abi_release)-$*
endif
ifeq ($(do_tools_x86),true)
	$(LN) ../../$(src_pkg_name)-tools-$(abi_release)/x86_energy_perf_policy $(toolspkgdir)/usr/lib/linux-tools/$(abi_release)-$*
	$(LN) ../../$(src_pkg_name)-tools-$(abi_release)/turbostat $(toolspkgdir)/usr/lib/linux-tools/$(abi_release)-$*
endif
endif
ifeq ($(do_cloud_tools),true)
ifeq ($(do_tools_hyperv),true)
	# Create the linux-hyperv tool links
	install -d $(cloudpkgdir)/usr/lib/linux-tools/$(abi_release)-$*
	$(LN) ../../$(src_pkg_name)-tools-$(abi_release)/hv_kvp_daemon $(cloudpkgdir)/usr/lib/linux-tools/$(abi_release)-$*
	$(LN) ../../$(src_pkg_name)-tools-$(abi_release)/hv_vss_daemon $(cloudpkgdir)/usr/lib/linux-tools/$(abi_release)-$*
	$(LN) ../../$(src_pkg_name)-tools-$(abi_release)/hv_fcopy_daemon $(cloudpkgdir)/usr/lib/linux-tools/$(abi_release)-$*
	$(LN) ../../$(src_pkg_name)-tools-$(abi_release)/lsvmbus $(cloudpkgdir)/usr/lib/linux-tools/$(abi_release)-$*
endif
endif

headers_tmp := $(CURDIR)/debian/tmp-headers
headers_dir := $(CURDIR)/debian/linux-libc-dev

hmake := $(MAKE) -C $(CURDIR) O=$(headers_tmp) \
	KERNELVERSION=$(abi_release) INSTALL_HDR_PATH=$(headers_tmp)/install \
	SHELL="$(SHELL)" ARCH=$(header_arch)

install-arch-headers:
	@echo Debug: $@
	dh_testdir
	dh_testroot
ifeq ($(do_libc_dev_package),true)
	dh_prep -plinux-libc-dev
endif

	rm -rf $(headers_tmp)
	install -d $(headers_tmp) $(headers_dir)/usr/include/

	$(hmake) $(defconfig)
	mv $(headers_tmp)/.config $(headers_tmp)/.config.old
	sed -e 's/^# \(CONFIG_MODVERSIONS\) is not set$$/\1=y/' \
	  -e 's/.*CONFIG_LOCALVERSION_AUTO.*/# CONFIG_LOCALVERSION_AUTO is not set/' \
	  $(headers_tmp)/.config.old > $(headers_tmp)/.config
	$(hmake) syncconfig
	$(hmake) headers_install

	( cd $(headers_tmp)/install/include/ && \
		find . -name '.' -o -name '.*' -prune -o -print | \
                cpio -pvd --preserve-modification-time \
			$(headers_dir)/usr/include/ )
	mkdir $(headers_dir)/usr/include/$(DEB_HOST_MULTIARCH)
	mv $(headers_dir)/usr/include/asm $(headers_dir)/usr/include/$(DEB_HOST_MULTIARCH)/

	rm -rf $(headers_tmp)

binary-arch-headers: install-arch-headers
	@echo Debug: $@
	dh_testdir
	dh_testroot
ifeq ($(do_libc_dev_package),true)
ifneq ($(DEBIAN),debian.master)
	echo "non-master branch building linux-libc-dev, aborting"
	exit 1
endif
	dh_installchangelogs -plinux-libc-dev
	dh_installdocs -plinux-libc-dev
	dh_compress -plinux-libc-dev
	dh_fixperms -plinux-libc-dev
	dh_installdeb -plinux-libc-dev
	$(lockme) dh_gencontrol -plinux-libc-dev -- $(libc_dev_version)
	dh_md5sums -plinux-libc-dev
	dh_builddeb -plinux-libc-dev
endif

binary-%: pkgimg = $(bin_pkg_name)-$*
binary-%: pkgimg_mods = $(mods_pkg_name)-$*
binary-%: pkgimg_ex = $(mods_extra_pkg_name)-$*
binary-%: pkgdir_ex = $(CURDIR)/debian/$(extra_pkg_name)-$*
binary-%: pkghdr = $(hdrs_pkg_name)-$*
binary-%: dbgpkg = $(bin_pkg_name)-$*-dbgsym
binary-%: dbgpkgdir = $(CURDIR)/debian/$(bin_pkg_name)-$*-dbgsym
binary-%: pkgtools = $(tools_flavour_pkg_name)-$*
binary-%: pkgcloud = $(cloud_flavour_pkg_name)-$*
binary-%: rprovides = $(if $(filter true,$(call custom_override,do_zfs,$*)),$(comma) spl-modules$(comma) spl-dkms$(comma) zfs-modules$(comma) zfs-dkms)
binary-%: target_flavour = $*
binary-%: install-%
	@echo Debug: $@
	dh_testdir
	dh_testroot

	dh_installchangelogs -p$(pkgimg)
	dh_installdocs -p$(pkgimg)
	dh_compress -p$(pkgimg)
	dh_fixperms -p$(pkgimg) -X/boot/
	dh_installdeb -p$(pkgimg)
	dh_shlibdeps -p$(pkgimg) $(shlibdeps_opts)
	$(lockme) dh_gencontrol -p$(pkgimg) -- -Vlinux:rprovides='$(rprovides)'
	dh_md5sums -p$(pkgimg)
	dh_builddeb -p$(pkgimg)

	dh_installchangelogs -p$(pkgimg_mods)
	dh_installdocs -p$(pkgimg_mods)
	dh_compress -p$(pkgimg_mods)
	dh_fixperms -p$(pkgimg_mods) -X/boot/
	dh_installdeb -p$(pkgimg_mods)
	dh_shlibdeps -p$(pkgimg_mods) $(shlibdeps_opts)
	$(lockme) dh_gencontrol -p$(pkgimg_mods)
	dh_md5sums -p$(pkgimg_mods)
	dh_builddeb -p$(pkgimg_mods)

ifeq ($(do_extras_package),true)
  ifeq ($(ship_extras_package),false)
	# If $(ship_extras_package) is explicitly set to false, then do not
	# construct the linux-image-extra package; instead just log all of the
	# "extra" modules which were pointlessly built yet won't be shipped.
	find $(pkgdir_ex) -name '*.ko' | sort \
		| sed 's|^$(pkgdir_ex)/|NOT-SHIPPED |' \
		| tee -a $(target_flavour).not-shipped.log;
  else
	if [ -f $(DEBIAN)/control.d/$(target_flavour).inclusion-list ] ; then \
		dh_installchangelogs -p$(pkgimg_ex); \
		dh_installdocs -p$(pkgimg_ex); \
		dh_compress -p$(pkgimg_ex); \
		dh_fixperms -p$(pkgimg_ex) -X/boot/; \
		dh_installdeb -p$(pkgimg_ex); \
		dh_shlibdeps -p$(pkgimg_ex) $(shlibdeps_opts); \
		$(lockme) dh_gencontrol -p$(pkgimg_ex); \
		dh_md5sums -p$(pkgimg_ex); \
		dh_builddeb -p$(pkgimg_ex); \
	fi
  endif
endif

	dh_installchangelogs -p$(pkghdr)
	dh_installdocs -p$(pkghdr)
	dh_compress -p$(pkghdr)
	dh_fixperms -p$(pkghdr)
	dh_shlibdeps -p$(pkghdr) $(shlibdeps_opts)
	dh_installdeb -p$(pkghdr)
	$(lockme) dh_gencontrol -p$(pkghdr)
	dh_md5sums -p$(pkghdr)
	dh_builddeb -p$(pkghdr)

ifneq ($(skipsub),true)
	@set -e; for sub in $($(*)_sub); do		\
		pkg=$(bin_pkg_name)-$$sub;	\
		dh_installchangelogs -p$$pkg;		\
		dh_installdocs -p$$pkg;			\
		dh_compress -p$$pkg;			\
		dh_fixperms -p$$pkg -X/boot/;		\
		dh_shlibdeps -p$$pkg $(shlibdeps_opts);	\
		dh_installdeb -p$$pkg;			\
		$(lockme) dh_gencontrol -p$$pkg;			\
		dh_md5sums -p$$pkg;			\
		dh_builddeb -p$$pkg;			\
	done
endif

ifneq ($(skipdbg),true)
	dh_installchangelogs -p$(dbgpkg)
	dh_installdocs -p$(dbgpkg)
	dh_compress -p$(dbgpkg)
	dh_fixperms -p$(dbgpkg)
	dh_installdeb -p$(dbgpkg)
	$(lockme) dh_gencontrol -p$(dbgpkg)
	dh_md5sums -p$(dbgpkg)
	dh_builddeb -p$(dbgpkg)

	# Hokay...here's where we do a little twiddling...
	# Renaming the debug package prevents it from getting into
	# the primary archive, and therefore prevents this very large
	# package from being mirrored. It is instead, through some
	# archive admin hackery, copied to http://ddebs.ubuntu.com.
	#
	mv ../$(dbgpkg)_$(release)-$(revision)_$(arch).deb \
		../$(dbgpkg)_$(release)-$(revision)_$(arch).ddeb
	set -e; \
	( \
		$(lockme_cmd) 9 || exit 1; \
		if grep -qs '^Build-Debug-Symbols: yes$$' /CurrentlyBuilding; then \
			sed -i '/^$(dbgpkg)_/s/\.deb /.ddeb /' debian/files; \
		else \
			grep -v '^$(dbgpkg)_.*$$' debian/files > debian/files.new; \
			mv debian/files.new debian/files; \
		fi; \
	) 9>$(lockme_file)
	# Now, the package wont get into the archive, but it will get put
	# into the debug system.
endif

ifeq ($(do_linux_tools),true)
	dh_installchangelogs -p$(pkgtools)
	dh_installdocs -p$(pkgtools)
	dh_compress -p$(pkgtools)
	dh_fixperms -p$(pkgtools)
	dh_shlibdeps -p$(pkgtools) $(shlibdeps_opts)
	dh_installdeb -p$(pkgtools)
	$(lockme) dh_gencontrol -p$(pkgtools)
	dh_md5sums -p$(pkgtools)
	dh_builddeb -p$(pkgtools)
endif
ifeq ($(do_cloud_tools),true)
	dh_installchangelogs -p$(pkgcloud)
	dh_installdocs -p$(pkgcloud)
	dh_compress -p$(pkgcloud)
	dh_fixperms -p$(pkgcloud)
	dh_shlibdeps -p$(pkgcloud) $(shlibdeps_opts)
	dh_installdeb -p$(pkgcloud)
	$(lockme) dh_gencontrol -p$(pkgcloud)
	dh_md5sums -p$(pkgcloud)
	dh_builddeb -p$(pkgcloud)
endif

ifneq ($(full_build),false)
	# Clean out this flavours build directory.
	rm -rf $(builddir)/build-$*
	# Clean out the debugging package source directory.
	rm -rf $(dbgpkgdir)
endif

#
# per-architecture packages
#
builddirpa = $(builddir)/tools-perarch

$(stampdir)/stamp-prepare-perarch:
	@echo Debug: $@
ifeq ($(do_any_tools),true)
	rm -rf $(builddirpa)
	install -d $(builddirpa)
	rsync -a --exclude debian --exclude debian.master --exclude $(DEBIAN) --exclude .git -a ./ $(builddirpa)/
endif
	touch $@

$(stampdir)/stamp-build-perarch: $(stampdir)/stamp-prepare-perarch install-arch-headers
	@echo Debug: $@
ifeq ($(do_linux_tools),true)
ifeq ($(do_tools_usbip),true)
	chmod 755 $(builddirpa)/tools/usb/usbip/autogen.sh
	cd $(builddirpa)/tools/usb/usbip && ./autogen.sh
	chmod 755 $(builddirpa)/tools/usb/usbip/configure
	cd $(builddirpa)/tools/usb/usbip && ./configure --prefix=$(builddirpa)/tools/usb/usbip/bin
	cd $(builddirpa)/tools/usb/usbip && make install CFLAGS="-g -O2 -static" CROSS_COMPILE=$(CROSS_COMPILE)
endif
ifeq ($(do_tools_acpidbg),true)
	cd $(builddirpa)/tools/power/acpi && make clean && make CFLAGS="-g -O2 -static -I$(builddirpa)/include" CROSS_COMPILE=$(CROSS_COMPILE) acpidbg
endif
ifeq ($(do_tools_cpupower),true)
	# Allow for multiple installed versions of cpupower and libcpupower.so:
	# Override LIB_MIN in order to to generate a versioned .so named
	# libcpupower.so.$(abi_release) and link cpupower with that.
	make -C $(builddirpa)/tools/power/cpupower \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		CROSS=$(CROSS_COMPILE) \
		LIB_MIN=$(abi_release) CPUFREQ_BENCH=false
endif
ifeq ($(do_tools_perf),true)
	cd $(builddirpa) && $(kmake) $(defconfig)
	mv $(builddirpa)/.config $(builddirpa)/.config.old
	sed -e 's/^# \(CONFIG_MODVERSIONS\) is not set$$/\1=y/' \
	  -e 's/.*CONFIG_LOCALVERSION_AUTO.*/# CONFIG_LOCALVERSION_AUTO is not set/' \
	  $(builddirpa)/.config.old > $(builddirpa)/.config
	cd $(builddirpa) && $(kmake) syncconfig
	cd $(builddirpa) && $(kmake) prepare
	cd $(builddirpa)/tools/perf && \
		$(kmake) prefix=/usr HAVE_NO_LIBBFD=1 HAVE_CPUS_DEMANGLE_SUPPORT=1 CROSS_COMPILE=$(CROSS_COMPILE) NO_LIBPYTHON=1 NO_LIBPERL=1 PYTHON=python2.7
endif
ifeq ($(do_tools_x86),true)
	cd $(builddirpa)/tools/power/x86/x86_energy_perf_policy && make CROSS_COMPILE=$(CROSS_COMPILE)
	cd $(builddirpa)/tools/power/x86/turbostat && make CROSS_COMPILE=$(CROSS_COMPILE)
endif
endif
ifeq ($(do_cloud_tools),true)
ifeq ($(do_tools_hyperv),true)
	cd $(builddirpa)/tools/hv && make CFLAGS="-I$(headers_dir)/usr/include -I$(headers_dir)/usr/include/$(DEB_HOST_MULTIARCH)" CROSS_COMPILE=$(CROSS_COMPILE) hv_kvp_daemon hv_vss_daemon hv_fcopy_daemon
endif
endif
	@touch $@

install-perarch: toolspkgdir = $(CURDIR)/debian/$(tools_pkg_name)
install-perarch: cloudpkgdir = $(CURDIR)/debian/$(cloud_pkg_name)
install-perarch: $(stampdir)/stamp-build-perarch
	@echo Debug: $@
	# Add the tools.
ifeq ($(do_linux_tools),true)
	install -d $(toolspkgdir)/usr/lib
	install -d $(toolspkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
ifeq ($(do_tools_usbip),true)
	install -m755 $(builddirpa)/tools/usb/usbip/bin/sbin/usbip \
		$(toolspkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
	install -m755 $(builddirpa)/tools/usb/usbip/bin/sbin/usbipd \
		$(toolspkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
endif
ifeq ($(do_tools_acpidbg),true)
	install -m755 $(builddirpa)/tools/power/acpi/acpidbg \
		$(toolspkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
endif
ifeq ($(do_tools_cpupower),true)
	install -m755 $(builddirpa)/tools/power/cpupower/cpupower \
		$(toolspkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
	# Install only the full versioned libcpupower.so.$(abi_release), not
	# the usual symlinks to it.
	install -m644 $(builddirpa)/tools/power/cpupower/libcpupower.so.$(abi_release) \
		$(toolspkgdir)/usr/lib/
endif
ifeq ($(do_tools_perf),true)
	install -m755 $(builddirpa)/tools/perf/perf $(toolspkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
endif
ifeq ($(do_tools_x86),true)
	install -m755 $(builddirpa)/tools/power/x86/x86_energy_perf_policy/x86_energy_perf_policy \
		$(toolspkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
	install -m755 $(builddirpa)/tools/power/x86/turbostat/turbostat \
		$(toolspkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
endif
endif
ifeq ($(do_cloud_tools),true)
ifeq ($(do_tools_hyperv),true)
	install -d $(cloudpkgdir)/usr/lib
	install -d $(cloudpkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
	install -m755 $(builddirpa)/tools/hv/hv_kvp_daemon \
		$(cloudpkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
	install -m755 $(builddirpa)/tools/hv/hv_vss_daemon \
		$(cloudpkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
	install -m755 $(builddirpa)/tools/hv/hv_fcopy_daemon \
		$(cloudpkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
	install -m755 $(builddirpa)/tools/hv/lsvmbus \
		$(cloudpkgdir)/usr/lib/$(src_pkg_name)-tools-$(abi_release)
endif
endif

binary-perarch: toolspkg = $(tools_pkg_name)
binary-perarch: cloudpkg = $(cloud_pkg_name)
binary-perarch: install-perarch
	@echo Debug: $@
ifeq ($(do_linux_tools),true)
	dh_strip -p$(toolspkg)
	dh_installchangelogs -p$(toolspkg)
	dh_installdocs -p$(toolspkg)
	dh_compress -p$(toolspkg)
	dh_fixperms -p$(toolspkg)
	dh_shlibdeps -p$(toolspkg) $(shlibdeps_opts)
	dh_installdeb -p$(toolspkg)
	$(lockme) dh_gencontrol -p$(toolspkg)
	dh_md5sums -p$(toolspkg)
	dh_builddeb -p$(toolspkg)
endif
ifeq ($(do_cloud_tools),true)
	dh_strip -p$(cloudpkg)
	dh_installchangelogs -p$(cloudpkg)
	dh_installdocs -p$(cloudpkg)
	dh_compress -p$(cloudpkg)
	dh_fixperms -p$(cloudpkg)
	dh_shlibdeps -p$(cloudpkg) $(shlibdeps_opts)
	dh_installdeb -p$(cloudpkg)
	$(lockme) dh_gencontrol -p$(cloudpkg)
	dh_md5sums -p$(cloudpkg)
	dh_builddeb -p$(cloudpkg)
endif

binary-debs: signing = $(CURDIR)/debian/$(bin_pkg_name)-signing
binary-debs: signingv = $(CURDIR)/debian/$(bin_pkg_name)-signing/$(release)-$(revision)
binary-debs: signing_tar = $(src_pkg_name)_$(release)-$(revision)_$(arch).tar.gz
binary-debs: binary-perarch $(addprefix binary-,$(flavours))
	@echo Debug: $@
ifeq ($(any_signed),true)
	install -d $(signingv)/control
	{ echo "tarball"; } >$(signingv)/control/options
	cd $(signing) && tar czvf ../../../$(signing_tar) .
	dpkg-distaddfile $(signing_tar) raw-signing -
endif

build-arch-deps-$(do_flavour_image_package) += $(addprefix $(stampdir)/stamp-build-,$(flavours))
build-arch: $(build-arch-deps-true)
	@echo Debug: $@

ifeq ($(AUTOBUILD),)
binary-arch-deps-$(do_flavour_image_package) += binary-udebs
else
binary-arch-deps-$(do_flavour_image_package) = binary-debs
endif
binary-arch-deps-$(do_libc_dev_package) += binary-arch-headers
ifneq ($(do_common_headers_indep),true)
binary-arch-deps-$(do_flavour_header_package) += binary-headers
endif
binary-arch: $(binary-arch-deps-true)
	@echo Debug: $@

