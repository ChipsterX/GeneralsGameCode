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
// GameWindowDebugPanel.h
//
// The debug options panel (debug builds only).
//
// It is a small window that can be opened in the game with Ctrl+Alt+D. It
// contains the check boxes for the different debug overlays of the engine, so
// that they can be switched on and off while playing.
//
// Ctrl+Alt+N switches the names of the GUI windows on and off without having
// to open the panel.
//
// The panel is not shown in the GUI editor, it knows where everything is.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/GameCommon.h"

// The panel is only available in debug builds.
#if defined(RTS_DEBUG)

//-------------------------------------------------------------------------------------------------
// Handles the hotkeys of the debug options and updates the panel.
// This is called once per frame, before the windows are drawn.
//-------------------------------------------------------------------------------------------------
void GameWindowDebugPanelUpdate();

//-------------------------------------------------------------------------------------------------
// Closes the panel and releases its windows. This has to be called before the
// window manager is destroyed.
//-------------------------------------------------------------------------------------------------
void GameWindowDebugPanelShutdown();

#else // RTS_DEBUG

// Builds without the panel: all calls are no-ops.
inline void GameWindowDebugPanelUpdate() {}
inline void GameWindowDebugPanelShutdown() {}

#endif // RTS_DEBUG
