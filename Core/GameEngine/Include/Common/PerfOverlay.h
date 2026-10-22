/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//////////////////////////////////////////////////////////////////////////////
//
// PerfOverlay.h
//
// A tiny, always available (debug builds only) on screen profiler.
//
// The legacy profiling code in Common/PerfTimer.h is completely disabled
// (PERF_TIMERS is never defined), so this module provides a simple and
// self contained replacement:
//
//   PERF_SCOPE("AI")   - times the enclosing scope and adds it to the "AI" counter
//   PERF_COUNT("AI")   - only counts the calls of the enclosing scope
//
// The counters are drawn on top of the screen every frame and can be toggled
// with the F11 key (debug builds only).
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/GameCommon.h"

class DebugDisplayInterface;

// The overlay is only available in debug builds.
// RTS_PERF_OVERLAY is not used, this is a debug only feature.
#if defined(RTS_DEBUG)

// The maximum number of counters that can be registered at the same time.
enum { PERF_OVERLAY_MAX_COUNTERS = 32 };

//-------------------------------------------------------------------------------------------------
// Registers a counter with the given name and returns its index.
// Registering the same name more than once returns the index of the existing counter.
// The name must be a static string (it is not copied).
//-------------------------------------------------------------------------------------------------
Int PerfOverlayRegister(const char *name);

//-------------------------------------------------------------------------------------------------
// Adds the given amount of time (in timer ticks) to the counter with the given index.
//-------------------------------------------------------------------------------------------------
void PerfOverlayAddTime(Int index, Int64 ticks);

//-------------------------------------------------------------------------------------------------
// Adds a single call to the counter with the given index.
//-------------------------------------------------------------------------------------------------
void PerfOverlayAddCall(Int index);

//-------------------------------------------------------------------------------------------------
// Publishes the counters of the frame that just finished and starts a new one.
// The published values are the ones that get displayed.
//-------------------------------------------------------------------------------------------------
void PerfOverlayEndFrame();

//-------------------------------------------------------------------------------------------------
// Draws the counters of the last finished frame on top of the screen.
// Does nothing when the overlay is hidden.
//-------------------------------------------------------------------------------------------------
void PerfOverlayDraw();

//-------------------------------------------------------------------------------------------------
// Visibility of the overlay.
//-------------------------------------------------------------------------------------------------
Bool PerfOverlayIsVisible();
void PerfOverlaySetVisible(Bool visible);
void PerfOverlayToggle();

//-------------------------------------------------------------------------------------------------
// Times the enclosing scope and adds the result to the counter with the given index.
//-------------------------------------------------------------------------------------------------
class PerfOverlayScope
{
	Int m_index;
	Int64 m_startTime;
public:
	explicit PerfOverlayScope(Int index);
	~PerfOverlayScope();
};

//-------------------------------------------------------------------------------------------------
// Helper for building unique variable names out of __LINE__.
//-------------------------------------------------------------------------------------------------
#define PERF_OVERLAY_CONCAT_IMPL(a, b) a##b
#define PERF_OVERLAY_CONCAT(a, b) PERF_OVERLAY_CONCAT_IMPL(a, b)

//-------------------------------------------------------------------------------------------------
// Times the enclosing scope and adds it to the named counter.
//-------------------------------------------------------------------------------------------------
#define PERF_SCOPE(name) \
	static const Int PERF_OVERLAY_CONCAT(perfCounterIndex_, __LINE__) = PerfOverlayRegister(name); \
	PerfOverlayScope PERF_OVERLAY_CONCAT(perfCounterScope_, __LINE__)(PERF_OVERLAY_CONCAT(perfCounterIndex_, __LINE__))

//-------------------------------------------------------------------------------------------------
// Counts the calls of the enclosing scope in the named counter.
//-------------------------------------------------------------------------------------------------
#define PERF_COUNT(name) \
	static const Int PERF_OVERLAY_CONCAT(perfCounterIndex_, __LINE__) = PerfOverlayRegister(name); \
	PerfOverlayAddCall(PERF_OVERLAY_CONCAT(perfCounterIndex_, __LINE__))

#else // RTS_DEBUG

// Builds without the overlay: all calls are no-ops.
inline void PerfOverlayEndFrame() {}
inline void PerfOverlayDraw() {}
inline Bool PerfOverlayIsVisible() { return false; }
inline void PerfOverlaySetVisible(Bool) {}
inline void PerfOverlayToggle() {}

#define PERF_SCOPE(name) ((void)0)
#define PERF_COUNT(name) ((void)0)

#endif // RTS_DEBUG
