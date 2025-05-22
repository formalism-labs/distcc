
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
	../..
	../../config/$(TARGET_OS_TYPE)
endef

CC_INCLUDE_DIRS += $(CC_INCLUDE_DIRS.common) $(CC_INCLUDE_DIRS.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

define CC_SRC_FILES.common
	misc.cpp
	sub_proc.cpp
	w32err.cpp
endef

CC_SRC_FILES += $(CC_SRC_FILES.common) $(CC_SRC_FILES.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

include $(MK)/rules
