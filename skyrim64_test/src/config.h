#pragma once

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN

#define SKYRIM64_CREATIONKIT_ONLY	1	// Only build code related to the Creation Kit hooks
#define SKYRIM64_GENERATE_OFFSETS	0	// Dump offset list to disk in codegen.cpp
#define SKYRIM64_USE_VTUNE			0	// Enable VTune instrumentation API
#define SKYRIM64_USE_VFS			0	// Enable virtual file system
#define SKYRIM64_USE_PROFILER		0	// Enable built-in profiler macros / "profiler.h"
#define SKYRIM64_USE_TRACY			0	// Enable tracy client + server / https://bitbucket.org/wolfpld/tracy/overview
#define SKYRIM64_USE_PAGE_HEAP		0	// Treat every memory allocation as a separate page (4096 bytes) for debugging