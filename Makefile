#
# Copyright (c) 2015, Josef Mihalits
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "COPYING" for details.
#

lib-dirs:=libs
libc=libmuslc

ifdef SEL4DOOM_DEBUG
DEFINES += SEL4DOOM_DEBUG
endif

-include .config
include tools/common/project.mk

all: cdrom

gdb:
	gdb -ex 'file $(IMAGE_ROOT)/$(apps)-image-ia32-pc99' \
		-ex 'target remote localhost:1234' \
		-ex 'break main' \
		-ex c

debug run: $(IMAGE_ROOT)/cdrom.iso
	qemu-system-i386 $(if $(subst run,,$@), -s -S) \
		-m 512 -serial stdio \
		-cdrom $(IMAGE_ROOT)/cdrom.iso \
		-boot order=d	\
		-vga std

# make a basic cdrom iso image (apps must only be one app)
cdrom: grub.cfg app-images
	@echo " [CD-ROM]"
	$(Q)mkdir -p $(IMAGE_ROOT)/cdrom/boot/grub
	$(Q)cp grub.cfg $(IMAGE_ROOT)/cdrom/boot/grub
	$(Q)cp $(IMAGE_ROOT)/kernel-ia32-pc99        $(IMAGE_ROOT)/cdrom/kernel
	$(Q)cp $(IMAGE_ROOT)/$(apps)-image-ia32-pc99 $(IMAGE_ROOT)/cdrom/app-image
	$(Q)grub-mkrescue -o $(IMAGE_ROOT)/cdrom.iso $(IMAGE_ROOT)/cdrom

clean-cdrom:
	@echo "[CLEAN] cdrom"
	$(Q)rm -rf $(IMAGE_ROOT)/cdrom.iso $(IMAGE_ROOT)/cdrom

