
SRC_ROOT=../../..
include $(SRC_ROOT)/freemason/framework/main

include $(MK)/defs

#----------------------------------------------------------------------------------------------

define CC_PP_DEFS.common
endef

CC_PP_DEFS += $(CC_PP_DEFS.common) $(CC_PP_DEFS.$(TARGET_OS_TYPE)) $(CC_PP_DEFS.$(TARGET_OS))

#----------------------------------------------------------------------------------------------

define CC_INCLUDE_DIRS.common
	.
	../..
endef

define CC_INCLUDE_DIRS.windows
endef

CC_INCLUDE_DIRS += $(CC_INCLUDE_DIRS.common) $(CC_INCLUDE_DIRS.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

define CC_SRC_FILES.common
	exceptions/general.cpp

	filesys/directory.cpp
	filesys/file.cpp
	filesys/filename.cpp

	time/datetime.cpp
	time/ticks.cpp
	
	text/text.cpp
endef

define CC_SRC_FILES.windows
	system/win32/handle.cpp
	system/win32/process.cpp
	system/win32/uuid.cpp
endef

CC_SRC_FILES += $(CC_SRC_FILES.common) $(CC_SRC_FILES.$(TARGET_OS_TYPE))

#----------------------------------------------------------------------------------------------

define TESTS.common
endef

TESTS += $(TESTS.common)

#----------------------------------------------------------------------------------------------

include $(MK)/rules
