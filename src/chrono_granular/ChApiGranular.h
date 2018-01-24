// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2016 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Dan Negrut
// =============================================================================

#pragma once

// When compiling this library, remember to define CH_API_COMPILE_GRANULAR
// (so that the symbols with 'CH_GRANULAR_API' in front of them will be
// marked as exported). Otherwise, just do not define it if you
// link the library to your code, and the symbols will be imported.

#if defined(CH_API_COMPILE_GRANULAR)
#define CH_GRANULAR_API ChApiEXPORT
#else
#define CH_GRANULAR_API ChApiIMPORT
#endif

// Macros for specifying type alignment
#if (defined __GNUC__) || (defined __INTEL_COMPILER)
#define CHRONO_ALIGN_16 __attribute__((aligned(16)))
#elif defined _MSC_VER
#define CHRONO_ALIGN_16 __declspec(align(16))
#else
#define CHRONO_ALIGN_16
#endif

#if defined _MSC_VER
#define fmax fmax
#define fmin fmin
#endif

#if defined(WIN32) || defined(WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define ELPP_WINSOCK2
#endif

/**
@defgroup parallel_module GRANULAR module
@brief Module that enables granular dynamics simulation in Chrono

This module implements dedicated algorithms that can be
used as a faster alternative to the default simulation algorithms
in Chrono::Engine or Chrono::Parallel. This is achieved using OpenMP, CUDA, CUB, etc.

For additional information, see:
- the [installation guide](@ref module_granular_installation)
- the [tutorials](@ref tutorial_root)

@{
@defgroup granular_physics Physics objects
@defgroup granular_cuda CUDA objects
@}
*/

