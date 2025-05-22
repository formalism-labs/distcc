
define MODULE_DEPENDS.common
	(distcc-common,$(VROOT)/distcc/common)
	(distcc-contrib-popt,$(VROOT)/distcc/contrib/popt)
	(distcc-contrib-rvfc,$(VROOT)/distcc/contrib/rvfc)
	(distcc-contrib-lzo,$(VROOT)/distcc/contrib/lzo)
	(distcc-contrib-boost,$(VROOT)/distcc/contrib/boost/lib/)
endef
