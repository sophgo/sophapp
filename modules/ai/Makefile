include $(SRCTREE)/build/base.mk

TOPTARGETS := all clean install

module-$(CONFIG_MODULE_AI_MD)					+= md
module-$(CONFIG_MODULE_AI_PD)					+= pd
module-$(CONFIG_MODULE_AI_FD_FACE)				+= face
module-$(CONFIG_MODULE_AI_IRFAECE)				+= ir_face
module-$(CONFIG_MODULE_AI_BABYCRY)				+= baby_cry

SUBDIRS += $(module-y)

$(TOPTARGETS): $(SUBDIRS) 

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)
