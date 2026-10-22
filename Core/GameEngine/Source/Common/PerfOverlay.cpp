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
// PerfOverlay.cpp
//
//////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/PerfOverlay.h"

#if defined(RTS_DEBUG)

#include "Common/PerfTimer.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/Keyboard.h"

namespace
{

//-------------------------------------------------------------------------------------------------
struct PerfCounter
{
	const char	*m_name;			///< name of the counter (a static string)
	Int64		m_ticks;			///< ticks accumulated during the current frame
	Int			m_calls;			///< calls accumulated during the current frame
	Int64		m_lastTicks;		///< smoothed ticks of the last published interval
	Int			m_lastCalls;		///< calls of the last published interval
	Int64		m_peakTicks;		///< worst case ticks seen so far
	Int			m_value;			///< value accumulated during the current frame
	Int			m_lastValue;		///< value of the last published interval
	Int			m_valueTotal;		///< total number the value relates to, 0 if there is none
	Int			m_lastValueTotal;	///< total number of the last published interval
	Int			m_accumValueTotal;	///< totals accumulated since the last publish
	UnsignedInt	m_detailGroup;		///< the group of detail counters, 0 if always shown
	Int64		m_accumTicks;		///< ticks accumulated since the last publish
	Int			m_accumCalls;		///< calls accumulated since the last publish
	Int			m_accumValue;		///< values accumulated since the last publish
};

//-------------------------------------------------------------------------------------------------
enum { MAX_COUNTERS = PERF_OVERLAY_MAX_COUNTERS };

// Position and look of the overlay.
enum
{
	PERF_OVERLAY_POS_X = 8,			///< pixels from the left screen edge
	PERF_OVERLAY_POS_Y = 64,			///< pixels from the top screen edge
	PERF_OVERLAY_PADDING = 5,		///< padding around the text, in pixels
	PERF_OVERLAY_PUBLISH_MS = 250,	///< how often the numbers are refreshed, in milliseconds
	PERF_OVERLAY_SMOOTHING = 4,		///< weight of the newest sample, bigger means less smoothing
	PERF_OVERLAY_SORT_BUCKET_MS = 1,	///< rows only change places when they differ by more than this
};

PerfCounter s_counters[MAX_COUNTERS];
Int s_numCounters = 0;
Bool s_visible = TRUE;
UnsignedInt s_detailVisibleMask = 0;
Int64 s_lastEndFrameTicks = 0;
Int64 s_frameTicks = 0;		///< smoothed duration of a frame, in ticks
Int64 s_frameAccumTicks = 0;	///< frame durations accumulated since the last publish, in ticks
Int64 s_lastPublishTicks = 0;	///< time of the last publish, in ticks
Int s_publishFrames = 0;		///< frames seen since the last publish
DisplayString *s_displayString = nullptr;

//-------------------------------------------------------------------------------------------------
// Returns the number of timer ticks per second, or 0 when the timer is not initialized yet.
//-------------------------------------------------------------------------------------------------
Int64 GetTicksPerSec()
{
	Int64 ticksPerSec = 0;
	GetPrecisionTimerTicksPerSec(&ticksPerSec);
	return ticksPerSec;
}

//-------------------------------------------------------------------------------------------------
// Converts the given amount of ticks into milliseconds.
//-------------------------------------------------------------------------------------------------
double TicksToMs(Int64 ticks, Int64 ticksPerSec)
{
	if (ticksPerSec <= 0)
	{
		return 0.0;
	}
	return (double)ticks * 1000.0 / (double)ticksPerSec;
}

//-------------------------------------------------------------------------------------------------
// Returns the value the counters are sorted by. The time is rounded to the given bucket,
// so that close counters keep their relative order instead of swapping on every refresh.
//-------------------------------------------------------------------------------------------------
Int64 GetSortKey(const PerfCounter &counter, Int64 bucketTicks)
{
	return (bucketTicks > 0) ? (counter.m_lastTicks / bucketTicks) : counter.m_lastTicks;
}

//-------------------------------------------------------------------------------------------------
// Creates the display string of the overlay, if it does not exist yet.
//-------------------------------------------------------------------------------------------------
DisplayString *GetDisplayString()
{
	if (s_displayString == nullptr)
	{
		if (TheDisplayStringManager == nullptr || TheFontLibrary == nullptr)
		{
			return nullptr;
		}

		s_displayString = TheDisplayStringManager->newDisplayString();
		if (s_displayString == nullptr)
		{
			return nullptr;
		}

		// Use the same font as the rest of the debug display.
		GameFont *font = nullptr;
		if (TheGlobalLanguageData && TheGlobalLanguageData->m_nativeDebugDisplay.name.isNotEmpty())
		{
			font = TheFontLibrary->getFont(
				TheGlobalLanguageData->m_nativeDebugDisplay.name,
				TheGlobalLanguageData->m_nativeDebugDisplay.size,
				TheGlobalLanguageData->m_nativeDebugDisplay.bold);
		}
		else
		{
			font = TheFontLibrary->getFont("FixedSys", 8, FALSE);
		}

		s_displayString->setFont(font);
	}

	return s_displayString;
}

}

