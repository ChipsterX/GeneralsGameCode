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
// GameWindowDebugOverlay.h
//
// A small on screen overlay (debug builds only) that shows the technical name
// of every GUI window in the top left corner of that window.
//
// The technical name is the name the window was saved with in its window
// layout file (.wnd). Those names are decorated with the file they were saved
// in, e.g. "Menus/MainMenu.wnd:Button.SinglePlayer", so the overlay tells you
// both the name of the control and the file it comes from.
//
// Windows that are created in code instead of being loaded from a layout file
// can be named with GameWindow::winSetDebugName().
//
// The label of a window is drawn as part of that window, so a window that is
// drawn on top of it hides its label. The overlay can be toggled with the F12
// key.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/GameCommon.h"

class GameWindow;

// The overlay is only available in debug builds.
#if defined(RTS_DEBUG)

//-------------------------------------------------------------------------------------------------
// Prepares the overlay for a new frame. This checks the hotkey that toggles the
// overlay and releases the label strings of the windows that are gone.
// This is called once per frame, before the windows are drawn.
//-------------------------------------------------------------------------------------------------
void GameWindowDebugOverlayUpdate();

//-------------------------------------------------------------------------------------------------
// Draws the technical name of the given window in its top left corner.
// This is called for every window that is drawn, so the label of a window is
// covered by the windows that are drawn on top of it.
//-------------------------------------------------------------------------------------------------
void GameWindowDebugOverlayDrawWindow( GameWindow *window );

//-------------------------------------------------------------------------------------------------
// Releases the label strings of all the windows. This has to be called before
// the display string manager is destroyed.
//-------------------------------------------------------------------------------------------------
void GameWindowDebugOverlayShutdown();

//-------------------------------------------------------------------------------------------------
// Visibility of the overlay.
//-------------------------------------------------------------------------------------------------
Bool GameWindowDebugOverlayIsVisible();
void GameWindowDebugOverlaySetVisible(Bool visible);
void GameWindowDebugOverlayToggle();

#else // RTS_DEBUG

// Builds without the overlay: all calls are no-ops.
inline void GameWindowDebugOverlayUpdate() {}
inline void GameWindowDebugOverlayDrawWindow( GameWindow * ) {}
inline void GameWindowDebugOverlayShutdown() {}
inline Bool GameWindowDebugOverlayIsVisible() { return false; }
inline void GameWindowDebugOverlaySetVisible(Bool) {}
inline void GameWindowDebugOverlayToggle() {}

#endif // RTS_DEBUG
