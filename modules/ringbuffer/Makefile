include $(SRCTREE)/build/base.mk
TARGET_DIR := $(TARGET_OUT_DIR)/lib
TARGET_A := $(TARGET_DIR)/libapp_ringbuffer.a

SEXT	:= c
SDIR	+= $(CURDIR)/src
#DEPEND
INCS								+= -I$(CURDIR)/include
INCS-$(CONFIG_MODULE_COMMON)		+= -I$(SRCTREE)/modules/common/include
INCS += $(INCS-y)
include $(SRCTREE)/build/build.mk