//-------------------------------------------------------------------------------------------------
Int PerfOverlayRegister(const char *name, UnsignedInt detailGroup)
{
	for (Int i = 0; i < s_numCounters; ++i)
	{
		if (s_counters[i].m_name != nullptr && strcmp(s_counters[i].m_name, name) == 0)
		{
			return i;
		}
	}

	if (s_numCounters >= MAX_COUNTERS)
	{
		DEBUG_LOG(("PerfOverlay: too many counters, '%s' is ignored.", name));
		return -1;
	}

	const Int index = s_numCounters++;
	s_counters[index].m_name = name;
	s_counters[index].m_ticks = 0;
	s_counters[index].m_calls = 0;
	s_counters[index].m_lastTicks = 0;
	s_counters[index].m_lastCalls = 0;
	s_counters[index].m_peakTicks = 0;
	s_counters[index].m_value = 0;
	s_counters[index].m_lastValue = 0;
	s_counters[index].m_detailGroup = detailGroup;
	s_counters[index].m_accumTicks = 0;
	s_counters[index].m_accumCalls = 0;
	s_counters[index].m_accumValue = 0;
	s_counters[index].m_valueTotal = 0;
	s_counters[index].m_lastValueTotal = 0;
	s_counters[index].m_accumValueTotal = 0;

	return index;
}

//-------------------------------------------------------------------------------------------------
void PerfOverlayAddTime(Int index, Int64 ticks)
{
	if (index < 0 || index >= s_numCounters)
	{
		return;
	}

	s_counters[index].m_ticks += ticks;
}

//-------------------------------------------------------------------------------------------------
void PerfOverlayAddCall(Int index)
{
	if (index < 0 || index >= s_numCounters)
	{
		return;
	}

	++s_counters[index].m_calls;
}

//-------------------------------------------------------------------------------------------------
void PerfOverlaySetValue(Int index, Int value, Int total)
{
	if (index < 0 || index >= s_numCounters)
	{
		return;
	}

	s_counters[index].m_value = value;
	s_counters[index].m_valueTotal = total;
}

//-------------------------------------------------------------------------------------------------
void PerfOverlaySetDetailGroupVisible(UnsignedInt detailGroup, Bool visible)
{
	if (visible)
	{
		s_detailVisibleMask |= detailGroup;
	}
	else
	{
		s_detailVisibleMask &= ~detailGroup;
	}
}

//-------------------------------------------------------------------------------------------------
Bool PerfOverlayIsDetailGroupVisible(UnsignedInt detailGroup)
{
	return (s_detailVisibleMask & detailGroup) != 0;
}

