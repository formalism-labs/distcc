
SRC_ROOT=../..
include $(SRC_ROOT)/freemason/framework/main

include $(MK)/defs

#----------------------------------------------------------------------------------------------

define CC_PP_DEFS.common
	HAVE_CONFIG_H
	_GNU_SOURCE
	SYSCONFDIR=\"/etc\"
	PKGDATADIR=\"/usr/share/distcc\"
endef

CC_PP_DEFS += $(CC_PP_DEFS.common) $(CC_PP_DEFS.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

define CC_INCLUDE_DIRS.common
	..
	../common
	../contrib
endef

CC_INCLUDE_DIRS += $(CC_INCLUDE_DIRS.common) $(CC_INCLUDE_DIRS.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

define CC_SRC_FILES.common
	backoff.cpp
	climasq.cpp
	clinet.cpp
	clirpc.cpp
	compile.cpp
	cpp.cpp
	distcc.cpp
	dopt.cpp
	hostfile.cpp
	hosts.cpp
	implicit.cpp
	loadfile.cpp
	remote.cpp
	ssh.cpp
	timefile.cpp
	traceenv.cpp
	where.cpp
endef

CC_SRC_FILES += $(CC_SRC_FILES.common) $(CC_SRC_FILES.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

include $(MK)/rules
