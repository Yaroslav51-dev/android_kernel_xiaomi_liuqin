# SPDX-License-Identifier: GPL-2.0
subdir-y += qcom
subdir-y += xiaomi

dtstree	:= $(srctree)/$(src)
vendor  := $(dtstree)/vendor
ifneq "$(wildcard $(vendor)/Makefile)" ""
    subdir-y += vendor
endif
