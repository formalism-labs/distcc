
MODULE=distcc-client
MODULE_DIR=$(VROOT)/distcc/client

include $(MK)/module/start

MODULE_PRODUCT=prog

MODULE_TARGET_NAME=distcc

include $(MK)/module/end
