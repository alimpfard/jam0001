/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Assertions.h>
#include <AK/Find.h>
#include <AK/Forward.h>
#include <AK/Iterator.h>
#include <AK/Optional.h>
#include <AK/Span.h>
#include <AK/StdLibExtras.h>
#include <AK/Traits.h>
#include <AK/TypedTransfer.h>
#include <AK/kmalloc.h>

template<typename T, size_t cap, size_t line>
consteval size_t evil_sizeof()
{
    if constexpr (cap == 0 && line > 700)
        return 0;
    else
        return sizeof(T);
}

template<typename T, size_t cap, size_t line>
consteval size_t evil_alignof()
{
    if constexpr (cap == 0 && line > 700)
        return 1;
    else
        return alignof(T);
}

#define sizeof(x) evil_sizeof<x, inline_capacity, __LINE__>()
#define alignof(x) evil_alignof<x, inline_capacity, __LINE__>()
#define alignas(x) alignas(evil_alignof<x, inline_capacity, __LINE__>())
#include <AK/Vector.h>
#undef sizeof
#undef alignof
#undef alignas