//-------------------------------------------------------------------------------------------------
// Publishes the numbers of the last interval. The numbers are averaged over the interval and smoothed
// over the previous intervals, so the table does not flicker on every frame.
//-------------------------------------------------------------------------------------------------
static void PublishCounters()
{
	const Int numFrames = (s_publishFrames > 0) ? s_publishFrames : 1;

	for (Int i = 0; i < s_numCounters; ++i)
	{
		PerfCounter &counter = s_counters[i];

		const Int64 avgTicks = counter.m_accumTicks / numFrames;
		counter.m_lastCalls = (counter.m_accumCalls + numFrames / 2) / numFrames;
		counter.m_lastValue = (counter.m_accumValue + numFrames / 2) / numFrames;
		counter.m_lastValueTotal = (counter.m_accumValueTotal + numFrames / 2) / numFrames;

		if (counter.m_lastTicks == 0)
		{
			// The very first sample is taken as is, so the numbers settle right away.
			counter.m_lastTicks = avgTicks;
		}
		else
		{
			counter.m_lastTicks =
				(counter.m_lastTicks * (PERF_OVERLAY_SMOOTHING - 1) + avgTicks) / PERF_OVERLAY_SMOOTHING;
		}

		counter.m_accumTicks = 0;
		counter.m_accumCalls = 0;
		counter.m_accumValue = 0;
		counter.m_accumValueTotal = 0;
	}

	const Int64 avgFrameTicks = s_frameAccumTicks / numFrames;
	if (s_frameTicks == 0)
	{
		s_frameTicks = avgFrameTicks;
	}
	else
	{
		s_frameTicks =
			(s_frameTicks * (PERF_OVERLAY_SMOOTHING - 1) + avgFrameTicks) / PERF_OVERLAY_SMOOTHING;
	}

	s_frameAccumTicks = 0;
	s_publishFrames = 0;
}

//-------------------------------------------------------------------------------------------------
void PerfOverlayEndFrame()
{
	Int64 now = 0;
	GetPrecisionTimer(&now);

	if (s_lastEndFrameTicks != 0 && now > s_lastEndFrameTicks)
	{
		s_frameAccumTicks += now - s_lastEndFrameTicks;
	}
	s_lastEndFrameTicks = now;

	for (Int i = 0; i < s_numCounters; ++i)
	{
		PerfCounter &counter = s_counters[i];

		if (counter.m_ticks > counter.m_peakTicks)
		{
			counter.m_peakTicks = counter.m_ticks;
		}

		counter.m_accumTicks += counter.m_ticks;
		counter.m_accumCalls += counter.m_calls;
		counter.m_accumValue += counter.m_value;
		counter.m_accumValueTotal += counter.m_valueTotal;

		counter.m_ticks = 0;
		counter.m_calls = 0;
		counter.m_value = 0;
		counter.m_valueTotal = 0;
	}

	++s_publishFrames;

	const Int64 ticksPerSec = GetTicksPerSec();
	const Int64 publishInterval = (ticksPerSec > 0)
		? ticksPerSec * PERF_OVERLAY_PUBLISH_MS / 1000
		: 0;

	if (s_lastPublishTicks == 0)
	{
		s_lastPublishTicks = now;
		return;
	}

	if (publishInterval > 0 && now - s_lastPublishTicks < publishInterval)
	{
		return;
	}

	s_lastPublishTicks = now;
	PublishCounters();
}

