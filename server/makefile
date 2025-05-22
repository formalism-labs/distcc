
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
	../common
	../contrib
endef

CC_INCLUDE_DIRS += $(CC_INCLUDE_DIRS.common) $(CC_INCLUDE_DIRS.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

define CC_SRC_FILES.common
	access.cpp
	daemon.cpp
	dopt.cpp
	dparent.cpp
	dsignal.cpp
	log.cpp
	prefork.cpp
	serve.cpp
	setuid.cpp
	srvnet.cpp
	srvrpc.cpp
endef

define CC_SRC_FILES.windows
	mini-dump.cpp
	service.cpp
endef

CC_SRC_FILES += $(CC_SRC_FILES.common) $(CC_SRC_FILES.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

include $(MK)/rules
