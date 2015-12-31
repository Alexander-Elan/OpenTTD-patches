/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file misc_sl.cpp Saving and loading of things that didn't fit anywhere else */

#include "../stdafx.h"
#include "../date_func.h"
#include "../zoom_func.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../viewport_func.h"
#include "../gfx_func.h"
#include "../core/random_func.hpp"
#include "../fios.h"

#include "saveload_buffer.h"

extern TileIndex _cur_tileloop_tile;
extern uint16 _disaster_delay;
extern byte _trees_tick_ctr;

/* Keep track of current game position */
int _saved_scrollpos_x;
int _saved_scrollpos_y;
ZoomLevelByte _saved_scrollpos_zoom;

void SaveViewportBeforeSaveGame()
{
	const Window *w = FindWindowById(WC_MAIN_WINDOW, 0);

	if (w != NULL) {
		_saved_scrollpos_x = w->viewport->scrollpos_x;
		_saved_scrollpos_y = w->viewport->scrollpos_y;
		_saved_scrollpos_zoom = w->viewport->zoom;
	}
}

void ResetViewportAfterLoadGame()
{
	Window *w = FindWindowById(WC_MAIN_WINDOW, 0);

	w->viewport->scrollpos_x = _saved_scrollpos_x;
	w->viewport->scrollpos_y = _saved_scrollpos_y;
	w->viewport->dest_scrollpos_x = _saved_scrollpos_x;
	w->viewport->dest_scrollpos_y = _saved_scrollpos_y;

	ViewPort *vp = w->viewport;
	vp->zoom = (ZoomLevel)min(_saved_scrollpos_zoom, ZOOM_LVL_MAX);
	vp->virtual_width = ScaleByZoom(vp->width, vp->zoom);
	vp->virtual_height = ScaleByZoom(vp->height, vp->zoom);

	/* If zoom_max is ZOOM_LVL_MIN then the setting has not been loaded yet, therefore all levels are allowed. */
	if (_settings_client.gui.zoom_max != ZOOM_LVL_MIN) {
		/* Ensure zoom level is allowed */
		while (vp->zoom < _settings_client.gui.zoom_min) DoZoomInOutWindow(ZOOM_OUT, w);
		while (vp->zoom > _settings_client.gui.zoom_max) DoZoomInOutWindow(ZOOM_IN, w);
	}

	vp->virtual_left = w->viewport->scrollpos_x;
	vp->virtual_top = w->viewport->scrollpos_y;
	w->InvalidateData(); // update button status
	MarkWholeScreenDirty();
}

byte _age_cargo_skip_counter; ///< Skip aging of cargo? Used before savegame version 162.

static const SaveLoad _date_desc[] = {
	SLEG_VAR(_date,                   SLE_FILE_U16 | SLE_VAR_I32,  , ,   0,  30),
	SLEG_VAR(_date,                   SLE_INT32,                  0, ,  31,    ),
	SLEG_VAR(_date_fract,             SLE_UINT16),
	SLEG_VAR(_tick_counter,           SLE_UINT16),
	SLE_NULL(2, , , 0, 156), // _vehicle_id_ctr_day
	SLEG_VAR(_age_cargo_skip_counter, SLE_UINT8,                   , ,   0, 161),
	SLE_NULL(1, , , 0, 45),
	SLEG_VAR(_cur_tileloop_tile,      SLE_FILE_U16 | SLE_VAR_U32,  , ,   0,   5),
	SLEG_VAR(_cur_tileloop_tile,      SLE_UINT32,                 0, ,   6,    ),
	SLEG_VAR(_disaster_delay,         SLE_UINT16),
	SLE_NULL(2, , , 0, 119),
	SLEG_VAR(_random.state[0],        SLE_UINT32),
	SLEG_VAR(_random.state[1],        SLE_UINT32),
	SLE_NULL(1, , ,  0,   9),
	SLE_NULL(4, , , 10, 119),
	SLEG_VAR(_cur_company_tick_index, SLE_FILE_U8  | SLE_VAR_U32),
	SLEG_VAR(_next_competitor_start,  SLE_FILE_U16 | SLE_VAR_U32,  , ,   0, 108),
	SLEG_VAR(_next_competitor_start,  SLE_UINT32,                 0, , 109,    ),
	SLEG_VAR(_trees_tick_ctr,         SLE_UINT8),
	SLEG_VAR(_pause_mode,             SLE_UINT8,                  0, ,   4,    ),
	SLE_NULL(4, , , 11, 119),
	 SLE_END()
};

