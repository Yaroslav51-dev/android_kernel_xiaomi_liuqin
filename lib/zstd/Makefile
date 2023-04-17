# SPDX-License-Identifier: GPL-2.0-only
obj-$(CONFIG_ZSTD_COMPRESS) += zstd_compress.o
obj-$(CONFIG_ZSTD_DECOMPRESS) += zstd_decompress.o

KBUILD_CFLAGS	:= $(filter-out -O3, $(KBUILD_CFLAGS))
KBUILD_CFLAGS	+= -O2
KBUILD_CFLAGS	:= $(filter-out -flto, $(KBUILD_CFLAGS))
KBUILD_CFLAGS	+= $(DISABLE_LTO_CLANG)


zstd_compress-y := \
		zstd_compress_module.o \
		compress/fse_compress.o \
		compress/hist.o \
		compress/huf_compress.o \
		compress/zstd_compress.o \
		compress/zstd_compress_literals.o \
		compress/zstd_compress_sequences.o \
		compress/zstd_compress_superblock.o \
		compress/zstd_double_fast.o \
		compress/zstd_fast.o \
		compress/zstd_lazy.o \
		compress/zstd_ldm.o \
		compress/zstd_opt.o \

zstd_compress-y := fse_compress.o huf_compress.o compress.o \
		   entropy_common.o fse_decompress.o zstd_common.o
zstd_decompress-y := huf_decompress.o decompress.o \
		     entropy_common.o fse_decompress.o zstd_common.o
