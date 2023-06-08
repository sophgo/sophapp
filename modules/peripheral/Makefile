include $(SRCTREE)/build/base.mk

TOPTARGETS := all clean install


module-$(CONFIG_MODULE_GPIO)				+= gpio
module-$(CONFIG_MODULE_ADC)					+= adc
module-$(CONFIG_MODULE_IRCUT)				+= ircut
module-$(CONFIG_MODULE_PWM)					+= pwm


SUBDIRS += $(module-y)

$(TOPTARGETS): $(SUBDIRS) 

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)
