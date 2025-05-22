
MODULE=distcc-server
MODULE_DIR=$(VROOT)/distcc/server

include $(MK)/module/start

MODULE_PRODUCT=prog

MODULE_TARGET_NAME=distccd

include $(MK)/module/end