static const SaveLoad _date_check_desc[] = {
	SLEG_VAR(_load_check_data.current_date,  SLE_FILE_U16 | SLE_VAR_I32,  , ,  0, 30),
	SLEG_VAR(_load_check_data.current_date,  SLE_INT32,                  0, , 31,   ),
	SLE_NULL(2),                   // _date_fract
	SLE_NULL(2),                   // _tick_counter
	SLE_NULL(2,  , ,   0, 156),    // _vehicle_id_ctr_day
	SLE_NULL(1,  , ,   0, 161),    // _age_cargo_skip_counter
	SLE_NULL(1,  , ,   0,  45),
	SLE_NULL(2,  , ,   0,   5),    // _cur_tileloop_tile
	SLE_NULL(4, 0, ,   6,    ),    // _cur_tileloop_tile
	SLE_NULL(2),                   // _disaster_delay
	SLE_NULL(2,  , ,   0, 119),
	SLE_NULL(4),                   // _random.state[0]
	SLE_NULL(4),                   // _random.state[1]
	SLE_NULL(1,  , ,   0,   9),
	SLE_NULL(4,  , ,  10, 119),
	SLE_NULL(1),                   // _cur_company_tick_index
	SLE_NULL(2,  , ,   0, 108),    // _next_competitor_start
	SLE_NULL(4, 0, , 109,    ),    // _next_competitor_start
	SLE_NULL(1),                   // _trees_tick_ctr
	SLE_NULL(1, 0, ,   4,    ),    // _pause_mode
	SLE_NULL(4,  , ,  11, 119),
	 SLE_END()
};

/* Save date-related variables as well as persistent tick counters
 * XXX: currently some unrelated stuff is just put here */
static void Save_DATE(SaveDumper *dumper)
{
	dumper->WriteRIFFObject(NULL, _date_desc);
}

/* Load date-related variables as well as persistent tick counters
 * XXX: currently some unrelated stuff is just put here */
static void Load_DATE(LoadBuffer *reader)
{
	reader->ReadObject(NULL, _date_desc);
}

static void Check_DATE(LoadBuffer *reader)
{
	reader->ReadObject(NULL, _date_check_desc);
	if (reader->IsOTTDVersionBefore(31)) {
		_load_check_data.current_date += DAYS_TILL_ORIGINAL_BASE_YEAR;
	}
}


static const SaveLoad _view_desc[] = {
	SLEG_VAR(_saved_scrollpos_x,    SLE_FILE_I16 | SLE_VAR_I32,  , , 0, 5),
	SLEG_VAR(_saved_scrollpos_x,    SLE_INT32,                  0, , 6,  ),
	SLEG_VAR(_saved_scrollpos_y,    SLE_FILE_I16 | SLE_VAR_I32,  , , 0, 5),
	SLEG_VAR(_saved_scrollpos_y,    SLE_INT32,                  0, , 6,  ),
	SLEG_VAR(_saved_scrollpos_zoom, SLE_UINT8),
	 SLE_END()
};

static void Save_VIEW(SaveDumper *dumper)
{
	dumper->WriteRIFFObject(NULL, _view_desc);
}

static void Load_VIEW(LoadBuffer *reader)
{
	reader->ReadObject(NULL, _view_desc);
}

extern const ChunkHandler _misc_chunk_handlers[] = {
	{ 'DATE', Save_DATE, Load_DATE, NULL, Check_DATE, CH_RIFF},
	{ 'VIEW', Save_VIEW, Load_VIEW, NULL, NULL,       CH_RIFF | CH_LAST},
};
