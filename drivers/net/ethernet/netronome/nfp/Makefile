obj-$(CONFIG_NFP)	+= nfp.o

nfp-objs := \
	    nfpcore/nfp6000_pcie.o \
	    nfpcore/nfp_cppcore.o \
	    nfpcore/nfp_cpplib.o \
	    nfpcore/nfp_target.o \
	    nfp_main.o \
	    nfp_net_common.o \
	    nfp_net_ethtool.o \
	    nfp_netvf_main.o

nfp-$(CONFIG_NFP_DEBUG) += nfp_net_debugfs.o
