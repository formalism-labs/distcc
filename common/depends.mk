
define MODULE_DEPENDS.common
	(distcc-contrib-lzo,$(VROOT)/distcc/contrib/lzo)
endef

define MODULE_DEPENDS.windows
	(distcc-contrib-subproc,$(VROOT)/distcc/contrib/subproc)
	(distcc-contrib-syslog,$(VROOT)/distcc/contrib/syslog)
endef
