
ifeq ($(SRCS), )
SRCS ?= $(shell find $(SDIR) -type f -name "*.$(SEXT)")
endif
OBJTREE := $(TARGET_OUT_DIR)/obj
ODIR := $(patsubst $(SRCTREE)/%,$(OBJTREE)/%,$(SDIR))
OBJDIR := $(patsubst $(SRCTREE)/%,$(OBJTREE)/%,$(SDIR))
OBJS := $(addprefix $(ODIR)/,$(notdir $(SRCS:.$(SEXT)=.o)))
DEPS := $(addprefix $(ODIR)/,$(notdir $(SRCS:.$(SEXT)=.d)))

CFLAGS += $(INCS)
CXXFLAGS += $(INCS)
Q = @

$(OBJDIR)/%.o : $(SDIR)/%.c
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -o $@ -c $<

$(OBJDIR)/%.o : $(SDIR)/%.cpp
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CXX) $(CXXFLAGS) -o $@ -c $<

$(TARGET_A) : $(OBJS)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(AR) $(ARFLAGS) $@ $^

$(TARGET_SO) : $(OBJS)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(LDFLAGS_SO) -o $@ $^

$(TARGET) : $(OBJS)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CXX) $(TARGETFLAGS) $^ $(TARGET_LIB) $(LIBS) -o $@
	$(Q)$(STRIP) $@

clean:
	$(Q)rm -rf $(TARGET_OUT_DIR)
	$(Q)rm -rf $(SRCTREE)/install

-include $(DEPS)
