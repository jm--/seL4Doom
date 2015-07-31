#
# Copyright (c) 2015, Josef Mihalits
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "COPYING" for details.
#

TARGETS := $(notdir $(SOURCE_DIR)).bin

ENTRY_POINT := _sel4_start

# required source files
CFILES   := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))

# CPIO archive
OFILES := archive.o

# required libraries
LIBS = c cpio sel4 sel4muslcsys sel4vka sel4allocman \
       platsupport sel4platsupport sel4vspace \
       sel4simple sel4simple-stable sel4utils utils

# extra flags
# (IPPORT_USERRESERVED is not used and only defined to make program compile)
CFLAGS += -ggdb -g3 -I$(SOURCE_DIR)/src/SDL -DIPPORT_USERRESERVED=5000

include $(SEL4_COMMON)/common.mk

# whitespace separated list of relative filenames to include
CPIO_FILES :=
CPIO_FILES_FULL := $(addprefix $(SOURCE_DIR)/, $(CPIO_FILES))

# list absolute files to include
CPIO_FILES_FULL +=

# remove enclosing quotes
CONFIG_APP_DOOM_CPIO_FILES_FULL:=$(patsubst %",%,\
	$(patsubst "%,%,${CONFIG_APP_DOOM_CPIO_FILES_FULL}))

# add user specified files
CPIO_FILES_FULL += $(CONFIG_APP_DOOM_CPIO_FILES_FULL)

archive.o: $(CPIO_FILES_FULL)
	$(Q)mkdir -p $(dir $@)
	${COMMON_PATH}/files_to_obj.sh $@ _cpio_archive $^