//-------------------------------------------------------------------------------------------------
void PerfOverlayDraw()
{
	if (!s_visible)
	{
		return;
	}

	const Int64 ticksPerSec = GetTicksPerSec();
	if (ticksPerSec <= 0)
	{
		// The precision timer is not initialized yet.
		return;
	}

	DisplayString *displayString = GetDisplayString();
	if (displayString == nullptr || TheDisplay == nullptr)
	{
		return;
	}

	const double frameMs = TicksToMs(s_frameTicks, ticksPerSec);

	AsciiString text;
	text.format("Perf %.2f ms/frame\n", frameMs);

	// The sort keys are rounded to whole milliseconds, so that the rows do not change places
	// because of the noise of a single interval.
	const Int64 bucketTicks = (ticksPerSec * PERF_OVERLAY_SORT_BUCKET_MS) / 1000;

	// Sort the counters by the time they spent recently, the heaviest one first.
	Int order[MAX_COUNTERS];
	Int numShown = 0;
	for (Int i = 0; i < s_numCounters; ++i)
	{
		// The detail counters are only shown when their group is enabled.
		if (s_counters[i].m_detailGroup != 0 &&
			(s_detailVisibleMask & s_counters[i].m_detailGroup) == 0)
		{
			continue;
		}
		order[numShown++] = i;
	}

	for (Int i = 1; i < numShown; ++i)
	{
		const Int index = order[i];
		const Int64 key = GetSortKey(s_counters[index], bucketTicks);
		Int j = i - 1;
		while (j >= 0 && GetSortKey(s_counters[order[j]], bucketTicks) < key)
		{
			order[j + 1] = order[j];
			--j;
		}
		order[j + 1] = index;
	}

	AsciiString countText;
	AsciiString line;
	for (Int i = 0; i < numShown; ++i)
	{
		const PerfCounter &counter = s_counters[order[i]];
		const double ms = TicksToMs(counter.m_lastTicks, ticksPerSec);
		const double peakMs = TicksToMs(counter.m_peakTicks, ticksPerSec);
		const double percent = (frameMs > 0.0) ? (ms * 100.0 / frameMs) : 0.0;

		// The count of a counter can relate to a total number, e.g. the visible and all trees.
		if (counter.m_lastValueTotal > 0)
		{
			countText.format("%d/%d", counter.m_lastValue, counter.m_lastValueTotal);
		}
		else
		{
			countText.format("%d", counter.m_lastValue);
		}

		line.format("%-22s %7.3f ms %5.1f%% %6d calls %11s  peak %7.3f ms",
			counter.m_name, ms, percent, counter.m_lastCalls, countText.str(), peakMs);
		text.concat(line);
		if (i + 1 < numShown)
		{
			text.concat("\n");
		}
	}

	// The numbers only change a few times per second, so the text is only rendered when it changed.
	static AsciiString s_lastText;
	if (strcmp(s_lastText.str(), text.str()) != 0)
	{
		s_lastText = text;

		UnicodeString unicodeText;
		unicodeText.translate(text);
		displayString->setText(unicodeText);
	}

	Int width = 0;
	Int height = 0;
	displayString->getSize(&width, &height);

	const Int x = PERF_OVERLAY_POS_X;
	const Int y = PERF_OVERLAY_POS_Y;

	// Semi transparent background, so the text stays readable over the game.
	TheDisplay->drawFillRect(x - PERF_OVERLAY_PADDING, y - PERF_OVERLAY_PADDING,
		width + PERF_OVERLAY_PADDING * 2 + 2, height + PERF_OVERLAY_PADDING * 2,
		GameMakeColor(0, 0, 0, 140));

	displayString->draw(x, y, GameMakeColor(255, 255, 255, 255), GameMakeColor(0, 0, 0, 255));
}

//-------------------------------------------------------------------------------------------------
Bool PerfOverlayIsVisible()
{
	return s_visible;
}

//-------------------------------------------------------------------------------------------------
void PerfOverlaySetVisible(Bool visible)
{
	s_visible = visible;
}

//-------------------------------------------------------------------------------------------------
void PerfOverlayToggle()
{
	s_visible = !s_visible;
}

//-------------------------------------------------------------------------------------------------
PerfOverlayScope::PerfOverlayScope(Int index) :
	m_index(index),
	m_startTime(0)
{
	if (m_index < 0)
	{
		return;
	}

	GetPrecisionTimer(&m_startTime);
}

//-------------------------------------------------------------------------------------------------
PerfOverlayScope::~PerfOverlayScope()
{
	if (m_index < 0)
	{
		return;
	}

	Int64 now = 0;
	GetPrecisionTimer(&now);

	PerfOverlayAddTime(m_index, now - m_startTime);
	PerfOverlayAddCall(m_index);
}

#endif // RTS_DEBUG
