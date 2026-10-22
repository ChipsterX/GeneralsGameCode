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
// GameWindowDebugPanel.cpp
//
// The debug options panel (debug builds only).
//
// The panel is a plain window that draws its own check boxes, so that it does
// not depend on any of the game data. It is not modal, the game keeps working
// while it is open, only the clicks on the panel itself are used by it.
//
//////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/GameWindowDebugPanel.h"

#if defined(RTS_DEBUG)

#include "Common/Debug.h"
#include "Common/PerfOverlay.h"
#include "GameClient/Color.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowDebugOverlay.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/Keyboard.h"

namespace
{

//-------------------------------------------------------------------------------------------------
// The hotkeys of the debug options. They use the control and the alt key, so
// that they do not collide with the shortcuts of the debugger.
//-------------------------------------------------------------------------------------------------
const KeyDefType NAMES_KEY = KEY_N;
const KeyDefType PANEL_KEY = KEY_D;
const UnsignedInt HOTKEY_MODIFIER = KEY_STATE_CONTROL | KEY_STATE_ALT;

//-------------------------------------------------------------------------------------------------
// The options of the panel.
//-------------------------------------------------------------------------------------------------
enum DebugOption
{
	DEBUG_OPTION_NAMES = 0,			///< show the names of the GUI windows
	DEBUG_OPTION_FRAMES,			///< show the frames of the GUI windows
	DEBUG_OPTION_PERF,				///< show the profiling overlay
	DEBUG_OPTION_COUNT
};

const char *const s_optionLabels[DEBUG_OPTION_COUNT] =
{
	"Show the names of the GUI windows",
	"Show the frames of the GUI windows",
	"Show the profiling overlay",
};

const char *const s_optionTitle = "Debug options";

//-------------------------------------------------------------------------------------------------
// The look and the position of the panel.
//-------------------------------------------------------------------------------------------------
enum
{
	PANEL_MARGIN = 16,					///< distance to the screen border
	PANEL_WIDTH = 320,					///< width of the panel
	PANEL_HEIGHT = 130,					///< height of the panel
	TITLE_HEIGHT = 22,					///< height of the title line
	BOX_SIZE = 14,						///< size of the check boxes
	BOX_X = 12,							///< horizontal position of the check boxes
	ROW_HEIGHT = 26,					///< vertical distance between the check boxes
};

//-------------------------------------------------------------------------------------------------
// The windows and the strings of the panel.
//-------------------------------------------------------------------------------------------------
GameWindow *s_panel = nullptr;
DisplayString *s_titleString = nullptr;
DisplayString *s_optionStrings[DEBUG_OPTION_COUNT] = { nullptr };

Bool s_panelKeyIsDown = FALSE;
Bool s_namesKeyIsDown = FALSE;

//-------------------------------------------------------------------------------------------------
/** Get the value of the given option. */
//-------------------------------------------------------------------------------------------------
Bool GetOptionValue( DebugOption option )
{

	switch( option )
	{
		case DEBUG_OPTION_NAMES: return GameWindowDebugOverlayIsVisible();
		case DEBUG_OPTION_FRAMES: return GameWindowDebugOverlayGetShowFrames();
		case DEBUG_OPTION_PERF:   return PerfOverlayIsVisible();
		default: break;
	}

	return FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Set the value of the given option. */
//-------------------------------------------------------------------------------------------------
void SetOptionValue( DebugOption option, Bool value )
{

	switch( option )
	{
		case DEBUG_OPTION_NAMES: GameWindowDebugOverlaySetVisible( value ); break;
		case DEBUG_OPTION_FRAMES: GameWindowDebugOverlaySetShowFrames( value ); break;
		case DEBUG_OPTION_PERF:   PerfOverlaySetVisible( value ); break;
		default: break;
	}

}

//-------------------------------------------------------------------------------------------------
/** Get the font that is used for the panel. It is the same font that the rest
	* of the debug display uses. */
//-------------------------------------------------------------------------------------------------
GameFont *GetPanelFont()
{

	if( TheFontLibrary == nullptr )
		return nullptr;

	if( TheGlobalLanguageData && TheGlobalLanguageData->m_nativeDebugDisplay.name.isNotEmpty() )
	{
		return TheFontLibrary->getFont(
			TheGlobalLanguageData->m_nativeDebugDisplay.name,
			TheGlobalLanguageData->m_nativeDebugDisplay.size,
			TheGlobalLanguageData->m_nativeDebugDisplay.bold);
	}

	return TheFontLibrary->getFont( "FixedSys", 10, FALSE );

}

//-------------------------------------------------------------------------------------------------
/** Get the display string of the given text, it is created on demand. */
//-------------------------------------------------------------------------------------------------
DisplayString *GetString( DisplayString **strings, const char *text )
{

	if( *strings != nullptr )
		return *strings;

	if( TheDisplayStringManager == nullptr )
		return nullptr;

	*strings = TheDisplayStringManager->newDisplayString();
	if( *strings == nullptr )
		return nullptr;

	(*strings)->setFont( GetPanelFont() );

	UnicodeString label;
	label.translate( AsciiString( text ) );
	(*strings)->setText( label );

	return *strings;

}

//-------------------------------------------------------------------------------------------------
/** Draw the check box of the given option. */
//-------------------------------------------------------------------------------------------------
void DrawCheckBox( Int x, Int y, Bool isChecked )
{

	// the box
	TheDisplay->drawFillRect( x, y, BOX_SIZE, BOX_SIZE, GameMakeColor( 40, 40, 40, 255 ) );
	TheDisplay->drawOpenRect( x, y, BOX_SIZE, BOX_SIZE, 1.0f, GameMakeColor( 220, 220, 220, 255 ) );

	if( !isChecked )
		return;

	// the check mark
	const Color color = GameMakeColor( 80, 255, 80, 255 );
	const Int middle = x + BOX_SIZE / 2;
	const Int bottom = y + BOX_SIZE - 3;
	TheDisplay->drawLine( x + 3, y + BOX_SIZE / 2, middle, bottom, 2.0f, color );
	TheDisplay->drawLine( middle, bottom, x + BOX_SIZE - 2, y + 3, 2.0f, color );

}

//-------------------------------------------------------------------------------------------------
/** Draw the background and the contents of the panel. */
//-------------------------------------------------------------------------------------------------
void PanelDraw( GameWindow *window, WinInstanceData *instData )
{

	Int x = 0;
	Int y = 0;
	window->winGetScreenPosition( &x, &y );

	Int width = 0;
	Int height = 0;
	window->winGetSize( &width, &height );

	// A semi transparent background, so the panel stays readable over the game.
	TheDisplay->drawFillRect( x, y, width, height, GameMakeColor( 0, 0, 0, 200 ) );
	TheDisplay->drawOpenRect( x, y, width, height, 1.0f, GameMakeColor( 255, 255, 0, 255 ) );

	const Color textColor = GameMakeColor( 255, 255, 255, 255 );
	const Color shadowColor = GameMakeColor( 0, 0, 0, 255 );

	// the title
	DisplayString *title = GetString( &s_titleString, s_optionTitle );
	if( title )
		title->draw( x + BOX_X, y + 4, textColor, shadowColor );

	// the options
	for( Int i = 0; i < DEBUG_OPTION_COUNT; i++ )
	{
		const Int boxY = y + TITLE_HEIGHT + i * ROW_HEIGHT;

		DrawCheckBox( x + BOX_X, boxY, GetOptionValue( (DebugOption)i ) );

		DisplayString *label = GetString( &s_optionStrings[ i ], s_optionLabels[ i ] );
		if( label )
			label->draw( x + BOX_X + BOX_SIZE + 8, boxY, textColor, shadowColor );
	}

}

//-------------------------------------------------------------------------------------------------
/** The input callback of the panel. It handles the clicks on the check boxes. */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType PanelInput( GameWindow *window, UnsignedInt msg,
	WindowMsgData mData1, WindowMsgData mData2 )
{

	switch( msg )
	{

		case GWM_LEFT_DOWN:
		{
			// Take the mouse, so that the click does not reach the game.
			return MSG_HANDLED;
		}

		case GWM_LEFT_UP:
		{
			const Int mouseX = (Int)( mData1 & 0xFFFF );
			const Int mouseY = (Int)( ( mData1 >> 16 ) & 0xFFFF );

			Int x = 0;
			Int y = 0;
			window->winGetScreenPosition( &x, &y );

			for( Int i = 0; i < DEBUG_OPTION_COUNT; i++ )
			{
				const Int boxX = x + BOX_X;
				const Int boxY = y + TITLE_HEIGHT + i * ROW_HEIGHT;

				if( mouseX >= boxX && mouseX <= boxX + BOX_SIZE &&
						mouseY >= boxY && mouseY <= boxY + BOX_SIZE )
				{
					SetOptionValue( (DebugOption)i, !GetOptionValue( (DebugOption)i ) );
					return MSG_HANDLED;
				}
			}

			return MSG_HANDLED;
		}

	}

	return MSG_IGNORED;

}

//-------------------------------------------------------------------------------------------------
/** The system callback of the panel. */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType PanelSystem( GameWindow *window, UnsignedInt msg,
	WindowMsgData mData1, WindowMsgData mData2 )
{

	switch( msg )
	{

		case GWM_DESTROY:
		{
			// The panel is going away, forget about it.
			s_panel = nullptr;
			return MSG_HANDLED;
		}

	}

	return MSG_IGNORED;

}

//-------------------------------------------------------------------------------------------------
/** Create the panel. */
//-------------------------------------------------------------------------------------------------
void CreatePanel()
{

	if( s_panel != nullptr )
		return;

	if( TheWindowManager == nullptr || TheDisplay == nullptr )
		return;

	// Put the panel in the upper right corner, the profiling overlay is in the
	// upper left one.
	Int x = (Int)TheDisplay->getWidth() - PANEL_WIDTH - PANEL_MARGIN;
	if( x < PANEL_MARGIN )
		x = PANEL_MARGIN;

	s_panel = TheWindowManager->winCreate( nullptr,
		WIN_STATUS_ENABLED | WIN_STATUS_ABOVE | WIN_STATUS_NO_FOCUS,
		x, PANEL_MARGIN, PANEL_WIDTH, PANEL_HEIGHT,
		PanelSystem );

	if( s_panel == nullptr )
		return;

	s_panel->winSetDrawFunc( PanelDraw );
	s_panel->winSetInputFunc( PanelInput );

}

//-------------------------------------------------------------------------------------------------
/** Close the panel. */
//-------------------------------------------------------------------------------------------------
void DestroyPanel()
{

	if( s_panel == nullptr )
		return;

	GameWindow *panel = s_panel;
	s_panel = nullptr;

	TheWindowManager->winDestroy( panel );

}

//-------------------------------------------------------------------------------------------------
/** Toggle the panel. */
//-------------------------------------------------------------------------------------------------
void TogglePanel()
{

	if( s_panel != nullptr )
	{
		DestroyPanel();
	}
	else
	{
		CreatePanel();
	}

}

//-------------------------------------------------------------------------------------------------
/** Handle the hotkeys of the debug options. */
//-------------------------------------------------------------------------------------------------
void CheckHotkeys()
{

	if( TheKeyboard == nullptr )
		return;

	for( KeyboardIO *io = TheKeyboard->getFirstKey(); io->key != KEY_NONE; ++io )
	{

		// The hotkeys need the control and the alt key, otherwise they would
		// collide with the shortcuts of the debugger.
		if( !BitIsSet( io->state, HOTKEY_MODIFIER ) )
			continue;

		if( io->key == NAMES_KEY )
		{
			if( BitIsSet( io->state, KEY_STATE_UP ) )
			{
				s_namesKeyIsDown = FALSE;
			}
			else if( BitIsSet( io->state, KEY_STATE_DOWN ) && !s_namesKeyIsDown )
			{
				s_namesKeyIsDown = TRUE;
				GameWindowDebugOverlayToggle();
			}
		}
		else if( io->key == PANEL_KEY )
		{
			if( BitIsSet( io->state, KEY_STATE_UP ) )
			{
				s_panelKeyIsDown = FALSE;
			}
			else if( BitIsSet( io->state, KEY_STATE_DOWN ) && !s_panelKeyIsDown )
			{
				s_panelKeyIsDown = TRUE;
				TogglePanel();
			}
		}

	}

}

}

//-------------------------------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
/** Handle the hotkeys and update the panel. */
//-------------------------------------------------------------------------------------------------
void GameWindowDebugPanelUpdate()
{

	// The GUI editor knows where everything is, it does not want the panel.
	if( TheWindowManager == nullptr || !TheWindowManager->getShowDebugNames() )
		return;

	CheckHotkeys();

}

//-------------------------------------------------------------------------------------------------
/** Close the panel and release its windows. */
//-------------------------------------------------------------------------------------------------
void GameWindowDebugPanelShutdown()
{

	DestroyPanel();

	if( TheDisplayStringManager )
	{
		if( s_titleString )
		{
			TheDisplayStringManager->freeDisplayString( s_titleString );
			s_titleString = nullptr;
		}

		for( Int i = 0; i < DEBUG_OPTION_COUNT; i++ )
		{
			if( s_optionStrings[ i ] )
			{
				TheDisplayStringManager->freeDisplayString( s_optionStrings[ i ] );
				s_optionStrings[ i ] = nullptr;
			}
		}
	}

}

#endif // RTS_DEBUG
