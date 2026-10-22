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
// GameWindowDebugOverlay.cpp
//
// Draws the technical name of a GUI window in its top left corner.
//
//////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/GameWindowDebugOverlay.h"

#if defined(RTS_DEBUG)

#include "Common/Debug.h"
#include "GameClient/Color.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/Keyboard.h"

namespace
{

//-------------------------------------------------------------------------------------------------
// The look of the labels.
enum
{
	LABEL_OFFSET_X = 2,					///< pixels between the window border and the label
	LABEL_OFFSET_Y = 1,					///< pixels between the window border and the label
	LABEL_PADDING = 2,					///< padding of the background behind the label
};

//-------------------------------------------------------------------------------------------------
// The state of the overlay.
Bool s_visible = TRUE;
Bool s_showFrames = FALSE;
GameFont *s_labelFont = nullptr;

//-------------------------------------------------------------------------------------------------
// The label of the window that is currently drawn. These are reused for every
// window, so their buffers are allocated only once.
AsciiString s_label;
AsciiString s_styleNames;
UnicodeString s_unicodeLabel;

//-------------------------------------------------------------------------------------------------
// The display string that is used to draw the label of a window. Every window
// gets its own one, so that the text of the label only has to be rendered
// once and not on every frame.
std::map<GameWindow *, DisplayString *> s_labelStrings;

//-------------------------------------------------------------------------------------------------
// The windows that were drawn in the last frame. Used to release the label
// strings of the windows that are gone.
std::set<GameWindow *> s_liveWindows;

//-------------------------------------------------------------------------------------------------
/** Is the overlay enabled for the current window manager? The GUI editor knows
	* where everything is, so it does not want the names of the windows. */
//-------------------------------------------------------------------------------------------------
Bool IsOverlayEnabled()
{

	if( TheWindowManager == nullptr )
		return FALSE;

	return TheWindowManager->getShowDebugNames();

}

//-------------------------------------------------------------------------------------------------
/** Get the font that is used to draw the labels. It is the same font that the
	* rest of the debug display uses. */
//-------------------------------------------------------------------------------------------------
GameFont *GetLabelFont()
{

	if( s_labelFont == nullptr )
	{

		if( TheFontLibrary == nullptr )
			return nullptr;

		// Use the same font as the rest of the debug display.
		if( TheGlobalLanguageData && TheGlobalLanguageData->m_nativeDebugDisplay.name.isNotEmpty() )
		{
			s_labelFont = TheFontLibrary->getFont(
				TheGlobalLanguageData->m_nativeDebugDisplay.name,
				TheGlobalLanguageData->m_nativeDebugDisplay.size,
				TheGlobalLanguageData->m_nativeDebugDisplay.bold);
		}
		else
		{
			s_labelFont = TheFontLibrary->getFont( "FixedSys", 8, FALSE );
		}

	}

	return s_labelFont;

}

//-------------------------------------------------------------------------------------------------
/** Get the display string that is used to draw the label of the given window.
	* It is created on demand. */
//-------------------------------------------------------------------------------------------------
DisplayString *GetLabelString( GameWindow *window )
{

	std::map<GameWindow *, DisplayString *>::const_iterator it = s_labelStrings.find( window );
	if( it != s_labelStrings.end() )
		return it->second;

	if( TheDisplayStringManager == nullptr )
		return nullptr;

	DisplayString *string = TheDisplayStringManager->newDisplayString();
	if( string == nullptr )
		return nullptr;

	string->setFont( GetLabelFont() );

	s_labelStrings[ window ] = string;

	return string;

}

//-------------------------------------------------------------------------------------------------
/** Release the label strings of the windows that do not exist anymore. */
//-------------------------------------------------------------------------------------------------
void ReleaseLabelStrings()
{

	for (std::map<GameWindow*, DisplayString*>::iterator it = s_labelStrings.begin(); it != s_labelStrings.end(); ++it)
	{
		if (it->second && TheDisplayStringManager)
			TheDisplayStringManager->freeDisplayString(it->second);
	}

	s_labelStrings.clear();

}

//-------------------------------------------------------------------------------------------------
/** Release the label strings of the windows that do not exist anymore. */
//-------------------------------------------------------------------------------------------------
void ReleaseDeadLabelStrings()
{

	for( std::map<GameWindow *, DisplayString *>::iterator it = s_labelStrings.begin(); it != s_labelStrings.end(); )
	{

		if( s_liveWindows.find( it->first ) == s_liveWindows.end() )
		{
			if( it->second && TheDisplayStringManager )
				TheDisplayStringManager->freeDisplayString( it->second );
			it = s_labelStrings.erase( it );
		}
		else
		{
			++it;
		}

	}

}

//-------------------------------------------------------------------------------------------------
/** Append the names of the styles of the given window to the label. This tells
	* what kind of control the window is, e.g. "PUSHBUTTON". */
//-------------------------------------------------------------------------------------------------
void AppendStyleNames( AsciiString &label, UnsignedInt style )
{

	Bool first = TRUE;

	for( Int i = 0; style != 0 && WindowStyleNames[ i ] != nullptr; ++i )
	{

		if( !BitIsSet( style, ( 1 << i ) ) )
			continue;

		if( !first )
			label.concat( "|" );
		first = FALSE;

		label.concat( WindowStyleNames[ i ] );

	}

}

//-------------------------------------------------------------------------------------------------
/** Build the text that is shown for the given window. */
//-------------------------------------------------------------------------------------------------
void BuildLabel( GameWindow *window )
{

	s_label = window->winGetDebugName();

	// Add the kind of the window, so it is obvious what type of control it is.
	const UnsignedInt style = window->winGetStyle();
	if( style != 0 )
	{
		s_styleNames.truncateTo( 0 );
		AppendStyleNames( s_styleNames, style );

		if( s_styleNames.isNotEmpty() )
		{
			s_label.concat( " [" );
			s_label.concat( s_styleNames );
			s_label.concat( "]" );
		}

	}

}

}

