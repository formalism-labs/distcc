
SRC_ROOT=../..
include $(SRC_ROOT)/freemason/framework/main

include $(MK)/defs

#----------------------------------------------------------------------------------------------

define CC_PP_DEFS.common
	HAVE_CONFIG_H
	_GNU_SOURCE
endef

CC_PP_DEFS += $(CC_PP_DEFS.common) $(CC_PP_DEFS.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

define CC_INCLUDE_DIRS.common
	..
	../contrib
	../contrib/lzo
endef

CC_INCLUDE_DIRS += $(CC_INCLUDE_DIRS.common) $(CC_INCLUDE_DIRS.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

define CC_SRC_FILES.common
	arg.cpp
	argutil.cpp
	bulk.cpp
	cc-diab.cpp
	cc-gcc.cpp
	cc-msc.cpp
	cleanup.cpp
	compiler.cpp
	compress.cpp
	exec.cpp
	filename.cpp
	help.cpp
	io.cpp
	lock.cpp
	ncpus.cpp
	netutil.cpp
	pump.cpp
	rpc1.cpp
	safeguard.cpp
	sendfile.cpp
	snprintf.cpp
	state.cpp
	strip.cpp
	tempfile.cpp
	timeval.cpp
	trace.cpp
	util.cpp
endef

define CC_SRC_FILES.windows
	win32.cpp
endef

CC_SRC_FILES += $(CC_SRC_FILES.common) $(CC_SRC_FILES.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

include $(MK)/rules
