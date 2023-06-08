include $(SRCTREE)/build/base.mk
TARGET_DIR := $(TARGET_OUT_DIR)/lib
TARGET_A := $(TARGET_DIR)/libapp_recorder.a

SEXT	:= c
SDIR	+= $(CURDIR)/src
#DEPEND
INCS		+= -I$(CURDIR)/include
INCS		+= -I$(CURDIR)/include/muxer
INCS-$(CONFIG_MODULE_COMMON)			+= -I$(SRCTREE)/modules/common/include
INCS-$(CONFIG_MODULE_VIDEO)				+= -I$(SRCTREE)/modules/video/include
INCS-$(CONFIG_MODULE_PARAMPARSE)		+= -I$(SRCTREE)/modules/paramparse/include
INCS-$(CONFIG_MODULE_OSD)				+= -I$(SRCTREE)/modules/osd/include
INCS-$(CONFIG_MODULE_AUDIO)				+= -I$(SRCTREE)/modules/audio/include
INCS-$(CONFIG_MODULE_AI)				+= -I$(SRCTREE)/modules/ai/include
INCS-$(CONFIG_MODULE_RECORD)			+= -I$(SRCTREE)/modules/file_recover/include
INCS-$(CONFIG_MODULE_RINGBUF)			+= -I$(SRCTREE)/modules/ringbuffer/include
CFLAGS += -D_GNU_SOURCE
INCS += $(INCS-y)

include $(SRCTREE)/build/build.mk