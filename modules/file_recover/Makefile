include $(SRCTREE)/build/base.mk
TARGET_DIR := $(TARGET_OUT_DIR)/lib
TARGET_A := $(TARGET_DIR)/libapp_file_recover.a

SEXT	:= cpp
SDIR	+= $(CURDIR)/src
#DEPEND
INCS		+= -I$(CURDIR)/include
INCS		+= -I$(CURDIR)/include/muxer
INCS		+= -I$(SRCTREE)/modules/common/include
INCS		+= -I$(SRCTREE)/modules/video/include
INCS		+= -I$(SRCTREE)/modules/paramparse/include
INCS		+= -I$(SRCTREE)/modules/osd/include
INCS		+= -I$(SRCTREE)/modules/audio/include
INCS		+= -I$(SRCTREE)/modules/ai/include
INCS		+= -I$(SRCTREE)/modules/file_recover/include
INCS		+= -I$(SRCTREE)/modules/ringbuffer/include

CXXFLAGS += -std=c++14

include $(SRCTREE)/build/build.mk