
SRC_ROOT=../../..
include $(SRC_ROOT)/freemason/framework/main

include $(MK)/defs

#----------------------------------------------------------------------------------------------

define CC_PP_DEFS.common
	HAVE_CONFIG_H
endef

CC_PP_DEFS += $(CC_PP_DEFS.common) $(CC_PP_DEFS.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

define CC_INCLUDE_DIRS.common
	../../config/$(TARGET_OS_TYPE)
endef

CC_INCLUDE_DIRS += $(CC_INCLUDE_DIRS.common) $(CC_INCLUDE_DIRS.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

define CC_SRC_FILES.common
	findme.c
	popt.c
	poptconfig.c
	popthelp.c
	poptparse.c
endef

CC_SRC_FILES += $(CC_SRC_FILES.common) $(CC_SRC_FILES.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

include $(MK)/rules
