/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) Boyan Karatotev */
#ifndef __KERNEL_H_
#define __KERNEL_H_

#include <stdio.h>

// since I steal the btree implementation from the kernel, it requires some
// kernel things. Here I stub them out to keep things working while keeping the
// code (mostly) the same

typedef long gfp_t;
typedef long mempool_t;
typedef uint64_t u64;
typedef uint32_t u32;

// can't have sizeof in #define for some reason??
#define BITS_PER_LONG 64

#define likely(a) a
#define BUG_ON(a)							\
	do {								\
		if (a) {						\
			fprintf(					\
				stderr, "BUG, aborting at %s:%ld\n",	\
				__FILE__, __LINE__			\
			);						\
			abort();					\
		}							\
	} while (0)

// gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-warn_005funused_005fresult-function-attribute
// clang: https://clang.llvm.org/docs/AttributeReference.html#nodiscard-warn-unused-result
#define __must_check __attribute__((__warn_unused_result__))
#define __init
#define __exit

#endif
