#
# Copyright (c) 2015, Josef Mihalits
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "COPYING" for details.
#

apps-$(CONFIG_APP_DOOM) += doom

# dependencies
doom-y = common libsel4 libcpio libmuslc libsel4muslcsys libsel4vka \
         libsel4allocman libsel4simple libsel4simple-stable \
         libsel4platsupport libsel4vspace libsel4utils libutils

# dependencies
doom: kernel_elf $(doom-y)

