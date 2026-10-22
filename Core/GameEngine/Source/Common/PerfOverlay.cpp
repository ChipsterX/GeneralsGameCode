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
	Int64		m_lastTicks;		///< ticks of the last published frame
	Int			m_lastCalls;		///< calls of the last published frame
	Int64		m_peakTicks;		///< worst case ticks seen so far
};

//-------------------------------------------------------------------------------------------------
enum { MAX_COUNTERS = PERF_OVERLAY_MAX_COUNTERS };

// The key that toggles the overlay.
const KeyDefType PERF_OVERLAY_TOGGLE_KEY = KEY_F11;

// Position and look of the overlay.
enum
{
	PERF_OVERLAY_POS_X = 8,			///< pixels from the left screen edge
	PERF_OVERLAY_POS_Y = 64,			///< pixels from the top screen edge
	PERF_OVERLAY_PADDING = 5,		///< padding around the text, in pixels
};

PerfCounter s_counters[MAX_COUNTERS];
Int s_numCounters = 0;
Bool s_visible = TRUE;
Bool s_toggleKeyIsDown = FALSE;
Int64 s_lastEndFrameTicks = 0;
Int64 s_frameTicks = 0;		///< duration of the last frame, in ticks
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

//-------------------------------------------------------------------------------------------------
// Toggles the overlay when the hotkey is pressed.
// The keyboard has already consumed the key events by the time we get here,
// so the key state is inspected directly and the toggle only happens once per press.
//-------------------------------------------------------------------------------------------------
void CheckToggleHotkey()
{
	if (TheKeyboard == nullptr)
	{
		return;
	}

	for (KeyboardIO *io = TheKeyboard->getFirstKey(); io->key != KEY_NONE; ++io)
	{
		if (io->key != PERF_OVERLAY_TOGGLE_KEY)
		{
			continue;
		}

		if (BitIsSet(io->state, KEY_STATE_UP))
		{
			// The key was released, so the next press toggles the overlay again.
			s_toggleKeyIsDown = FALSE;
		}
		else if (BitIsSet(io->state, KEY_STATE_DOWN) && !s_toggleKeyIsDown)
		{
			// The key was pressed. Only toggle once, no matter how long it is held.
			s_toggleKeyIsDown = TRUE;
			PerfOverlayToggle();
		}
	}
}

}

//-------------------------------------------------------------------------------------------------
Int PerfOverlayRegister(const char *name)
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
void PerfOverlayEndFrame()
{
	Int64 now = 0;
	GetPrecisionTimer(&now);

	if (s_lastEndFrameTicks != 0 && now > s_lastEndFrameTicks)
	{
		s_frameTicks = now - s_lastEndFrameTicks;
	}
	s_lastEndFrameTicks = now;

	for (Int i = 0; i < s_numCounters; ++i)
	{
		s_counters[i].m_lastTicks = s_counters[i].m_ticks;
		s_counters[i].m_lastCalls = s_counters[i].m_calls;

		if (s_counters[i].m_lastTicks > s_counters[i].m_peakTicks)
		{
			s_counters[i].m_peakTicks = s_counters[i].m_lastTicks;
		}

		s_counters[i].m_ticks = 0;
		s_counters[i].m_calls = 0;
	}
}

//-------------------------------------------------------------------------------------------------
void PerfOverlayDraw()
{
	CheckToggleHotkey();

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
	text.format("Perf %.2f ms/frame (F11 hides)\n", frameMs);

	// Sort the counters by the time they spent in the last frame, the heaviest one first.
	Int order[MAX_COUNTERS];
	for (Int i = 0; i < s_numCounters; ++i)
	{
		order[i] = i;
	}

	for (Int i = 1; i < s_numCounters; ++i)
	{
		const Int index = order[i];
		const Int64 ticks = s_counters[index].m_lastTicks;
		Int j = i - 1;
		while (j >= 0 && s_counters[order[j]].m_lastTicks < ticks)
		{
			order[j + 1] = order[j];
			--j;
		}
		order[j + 1] = index;
	}

	AsciiString line;
	for (Int i = 0; i < s_numCounters; ++i)
	{
		const PerfCounter &counter = s_counters[order[i]];
		const double ms = TicksToMs(counter.m_lastTicks, ticksPerSec);
		const double peakMs = TicksToMs(counter.m_peakTicks, ticksPerSec);
		const double percent = (frameMs > 0.0) ? (ms * 100.0 / frameMs) : 0.0;

		line.format("%-22s %7.3f ms %5.1f%% %6d calls  peak %7.3f ms",
			counter.m_name, ms, percent, counter.m_lastCalls, peakMs);
		text.concat(line);
		if (i + 1 < s_numCounters)
		{
			text.concat("\n");
		}
	}

	UnicodeString unicodeText;
	unicodeText.translate(text);
	displayString->setText(unicodeText);

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