//-------------------------------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
/** Prepare the overlay for a new frame. */
//-------------------------------------------------------------------------------------------------
void GameWindowDebugOverlayUpdate()
{

	if( !IsOverlayEnabled() )
	{
		// The overlay is not wanted here, get rid of the label strings.
		ReleaseDeadLabelStrings();
		s_liveWindows.clear();
		return;
	}

	// Release the label strings of the windows that were gone in the last frame.
	ReleaseDeadLabelStrings();

	s_liveWindows.clear();

}

//-------------------------------------------------------------------------------------------------
/** Draw the technical name of the given window in its top left corner.
	*
	* This is called while the window itself is drawn, so that the label is
	* covered by the windows that are drawn on top of it.
	*/
//-------------------------------------------------------------------------------------------------
void GameWindowDebugOverlayDrawWindow( GameWindow *window )
{

	if( !s_visible || !IsOverlayEnabled() )
		return;

	if( window == nullptr || TheDisplay == nullptr )
		return;

	// Remember that this window is still alive, so that its label string is
	// not released while we are drawing it.
	s_liveWindows.insert( window );

	Int x = 0;
	Int y = 0;
	window->winGetScreenPosition( &x, &y );

	// Draw the frame of the window, so that the position and the size of the
	// windows can be seen.
	if( s_showFrames )
	{
		Int width = 0;
		Int height = 0;
		window->winGetSize( &width, &height );

		TheDisplay->drawOpenRect( x, y, width, height, 1.0f, GameMakeColor( 0, 255, 255, 255 ) );
	}

	GameFont *font = GetLabelFont();
	if( font == nullptr )
		return;

	BuildLabel( window );

	if( s_label.isEmpty() )
		return;

	// Every window has its own display string, so the text of the label only
	// has to be rendered once and not on every frame.
	DisplayString *labelString = GetLabelString( window );
	if( labelString == nullptr )
		return;

	if( labelString->getFont() != font )
		labelString->setFont( font );

	s_unicodeLabel.translate( s_label );
	if( labelString->getText() != s_unicodeLabel )
		labelString->setText( s_unicodeLabel );

	x += LABEL_OFFSET_X;
	y += LABEL_OFFSET_Y;

	Int width = 0;
	Int height = 0;
	labelString->getSize( &width, &height );

	// A semi transparent background, so the text stays readable over the game.
	TheDisplay->drawFillRect( x - LABEL_PADDING, y - LABEL_PADDING,
		width + LABEL_PADDING * 2 + 2, height + LABEL_PADDING * 2,
		GameMakeColor( 0, 0, 0, 140 ) );

	labelString->draw( x, y, GameMakeColor( 255, 255, 0, 255 ), GameMakeColor( 0, 0, 0, 255 ) );

}

//-------------------------------------------------------------------------------------------------
/** Release the label strings of all the windows. This has to be called before
	* the display string manager is destroyed. */
	//-------------------------------------------------------------------------------------------------
void GameWindowDebugOverlayShutdown()
{

	ReleaseLabelStrings();

	s_liveWindows.clear();

}

//-------------------------------------------------------------------------------------------------
/** Should the frames of the windows be drawn as well? */
//-------------------------------------------------------------------------------------------------
void GameWindowDebugOverlaySetShowFrames( Bool show )
{

	s_showFrames = show;

}

//-------------------------------------------------------------------------------------------------
/** Should the frames of the windows be drawn as well? */
//-------------------------------------------------------------------------------------------------
Bool GameWindowDebugOverlayGetShowFrames()
{

	return s_showFrames;

}

//-------------------------------------------------------------------------------------------------
/** Is the overlay visible? */
//-------------------------------------------------------------------------------------------------
Bool GameWindowDebugOverlayIsVisible()
{

	return s_visible;

}

//-------------------------------------------------------------------------------------------------
/** Show or hide the overlay. */
//-------------------------------------------------------------------------------------------------
void GameWindowDebugOverlaySetVisible( Bool visible )
{

	s_visible = visible;

}

//-------------------------------------------------------------------------------------------------
/** Show or hide the overlay. */
//-------------------------------------------------------------------------------------------------
void GameWindowDebugOverlayToggle()
{

	s_visible = !s_visible;

}

#endif // RTS_DEBUG
