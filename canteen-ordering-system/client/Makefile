#
# Makefile
#
CC = arm-linux-gcc
LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= ${shell pwd}
CFLAGS ?= -O3 -g0 -I$(LVGL_DIR)/ -I$(LVGL_DIR)/ui -Wall -Wshadow -Wundef -Wmissing-prototypes -Wno-discarded-qualifiers -Wall -Wextra -Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith -fno-strict-aliasing -Wno-error=cpp -Wuninitialized -Wmaybe-uninitialized -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wformat-security -Wno-ignored-qualifiers -Wno-error=pedantic -Wno-sign-compare -Wno-error=missing-prototypes -Wdouble-promotion -Wclobbered -Wdeprecated -Wempty-body -Wtype-limits  -Wstack-usage=2048 -Wno-unused-value -Wno-unused-parameter -Wno-missing-field-initializers -Wuninitialized -Wmaybe-uninitialized -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wpointer-arith -Wno-cast-qual -Wmissing-prototypes -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wno-discarded-qualifiers -Wformat-security -Wno-ignored-qualifiers -Wno-sign-compare
LDFLAGS ?= -lm
BUILD_DIR = build
BUILD_OBJ_DIR = $(BUILD_DIR)/obj
BUILD_BIN_DIR = $(BUILD_DIR)/bin
BIN = demo


#Collect the files to compile
MAINSRC = ./main.c ./lv_lib_100ask-release-v8.x/src/lv_100ask_pinyin_ime/lv_100ask_pinyin_ime.c ./lv_lib_100ask-release-v8.x/test/lv_100ask_pinyin_ime_test/simple_test.c ./lv_lib_100ask-release-v8.x/test/lv_100ask_pinyin_ime_test/lv_font_source_han_sans_normal_16.c 

include $(LVGL_DIR)/lvgl/lvgl.mk
include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

CSRCS += $(LVGL_DIR)/mouse_cursor_icon.c
CSRCS += $(LVGL_DIR)/photo_album.c
CSRCS += $(LVGL_DIR)/login_screen.c
CSRCS += $(shell find $(LVGL_DIR)/ui  -name "*.c")

SRCS = $(ASRCS) $(CSRCS) $(MAINSRC)

# build/obj/<relative-path>.o  (strip LVGL_DIR/ or ./)
src_to_obj = $(BUILD_OBJ_DIR)/$(patsubst $(LVGL_DIR)/%,%,$(patsubst ./%,%,$(basename $(1)))).o
OBJS = $(foreach s,$(SRCS),$(call src_to_obj,$(s)))

.PHONY: all default clean

all: default

default: $(BUILD_BIN_DIR)/$(BIN)

$(BUILD_BIN_DIR)/$(BIN): $(OBJS)
	@mkdir -p $(BUILD_BIN_DIR)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
	@echo "BIN $@"

define compile_rule
$(call src_to_obj,$(1)): $(1)
	@mkdir -p $$(dir $$@)
	@$$(CC) $$(CFLAGS) -c $$< -o $$@
	@echo "CC $$<"
endef

$(foreach s,$(SRCS),$(eval $(call compile_rule,$(s))))

clean:
	rm -rf $(BUILD_DIR)
