/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file landscape.cpp Functions related to the landscape (slopes etc.). */

/** @defgroup SnowLineGroup Snowline functions and data structures */

#include "stdafx.h"
#include "map/ground.h"
#include "map/slope.h"
#include "map/bridge.h"
#include "heightmap.h"
#include "spritecache.h"
#include "viewport_func.h"
#include "command_func.h"
#include "landscape.h"
#include "tgp.h"
#include "genworld.h"
#include "fios.h"
#include "date_func.h"
#include "water.h"
#include "effectvehicle_func.h"
#include "landscape_type.h"
#include "animated_tile_func.h"
#include "core/random_func.hpp"
#include "object_base.h"
#include "company_func.h"
#include "pathfinder/yapf/astar.hpp"
#include <queue>
#include <set>

#include "table/strings.h"
#include "table/sprites.h"

extern const TileTypeProcs
	_tile_type_clear_procs,
	_tile_type_rail_procs,
	_tile_type_road_procs,
	_tile_type_town_procs,
	_tile_type_misc_procs,
	_tile_type_station_procs,
	_tile_type_water_procs,
	_tile_type_industry_procs,
	_tile_type_object_procs;

/**
 * Tile callback functions for each type of tile.
 * @ingroup TileCallbackGroup
 * @see TileType
 */
extern const TileTypeProcs * const _tile_type_procs[16] = {
	&_tile_type_clear_procs,        ///< Callback functions for clear tiles
	&_tile_type_object_procs,       ///< Callback functions for object tiles
	&_tile_type_water_procs,        ///< Callback functions for water tiles
	NULL,
	&_tile_type_rail_procs,         ///< Callback functions for railway tiles
	&_tile_type_road_procs,         ///< Callback functions for road tiles
	&_tile_type_misc_procs,         ///< Callback functions for misc tiles
	&_tile_type_station_procs,      ///< Callback functions for station tiles
	&_tile_type_industry_procs,     ///< Callback functions for industry tiles
	&_tile_type_industry_procs,
	&_tile_type_industry_procs,
	&_tile_type_industry_procs,
	&_tile_type_town_procs,         ///< Callback functions for house tiles
	&_tile_type_town_procs,
	&_tile_type_town_procs,
	&_tile_type_town_procs,
};

/** landscape slope => sprite */
extern const byte _slope_to_sprite_offset[32] = {
	0, 1, 2, 3, 4, 5, 6,  7, 8, 9, 10, 11, 12, 13, 14, 0,
	0, 0, 0, 0, 0, 0, 0, 16, 0, 0,  0, 17,  0, 15, 18, 0,
};

/**
 * Description of the snow line throughout the year.
 *
 * If it is \c NULL, a static snowline height is used, as set by \c _settings_game.game_creation.snow_line_height.
 * Otherwise it points to a table loaded from a newGRF file that describes the variable snowline.
 * @ingroup SnowLineGroup
 * @see GetSnowLine() GameCreationSettings
 */
static SnowLine *_snow_line = NULL;

/**
 * Applies a foundation to a slope.
 *
 * @pre      Foundation and slope must be valid combined.
 * @param f  The #Foundation.
 * @param s  The #Slope to modify.
 * @return   Increment to the tile Z coordinate.
 */
uint ApplyFoundationToSlope(Foundation f, Slope *s)
{
	if (!IsFoundation(f)) return 0;

	if (IsLeveledFoundation(f)) {
		uint dz = 1 + (IsSteepSlope(*s) ? 1 : 0);
		*s = SLOPE_FLAT;
		return dz;
	}

	if (f != FOUNDATION_STEEP_BOTH && IsNonContinuousFoundation(f)) {
		*s = HalftileSlope(*s, GetHalftileFoundationCorner(f));
		return 0;
	}

	if (IsSpecialRailFoundation(f)) {
		*s = SlopeWithThreeCornersRaised(OppositeCorner(GetRailFoundationCorner(f)));
		return 0;
	}

	uint dz = IsSteepSlope(*s) ? 1 : 0;
	Corner highest_corner = GetHighestSlopeCorner(*s);

	switch (f) {
		case FOUNDATION_INCLINED_X:
			*s = (((highest_corner == CORNER_W) || (highest_corner == CORNER_S)) ? SLOPE_SW : SLOPE_NE);
			break;

		case FOUNDATION_INCLINED_Y:
			*s = (((highest_corner == CORNER_S) || (highest_corner == CORNER_E)) ? SLOPE_SE : SLOPE_NW);
			break;

		case FOUNDATION_STEEP_LOWER:
			*s = SlopeWithOneCornerRaised(highest_corner);
			break;

		case FOUNDATION_STEEP_BOTH:
			*s = HalftileSlope(SlopeWithOneCornerRaised(highest_corner), highest_corner);
			break;

		default: NOT_REACHED();
	}
	return dz;
}


/**
 * Determines height at given coordinate of a slope
 * @param x x coordinate
 * @param y y coordinate
 * @param corners slope to examine
 * @return height of given point of given slope
 */
uint GetPartialPixelZ(int x, int y, Slope corners)
{
	if (IsHalftileSlope(corners)) {
		bool on_halftile;
		switch (GetHalftileSlopeCorner(corners)) {
			case CORNER_W: on_halftile = (x >= y);         break;
			case CORNER_S: on_halftile = (x >= (y ^ 0xF)); break;
			case CORNER_E: on_halftile = (y >= x);         break;
			case CORNER_N: on_halftile = ((y ^ 0xF) >= x); break;
			default: NOT_REACHED();
		}
		if (on_halftile) return GetSlopeMaxPixelZ(corners);
	}

	int z = 0;

	switch (RemoveHalftileSlope(corners)) {
		case SLOPE_W:
			if (x - y >= 0) {
				z = (x - y) >> 1;
			}
			break;

		case SLOPE_S:
			y ^= 0xF;
			if ((x - y) >= 0) {
				z = (x - y) >> 1;
			}
			break;

		case SLOPE_SW:
			z = (x >> 1) + 1;
			break;

		case SLOPE_E:
			if (y - x >= 0) {
				z = (y - x) >> 1;
			}
			break;

		case SLOPE_EW:
		case SLOPE_NS:
		case SLOPE_ELEVATED:
			z = 4;
			break;

		case SLOPE_SE:
			z = (y >> 1) + 1;
			break;

		case SLOPE_WSE:
			z = 8;
			y ^= 0xF;
			if (x - y < 0) {
				z += (x - y) >> 1;
			}
			break;

		case SLOPE_N:
			y ^= 0xF;
			if (y - x >= 0) {
				z = (y - x) >> 1;
			}
			break;

		case SLOPE_NW:
			z = (y ^ 0xF) >> 1;
			break;

		case SLOPE_NWS:
			z = 8;
			if (x - y < 0) {
				z += (x - y) >> 1;
			}
			break;

		case SLOPE_NE:
			z = (x ^ 0xF) >> 1;
			break;

		case SLOPE_ENW:
			z = 8;
			y ^= 0xF;
			if (y - x < 0) {
				z += (y - x) >> 1;
			}
			break;

		case SLOPE_SEN:
			z = 8;
			if (y - x < 0) {
				z += (y - x) >> 1;
			}
			break;

		case SLOPE_STEEP_S:
			z = 1 + ((x + y) >> 1);
			break;

		case SLOPE_STEEP_W:
			z = 1 + ((x + (y ^ 0xF)) >> 1);
			break;

		case SLOPE_STEEP_N:
			z = 1 + (((x ^ 0xF) + (y ^ 0xF)) >> 1);
			break;

		case SLOPE_STEEP_E:
			z = 1 + (((x ^ 0xF) + y) >> 1);
			break;

		default: break;
	}

	return z;
}

int GetSlopePixelZ(int x, int y)
{
	TileIndex tile = TileVirtXY(x, y);

	return GetTileProcs(tile)->get_slope_z_proc(tile, x, y);
}

/**
 * Determine the Z height of a corner relative to TileZ.
 *
 * @pre The slope must not be a halftile slope.
 *
 * @param tileh The slope.
 * @param corner The corner.
 * @return Z position of corner relative to TileZ.
 */
int GetSlopeZInCorner(Slope tileh, Corner corner)
{
	assert(!IsHalftileSlope(tileh));
	return ((tileh & SlopeWithOneCornerRaised(corner)) != 0 ? 1 : 0) + (tileh == SteepSlope(corner) ? 1 : 0);
}

/**
 * Determine the Z height of the corners of a specific tile edge
 *
 * @note If a tile has a non-continuous halftile foundation, a corner can have different heights wrt. its edges.
 *
 * @pre z1 and z2 must be initialized (typ. with TileZ). The corner heights just get added.
 *
 * @param tileh The slope of the tile.
 * @param edge The edge of interest.
 * @param z1 Gets incremented by the height of the first corner of the edge. (near corner wrt. the camera)
 * @param z2 Gets incremented by the height of the second corner of the edge. (far corner wrt. the camera)
 */
void GetSlopePixelZOnEdge(Slope tileh, DiagDirection edge, int *z1, int *z2)
{
	static const Slope corners[4][4] = {
		/*    corner     |          steep slope
		 *  z1      z2   |       z1             z2        */
		{SLOPE_E, SLOPE_N, SLOPE_STEEP_E, SLOPE_STEEP_N}, // DIAGDIR_NE, z1 = E, z2 = N
		{SLOPE_S, SLOPE_E, SLOPE_STEEP_S, SLOPE_STEEP_E}, // DIAGDIR_SE, z1 = S, z2 = E
		{SLOPE_S, SLOPE_W, SLOPE_STEEP_S, SLOPE_STEEP_W}, // DIAGDIR_SW, z1 = S, z2 = W
		{SLOPE_W, SLOPE_N, SLOPE_STEEP_W, SLOPE_STEEP_N}, // DIAGDIR_NW, z1 = W, z2 = N
	};

	int halftile_test = (IsHalftileSlope(tileh) ? SlopeWithOneCornerRaised(GetHalftileSlopeCorner(tileh)) : 0);
	if (halftile_test == corners[edge][0]) *z2 += TILE_HEIGHT; // The slope is non-continuous in z2. z2 is on the upper side.
	if (halftile_test == corners[edge][1]) *z1 += TILE_HEIGHT; // The slope is non-continuous in z1. z1 is on the upper side.

	if ((tileh & corners[edge][0]) != 0) *z1 += TILE_HEIGHT; // z1 is raised
	if ((tileh & corners[edge][1]) != 0) *z2 += TILE_HEIGHT; // z2 is raised
	if (RemoveHalftileSlope(tileh) == corners[edge][2]) *z1 += TILE_HEIGHT; // z1 is highest corner of a steep slope
	if (RemoveHalftileSlope(tileh) == corners[edge][3]) *z2 += TILE_HEIGHT; // z2 is highest corner of a steep slope
}

/**
 * Get slope of a tile on top of a (possible) foundation
 * If a tile does not have a foundation, the function returns the same as GetTileSlope.
 *
 * @param tile The tile of interest.
 * @param z returns the z of the foundation slope. (Can be NULL, if not needed)
 * @return The slope on top of the foundation.
 */
Slope GetFoundationSlope(TileIndex tile, int *z)
{
	Slope tileh = GetTileSlope(tile, z);
	Foundation f = GetTileProcs(tile)->get_foundation_proc(tile, tileh);
	uint z_inc = ApplyFoundationToSlope(f, &tileh);
	if (z != NULL) *z += z_inc;
	return tileh;
}


bool HasFoundationNW(TileIndex tile, Slope slope_here, uint z_here)
{
	int z;

	int z_W_here = z_here;
	int z_N_here = z_here;
	GetSlopePixelZOnEdge(slope_here, DIAGDIR_NW, &z_W_here, &z_N_here);

	Slope slope = GetFoundationPixelSlope(TILE_ADDXY(tile, 0, -1), &z);
	int z_W = z;
	int z_N = z;
	GetSlopePixelZOnEdge(slope, DIAGDIR_SE, &z_W, &z_N);

	return (z_N_here > z_N) || (z_W_here > z_W);
}


bool HasFoundationNE(TileIndex tile, Slope slope_here, uint z_here)
{
	int z;

	int z_E_here = z_here;
	int z_N_here = z_here;
	GetSlopePixelZOnEdge(slope_here, DIAGDIR_NE, &z_E_here, &z_N_here);

	Slope slope = GetFoundationPixelSlope(TILE_ADDXY(tile, -1, 0), &z);
	int z_E = z;
	int z_N = z;
	GetSlopePixelZOnEdge(slope, DIAGDIR_SW, &z_E, &z_N);

	return (z_N_here > z_N) || (z_E_here > z_E);
}

/**
 * Draw foundation \a f at tile \a ti. Updates \a ti.
 * @param ti Tile to draw foundation on
 * @param f  Foundation to draw
 * @param side Side to skip
 */
void DrawFoundation(TileInfo *ti, Foundation f, DiagDirection side)
{
	if (!IsFoundation(f)) return;

	/* Two part foundations must be drawn separately */
	assert(f != FOUNDATION_STEEP_BOTH);

	uint sprite_block = 0;
	int z;
	Slope slope = GetFoundationPixelSlope(ti->tile, &z);

	/* Select the needed block of foundations sprites
	 * Block 0: Walls at NW and NE edge
	 * Block 1: Wall  at        NE edge
	 * Block 2: Wall  at NW        edge
	 * Block 3: No walls at NW or NE edge
	 */
	if (side == DIAGDIR_NW || !HasFoundationNW(ti->tile, slope, z)) sprite_block += 1;
	if (side == DIAGDIR_NE || !HasFoundationNE(ti->tile, slope, z)) sprite_block += 2;

	/* Use the original slope sprites if NW and NE borders should be visible */
	SpriteID leveled_base = (sprite_block == 0 ? (int)SPR_FOUNDATION_BASE : (SPR_SLOPES_VIRTUAL_BASE + sprite_block * SPR_TRKFOUND_BLOCK_SIZE));
	SpriteID inclined_base = SPR_SLOPES_VIRTUAL_BASE + SPR_SLOPES_INCLINED_OFFSET + sprite_block * SPR_TRKFOUND_BLOCK_SIZE;
	SpriteID halftile_base = SPR_HALFTILE_FOUNDATION_BASE + sprite_block * SPR_HALFTILE_BLOCK_SIZE;

	if (IsSteepSlope(ti->tileh)) {
		if (!IsNonContinuousFoundation(f)) {
			/* Lower part of foundation */
			AddSortableSpriteToDraw(
				leveled_base + (ti->tileh & ~SLOPE_STEEP), PAL_NONE, ti->x, ti->y, 16, 16, 7, ti->z
			);
		}

		Corner highest_corner = GetHighestSlopeCorner(ti->tileh);
		ti->z += ApplyPixelFoundationToSlope(f, &ti->tileh);

		if (IsInclinedFoundation(f)) {
			/* inclined foundation */
			byte inclined = highest_corner * 2 + (f == FOUNDATION_INCLINED_Y ? 1 : 0);

			AddSortableSpriteToDraw(inclined_base + inclined, PAL_NONE, ti->x, ti->y,
				f == FOUNDATION_INCLINED_X ? 16 : 1,
				f == FOUNDATION_INCLINED_Y ? 16 : 1,
				TILE_HEIGHT, ti->z
			);
			OffsetGroundSprite(31, 9);
		} else if (IsLeveledFoundation(f)) {
			AddSortableSpriteToDraw(leveled_base + SlopeWithOneCornerRaised(highest_corner), PAL_NONE, ti->x, ti->y, 16, 16, 7, ti->z - TILE_HEIGHT);
			OffsetGroundSprite(31, 1);
		} else if (f == FOUNDATION_STEEP_LOWER) {
			/* one corner raised */
			OffsetGroundSprite(31, 1);
		} else {
			/* halftile foundation */
			int x_bb = (((highest_corner == CORNER_W) || (highest_corner == CORNER_S)) ? 8 : 0);
			int y_bb = (((highest_corner == CORNER_S) || (highest_corner == CORNER_E)) ? 8 : 0);

			AddSortableSpriteToDraw(halftile_base + highest_corner, PAL_NONE, ti->x + x_bb, ti->y + y_bb, 8, 8, 7, ti->z + TILE_HEIGHT);
			OffsetGroundSprite(31, 9);
		}
	} else {
		if (IsLeveledFoundation(f)) {
			/* leveled foundation */
			AddSortableSpriteToDraw(leveled_base + ti->tileh, PAL_NONE, ti->x, ti->y, 16, 16, 7, ti->z);
			OffsetGroundSprite(31, 1);
		} else if (IsNonContinuousFoundation(f)) {
			/* halftile foundation */
			Corner halftile_corner = GetHalftileFoundationCorner(f);
			int x_bb = (((halftile_corner == CORNER_W) || (halftile_corner == CORNER_S)) ? 8 : 0);
			int y_bb = (((halftile_corner == CORNER_S) || (halftile_corner == CORNER_E)) ? 8 : 0);

			AddSortableSpriteToDraw(halftile_base + halftile_corner, PAL_NONE, ti->x + x_bb, ti->y + y_bb, 8, 8, 7, ti->z);
			OffsetGroundSprite(31, 9);
		} else if (IsSpecialRailFoundation(f)) {
			/* anti-zig-zag foundation */
			SpriteID spr;
			if (ti->tileh == SLOPE_NS || ti->tileh == SLOPE_EW) {
				/* half of leveled foundation under track corner */
				spr = leveled_base + SlopeWithThreeCornersRaised(GetRailFoundationCorner(f));
			} else {
				/* tile-slope = sloped along X/Y, foundation-slope = three corners raised */
				spr = inclined_base + 2 * GetRailFoundationCorner(f) + ((ti->tileh == SLOPE_SW || ti->tileh == SLOPE_NE) ? 1 : 0);
			}
			AddSortableSpriteToDraw(spr, PAL_NONE, ti->x, ti->y, 16, 16, 7, ti->z);
			OffsetGroundSprite(31, 9);
		} else {
			/* inclined foundation */
			byte inclined = GetHighestSlopeCorner(ti->tileh) * 2 + (f == FOUNDATION_INCLINED_Y ? 1 : 0);

			AddSortableSpriteToDraw(inclined_base + inclined, PAL_NONE, ti->x, ti->y,
				f == FOUNDATION_INCLINED_X ? 16 : 1,
				f == FOUNDATION_INCLINED_Y ? 16 : 1,
				TILE_HEIGHT, ti->z
			);
			OffsetGroundSprite(31, 9);
		}
		ti->z += ApplyPixelFoundationToSlope(f, &ti->tileh);
	}
}

void DoClearSquare(TileIndex tile)
{
	/* If the tile can have animation and we clear it, delete it from the animated tile list. */
	if (GetTileProcs(tile)->animate_tile_proc != NULL) DeleteAnimatedTile(tile);

	MakeClear(tile, GROUND_GRASS, _generating_world ? 3 : 0);
	MarkTileDirtyByTile(tile);
}

/**
 * Returns information about railway trackdirs and signal states.
 * If there is any trackbit at 'side', return all trackdirbits.
 * @param tile tile to get info about
 * @param side side we are entering from, INVALID_DIAGDIR to return all trackbits
 * @return trackdirbits and other info
 */
TrackStatus GetTileRailwayStatus(TileIndex tile, DiagDirection side)
{
	GetTileTrackStatusProc *proc = GetTileProcs(tile)->get_tile_railway_status_proc;
	return proc != NULL ? proc(tile, side) : 0;
}

/**
 * Returns information about road trackdirs and signal states.
 * If there is any trackbit at 'side', return all trackdirbits.
 * Return no trackbits if there is no roadbit (of given subtype) at given side.
 * @param tile tile to get info about
 * @param sub_mode roadtypes to check
 * @param side side we are entering from, INVALID_DIAGDIR to return all trackbits
 * @return trackdirbits and other info
 */
TrackStatus GetTileRoadStatus(TileIndex tile, uint sub_mode, DiagDirection side)
{
	GetTileRoadStatusProc *proc = GetTileProcs(tile)->get_tile_road_status_proc;
	return proc != NULL ? proc(tile, sub_mode, side) : 0;
}

/**
 * Returns information about waterway trackdirs.
 * If there is any trackbit at 'side', return all trackdirbits.
 * @param tile tile to get info about
 * @param side side we are entering from, INVALID_DIAGDIR to return all trackbits
 * @return trackdirbits
 */
TrackdirBits GetTileWaterwayStatus(TileIndex tile, DiagDirection side)
{
	GetTileWaterStatusProc *proc = GetTileProcs(tile)->get_tile_waterway_status_proc;
	return proc != NULL ? proc(tile, side) : TRACKDIR_BIT_NONE;
}

/**
 * Change the owner of a tile
 * @param tile      Tile to change
 * @param old_owner Current owner of the tile
 * @param new_owner New owner of the tile
 */
void ChangeTileOwner(TileIndex tile, Owner old_owner, Owner new_owner)
{
	GetTileProcs(tile)->change_tile_owner_proc(tile, old_owner, new_owner);
}

void GetTileDesc(TileIndex tile, TileDesc *td)
{
	GetTileProcs(tile)->get_tile_desc_proc(tile, td);
}

/**
 * Has a snow line table already been loaded.
 * @return true if the table has been loaded already.
 * @ingroup SnowLineGroup
 */
bool IsSnowLineSet()
{
	return _snow_line != NULL;
}

/**
 * Set a variable snow line, as loaded from a newgrf file.
 * @param table the 12 * 32 byte table containing the snowline for each day
 * @ingroup SnowLineGroup
 */
void SetSnowLine(byte table[SNOW_LINE_MONTHS][SNOW_LINE_DAYS])
{
	_snow_line = xcalloct<SnowLine>();
	_snow_line->lowest_value = 0xFF;
	memcpy(_snow_line->table, table, sizeof(_snow_line->table));

	for (uint i = 0; i < SNOW_LINE_MONTHS; i++) {
		for (uint j = 0; j < SNOW_LINE_DAYS; j++) {
			_snow_line->highest_value = max(_snow_line->highest_value, table[i][j]);
			_snow_line->lowest_value = min(_snow_line->lowest_value, table[i][j]);
		}
	}
}

/**
 * Get the current snow line, either variable or static.
 * @return the snow line height.
 * @ingroup SnowLineGroup
 */
byte GetSnowLine()
{
	if (_snow_line == NULL) return _settings_game.game_creation.snow_line_height;

	YearMonthDay ymd;
	ConvertDateToYMD(_date, &ymd);
	return _snow_line->table[ymd.month][ymd.day];
}

/**
 * Get the highest possible snow line height, either variable or static.
 * @return the highest snow line height.
 * @ingroup SnowLineGroup
 */
byte HighestSnowLine()
{
	return _snow_line == NULL ? _settings_game.game_creation.snow_line_height : _snow_line->highest_value;
}

/**
 * Get the lowest possible snow line height, either variable or static.
 * @return the lowest snow line height.
 * @ingroup SnowLineGroup
 */
byte LowestSnowLine()
{
	return _snow_line == NULL ? _settings_game.game_creation.snow_line_height : _snow_line->lowest_value;
}

/**
 * Clear the variable snow line table and free the memory.
 * @ingroup SnowLineGroup
 */
void ClearSnowLine()
{
	free(_snow_line);
	_snow_line = NULL;
}

/**
 * Clear a piece of landscape
 * @param tile tile to clear
 * @param flags of operation to conduct
 * @param p1 unused
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdLandscapeClear(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CommandCost cost(EXPENSES_CONSTRUCTION);
	bool do_clear = false;
	/* Test for stuff which results in water when cleared. Then add the cost to also clear the water. */
	if ((flags & DC_FORCE_CLEAR_TILE) && HasTileWaterClass(tile) && IsTileOnWater(tile) && !IsPlainWaterTile(tile) && !IsCoastTile(tile)) {
		if ((flags & DC_AUTO) && GetWaterClass(tile) == WATER_CLASS_CANAL) return_cmd_error(STR_ERROR_MUST_DEMOLISH_CANAL_FIRST);
		do_clear = true;
		cost.AddCost(GetWaterClass(tile) == WATER_CLASS_CANAL ? _price[PR_CLEAR_CANAL] : _price[PR_CLEAR_WATER]);
	}

	Company *c = (flags & (DC_AUTO | DC_BANKRUPT)) ? NULL : Company::GetIfValid(_current_company);
	if (c != NULL && (int)GB(c->clear_limit, 16, 16) < 1) {
		return_cmd_error(STR_ERROR_CLEARING_LIMIT_REACHED);
	}

	const ClearedObjectArea *coa = FindClearedObject(tile);

	/* If this tile was the first tile which caused object destruction, always
	 * pass it on to the tile_type_proc. That way multiple test runs and the exec run stay consistent. */
	if (coa != NULL && coa->first_tile != tile) {
		/* If this tile belongs to an object which was already cleared via another tile, pretend it has been
		 * already removed.
		 * However, we need to check stuff, which is not the same for all object tiles. (e.g. being on water or not) */

		/* If a object is removed, it leaves either bare land or water. */
		if ((flags & DC_NO_WATER) && HasTileWaterClass(tile) && IsTileOnWater(tile)) {
			return_cmd_error(STR_ERROR_CAN_T_BUILD_ON_WATER);
		}
	} else {
		cost.AddCost(GetTileProcs(tile)->clear_tile_proc(tile, flags));
	}

	if (flags & DC_EXEC) {
		if (c != NULL) c->clear_limit -= 1 << 16;
		if (do_clear) DoClearSquare(tile);
	}
	return cost;
}

/**
 * Clear a big piece of landscape
 * @param tile end tile of area dragging
 * @param flags of operation to conduct
 * @param p1 start tile of area dragging
 * @param p2 various bitstuffed data.
 *  bit      0: Whether to use the Orthogonal (0) or Diagonal (1) iterator.
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdClearArea(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (p1 >= MapSize()) return CMD_ERROR;

	Money money = GetAvailableMoneyForCommand();
	CommandCost cost(EXPENSES_CONSTRUCTION);
	CommandCost last_error = CMD_ERROR;
	bool had_success = false;

	const Company *c = (flags & (DC_AUTO | DC_BANKRUPT)) ? NULL : Company::GetIfValid(_current_company);
	int limit = (c == NULL ? INT32_MAX : GB(c->clear_limit, 16, 16));

	TileArea ta(tile, p1);
	TileIterator *iter = HasBit(p2, 0) ? (TileIterator *)new DiagonalTileIterator(tile, p1) : new OrthogonalTileIterator(ta);
	for (; *iter != INVALID_TILE; ++(*iter)) {
		TileIndex t = *iter;
		CommandCost ret = DoCommand(t, 0, 0, flags & ~DC_EXEC, CMD_LANDSCAPE_CLEAR);
		if (ret.Failed()) {
			last_error = ret;

			/* We may not clear more tiles. */
			if (c != NULL && GB(c->clear_limit, 16, 16) < 1) break;
			continue;
		}

		had_success = true;
		if (flags & DC_EXEC) {
			money -= ret.GetCost();
			if (ret.GetCost() > 0 && money < 0) {
				_additional_cash_required = ret.GetCost();
				delete iter;
				return cost;
			}
			DoCommand(t, 0, 0, flags, CMD_LANDSCAPE_CLEAR);

			/* draw explosion animation...
			 * Disable explosions when game is paused. Looks silly and blocks the view. */
			TileIndex off = t - ta.tile;
			if ((TileX(off) == 0 || TileX(off) == ta.w - 1U) && (TileY(off) == 0 || TileY(off) == ta.h - 1U) && _pause_mode == PM_UNPAUSED) {
				/* big explosion in each corner, or small explosion for single tiles */
				CreateEffectVehicleAbove(TileX(t) * TILE_SIZE + TILE_SIZE / 2, TileY(t) * TILE_SIZE + TILE_SIZE / 2, 2,
					ta.w == 1 && ta.h == 1 ? EV_EXPLOSION_SMALL : EV_EXPLOSION_LARGE
				);
			}
		} else {
			/* When we're at the clearing limit we better bail (unneed) testing as well. */
			if (ret.GetCost() != 0 && --limit <= 0) break;
		}
		cost.AddCost(ret);
	}

	delete iter;
	return had_success ? cost : last_error;
}


TileIndex _cur_tileloop_tile;

/**
 * Gradually iterate over all tiles on the map, calling their TileLoopProcs once every 256 ticks.
 */
void RunTileLoop()
{
	/* The pseudorandom sequence of tiles is generated using a Galois linear feedback
	 * shift register (LFSR). This allows a deterministic pseudorandom ordering, but
	 * still with minimal state and fast iteration. */

	/* Maximal length LFSR feedback terms, from 12-bit (for 64x64 maps) to 24-bit (for 4096x4096 maps).
	 * Extracted from http://www.ece.cmu.edu/~koopman/lfsr/ */
	static const uint32 feedbacks[] = {
		0xD8F, 0x1296, 0x2496, 0x4357, 0x8679, 0x1030E, 0x206CD, 0x403FE, 0x807B8, 0x1004B2, 0x2006A8, 0x4004B2, 0x800B87
	};
	assert_compile(lengthof(feedbacks) == 2 * MAX_MAP_SIZE_BITS - 2 * MIN_MAP_SIZE_BITS + 1);
	const uint32 feedback = feedbacks[MapLogX() + MapLogY() - 2 * MIN_MAP_SIZE_BITS];

	/* We update every tile every 256 ticks, so divide the map size by 2^8 = 256 */
	uint count = 1 << (MapLogX() + MapLogY() - 8);

	TileIndex tile = _cur_tileloop_tile;
	/* The LFSR cannot have a zeroed state. */
	assert(tile != 0);

	/* Manually update tile 0 every 256 ticks - the LFSR never iterates over it itself.  */
	if (_tick_counter % 256 == 0) {
		GetTileProcs(0)->tile_loop_proc(0);
		count--;
	}

	while (count--) {
		GetTileProcs(tile)->tile_loop_proc(tile);

		/* Get the next tile in sequence using a Galois LFSR. */
		tile = (tile >> 1) ^ (-(int32)(tile & 1) & feedback);
	}

	_cur_tileloop_tile = tile;
}

void InitializeLandscape()
{
	uint maxx = MapMaxX();
	uint maxy = MapMaxY();
	uint sizex = MapSizeX();

	uint y;
	for (y = _settings_game.construction.freeform_edges ? 1 : 0; y < maxy; y++) {
		uint x;
		for (x = _settings_game.construction.freeform_edges ? 1 : 0; x < maxx; x++) {
			MakeClear(sizex * y + x, GROUND_GRASS, 3);
			SetTileHeight(sizex * y + x, 0);
			SetTropicZone(sizex * y + x, TROPICZONE_NORMAL);
			ClearBridgeMiddle(sizex * y + x);
		}
		MakeVoid(sizex * y + x);
	}
	for (uint x = 0; x < sizex; x++) MakeVoid(sizex * y + x);
}

static const byte _genterrain_tbl_1[5] = { 10, 22, 33, 37, 4  };
static const byte _genterrain_tbl_2[5] = {  0,  0,  0,  0, 33 };

static void GenerateTerrain(int type, uint flag)
{
	uint32 r = Random();

	const MapGenSprite *templ = GetMapGenSprite ((((r >> 24) * _genterrain_tbl_1[type]) >> 8) + _genterrain_tbl_2[type] + 4845);
	if (templ == NULL) usererror("Map generator sprites could not be loaded");

	uint x = r & MapMaxX();
	uint y = (r >> MapLogX()) & MapMaxY();

	if (x < 2 || y < 2) return;

	DiagDirection direction = (DiagDirection)GB(r, 22, 2);
	uint w = templ->width;
	uint h = templ->height;

	if (DiagDirToAxis(direction) == AXIS_Y) Swap(w, h);

	const byte *p = templ->data;

	if ((flag & 4) != 0) {
		uint xw = x * MapSizeY();
		uint yw = y * MapSizeX();
		uint bias = (MapSizeX() + MapSizeY()) * 16;

		switch (flag & 3) {
			default: NOT_REACHED();
			case 0:
				if (xw + yw > MapSize() - bias) return;
				break;

			case 1:
				if (yw < xw + bias) return;
				break;

			case 2:
				if (xw + yw < MapSize() + bias) return;
				break;

			case 3:
				if (xw < yw + bias) return;
				break;
		}
	}

	if (x + w >= MapMaxX() - 1) return;
	if (y + h >= MapMaxY() - 1) return;

	TileIndex tile = TileXY(x, y);

	switch (direction) {
		default: NOT_REACHED();
		case DIAGDIR_NE:
			do {
				TileIndex tile_cur = tile;

				for (uint w_cur = w; w_cur != 0; --w_cur) {
					if (GB(*p, 0, 4) >= TileHeight(tile_cur)) SetTileHeight(tile_cur, GB(*p, 0, 4));
					p++;
					tile_cur++;
				}
				tile += TileDiffXY(0, 1);
			} while (--h != 0);
			break;

		case DIAGDIR_SE:
			do {
				TileIndex tile_cur = tile;

				for (uint h_cur = h; h_cur != 0; --h_cur) {
					if (GB(*p, 0, 4) >= TileHeight(tile_cur)) SetTileHeight(tile_cur, GB(*p, 0, 4));
					p++;
					tile_cur += TileDiffXY(0, 1);
				}
				tile += TileDiffXY(1, 0);
			} while (--w != 0);
			break;

		case DIAGDIR_SW:
			tile += TileDiffXY(w - 1, 0);
			do {
				TileIndex tile_cur = tile;

				for (uint w_cur = w; w_cur != 0; --w_cur) {
					if (GB(*p, 0, 4) >= TileHeight(tile_cur)) SetTileHeight(tile_cur, GB(*p, 0, 4));
					p++;
					tile_cur--;
				}
				tile += TileDiffXY(0, 1);
			} while (--h != 0);
			break;

		case DIAGDIR_NW:
			tile += TileDiffXY(0, h - 1);
			do {
				TileIndex tile_cur = tile;

				for (uint h_cur = h; h_cur != 0; --h_cur) {
					if (GB(*p, 0, 4) >= TileHeight(tile_cur)) SetTileHeight(tile_cur, GB(*p, 0, 4));
					p++;
					tile_cur -= TileDiffXY(0, 1);
				}
				tile += TileDiffXY(1, 0);
			} while (--w != 0);
			break;
	}
}


#include "table/genland.h"

static void CreateDesertOrRainForest()
{
	TileIndex update_freq = MapSize() / 4;
	const CoordDiff *data;
	uint max_desert_height = CeilDiv(_settings_game.construction.max_heightlevel, 4);

	for (TileIndex tile = 0; tile != MapSize(); ++tile) {
		if ((tile % update_freq) == 0) IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

		if (!IsValidTile(tile)) continue;

		for (data = _make_desert_or_rainforest_data;
				data != endof(_make_desert_or_rainforest_data); ++data) {
			TileIndex t = AddCoordDiffWrap(tile, *data);
			if (t != INVALID_TILE && (TileHeight(t) >= max_desert_height || IsWaterTile(t))) break;
		}
		if (data == endof(_make_desert_or_rainforest_data)) {
			SetTropicZone(tile, TROPICZONE_DESERT);
		}
	}

	for (uint i = 0; i != 256; i++) {
		if ((i % 64) == 0) IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

		RunTileLoop();
	}

	for (TileIndex tile = 0; tile != MapSize(); ++tile) {
		if ((tile % update_freq) == 0) IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

		if (!IsValidTile(tile)) continue;

		for (data = _make_desert_or_rainforest_data;
				data != endof(_make_desert_or_rainforest_data); ++data) {
			TileIndex t = AddCoordDiffWrap(tile, *data);
			if (t != INVALID_TILE && IsClearTile(t) && IsClearGround(t, GROUND_DESERT)) break;
		}
		if (data == endof(_make_desert_or_rainforest_data)) {
			SetTropicZone(tile, TROPICZONE_RAINFOREST);
		}
	}
}

/**
 * Find the spring of a river.
 * @param tile The tile to consider for being the spring.
 * @return True iff it is suitable as a spring.
 */
static bool FindSpring (TileIndex tile)
{
	int referenceHeight;
	if (!IsTileFlat(tile, &referenceHeight) || IsPlainWaterTile(tile)) return false;

	/* In the tropics rivers start in the rainforest. */
	if (_settings_game.game_creation.landscape == LT_TROPIC && GetTropicZone(tile) != TROPICZONE_RAINFOREST) return false;

	/* Are there enough higher tiles to warrant a 'spring'? */
	TileArea ta (tile);
	ta.expand (1); // check adjacent tiles (3x3 square)
	uint num = 0;
	TILE_AREA_LOOP(t, ta) {
		if (GetTileMaxZ(t) > referenceHeight) num++;
	}

	if (num < 4) return false;

	/* Are we near the top of a hill? */
	ta.expand (15); // check nearby tiles (radius 16)
	TILE_AREA_LOOP(t, ta) {
		if (GetTileMaxZ(t) > referenceHeight + 2) return false;
	}

	return true;
}

/**
 * Make a connected lake; fill all tiles in the circular tile search that are connected.
 * @param tile The tile to consider for lake making.
 * @param height The height of the lake.
 */
static void MakeLake (TileIndex tile, uint height)
{
	if (!IsValidTile(tile) || TileHeight(tile) != height || !IsTileFlat(tile)) return;
	if (_settings_game.game_creation.landscape == LT_TROPIC && GetTropicZone(tile) == TROPICZONE_DESERT) return;

	for (DiagDirection d = DIAGDIR_BEGIN; d < DIAGDIR_END; d++) {
		TileIndex t2 = tile + TileOffsByDiagDir(d);
		if (IsPlainWaterTile(t2)) {
			MakeRiver(tile, Random());
			return;
		}
	}
}

/** River node struct for Astar. */
struct RiverNode : AstarNode <RiverNode> {
	typedef AstarNode <RiverNode> Base;
	typedef RiverNode Key; // we are our own key

	TileIndex tile;
	Slope slope;

	RiverNode (const RiverNode *parent, TileIndex t, Slope s)
		: Base (parent), tile (t), slope (s)
	{
	}

	bool operator == (const RiverNode &other) const
	{
		return tile == other.tile;
	}

	const Key& GetKey() const
	{
		return *this;
	}

	int CalcHash() const
	{
		return TileHash (TileX(tile), TileY(tile));
	}
};

/** River pathfinder. */
struct RiverAstar : Astar <RiverNode, 8, 8>
{
	const TileIndex target;

	RiverAstar (TileIndex target) : target(target) { }
};

/**
 * Check if a given slope is valid for a flow when moving in a given direction.
 * @param dir The direction in which we are moving.
 * @param slope The slope of the new tile.
 * @return Whether the slope is valid for flowing in the given direction.
 */
static bool FlowsDown (DiagDirection dir, Slope slope)
{
	return (slope == SLOPE_FLAT) || (slope == ComplementSlope (InclinedSlope (dir)));
}

/** River neighbour finder for the A-star algorithm in a given direction. */
static void RiverFollowDir (RiverAstar *a, const RiverNode *n, DiagDirection d)
{
	TileIndex tile = TileAddByDiagDir (n->tile, d);
	if (!IsValidTile (tile)) return;

	Slope slope = GetTileSlope (tile);
	if (FlowsDown (d, slope)) {
		RiverNode m (n, tile, slope);
		m.m_cost = n->m_cost + 1 + RandomRange (_settings_game.game_creation.river_route_random);
		if (tile == a->target) {
			m.m_estimate = m.m_cost;
			a->InsertTarget(m);
		} else {
			m.m_estimate = m.m_cost + DistanceManhattan (tile, a->target);
			a->InsertNode(m);
		}
	}
}

/**
 * River neighbour finder for the A-star algorithm
 */
static void RiverFollow (RiverAstar *a, const RiverNode *n)
{
	if (n->slope == SLOPE_FLAT) {
		/* We can flow in all four direction from a flat tile. */
		for (DiagDirection d = DIAGDIR_BEGIN; d < DIAGDIR_END; d++) {
			RiverFollowDir (a, n, d);
		}
	} else {
		/* We can only flow downhill from a sloped tile. */
		DiagDirection d = GetInclinedSlopeDirection (n->slope);
		assert (d != INVALID_DIAGDIR);
		RiverFollowDir (a, n, ReverseDiagDir(d));
	}
}

/**
 * Actually build the river between the begin and end tiles using AyStar.
 * @param begin The begin of the river.
 * @param end The end of the river.
 */
static void BuildRiver(TileIndex begin, TileIndex end)
{
	assert (IsTileFlat (begin));

	RiverAstar finder (end);
	finder.InsertInitialNode (RiverNode (NULL, begin, SLOPE_FLAT));

	if (finder.FindPath(&RiverFollow)) {
		for (const RiverNode *n = finder.best; n != NULL; n = n->m_parent) {
			TileIndex tile = n->tile;
			if (!IsPlainWaterTile(tile)) {
				MakeRiver(tile, Random());
				/* Remove desert directly around the river tile. */
				RiverModifyDesertZone (tile);
			}
		}
	}
}

/**
 * Try to flow the river down from a given begin.
 * @param spring The springing point of the river.
 * @param begin  The begin point we are looking from; somewhere down hill from the spring.
 * @return True iff a river could/has been built, otherwise false.
 */
static bool FlowRiver(TileIndex spring, TileIndex begin)
{
	assert (IsValidTile(begin));
	assert (IsTileFlat (begin));

	uint height = TileHeight(begin);
	if (IsPlainWaterTile(begin)) return DistanceManhattan(spring, begin) > _settings_game.game_creation.min_river_length;

	std::set<TileIndex> marks;
	marks.insert(begin);

	/* Breadth first search for the closest tile we can flow down to. */
	std::queue <std::pair <TileIndex, Slope> > queue;

	TileIndex end = begin;
	Slope slope = SLOPE_FLAT;
	bool found = false;

	for (;;) {
		DiagDirection d0, d1;
		if (slope == SLOPE_FLAT) {
			uint height2 = TileHeight (end);
			assert (height2 <= height);
			if (height2 < height || IsPlainWaterTile(end)) {
				found = true;
				break;
			}
			/* try all neighbour tiles from a flat tile */
			d0 = DIAGDIR_BEGIN;
			d1 = DIAGDIR_END;
		} else {
			DiagDirection d = GetInclinedSlopeDirection (slope);
			assert (d != INVALID_DIAGDIR);
			/* only try this direction from a sloped tile */
			d0 = ReverseDiagDir (d);
			d1 = (DiagDirection)(d0 + 1);
		}

		for (DiagDirection d = d0; d < d1; d++) {
			TileIndex neighbour = end + TileOffsByDiagDir (d);
			if (IsValidTile(neighbour) && (marks.find(neighbour) == marks.end())) {
				Slope neighbour_slope = GetTileSlope (neighbour);
				if (FlowsDown (d, neighbour_slope)) {
					marks.insert (neighbour);
					queue.push (std::make_pair (neighbour, neighbour_slope));
				}
			}
		}

		if (queue.empty()) break;

		end = queue.front().first;
		slope = queue.front().second;
		queue.pop();
	}

	if (found) {
		/* Flow further down hill. */
		found = FlowRiver(spring, end);
	} else if (marks.size() > 32) {
		/* Maybe we can make a lake. Find the Nth of the considered tiles. */
		std::set<TileIndex>::const_iterator cit = marks.begin();
		for (uint i = RandomRange (marks.size()); i > 0; i--) cit++;
		TileIndex lakeCenter = *cit;

		if (IsValidTile(lakeCenter) &&
				/* A river, or lake, can only be built on flat slopes. */
				IsTileFlat(lakeCenter) &&
				/* We want the lake to be built at the height of the river. */
				TileHeight(begin) == TileHeight(lakeCenter) &&
				/* We don't want the lake at the entry of the valley. */
				lakeCenter != begin &&
				/* We don't want lakes in the desert. */
				(_settings_game.game_creation.landscape != LT_TROPIC || GetTropicZone(lakeCenter) != TROPICZONE_DESERT) &&
				/* We only want a lake if the river is long enough. */
				DistanceManhattan(spring, lakeCenter) > _settings_game.game_creation.min_river_length) {
			end = lakeCenter;
			MakeRiver(lakeCenter, Random());
			uint range = RandomRange(8) + 3;
			CircularTileIterator iter1 (lakeCenter, range);
			for (lakeCenter = iter1; lakeCenter != INVALID_TILE; lakeCenter = ++iter1) {
				MakeLake (lakeCenter, height);
			}
			/* Call the search a second time so artefacts from going circular in one direction get (mostly) hidden. */
			lakeCenter = end;
			CircularTileIterator iter2 (lakeCenter, range);
			for (lakeCenter = iter2; lakeCenter != INVALID_TILE; lakeCenter = ++iter2) {
				MakeLake (lakeCenter, height);
			}
			found = true;
		}
	}

	marks.clear();
	if (found) BuildRiver(begin, end);
	return found;
}

/**
 * Actually (try to) create some rivers.
 */
static void CreateRivers()
{
	int amount = _settings_game.game_creation.amount_of_rivers;
	if (amount == 0) return;

	uint wells = ScaleByMapSize(4 << _settings_game.game_creation.amount_of_rivers);
	SetGeneratingWorldProgress(GWP_RIVER, wells + 256 / 64); // Include the tile loop calls below.

	for (; wells != 0; wells--) {
		IncreaseGeneratingWorldProgress(GWP_RIVER);
		for (int tries = 0; tries < 128; tries++) {
			TileIndex t = RandomTile();
			CircularTileIterator iter (t, 8);
			for (t = iter; t != INVALID_TILE; t = ++iter) {
				if (IsValidTile(t) && FindSpring (t)) break;
			}
			if (t == INVALID_TILE) continue;
			if (FlowRiver(t, t)) break;
		}
	}

	/* Run tile loop to update the ground density. */
	for (uint i = 0; i != 256; i++) {
		if (i % 64 == 0) IncreaseGeneratingWorldProgress(GWP_RIVER);
		RunTileLoop();
	}
}

void GenerateLandscape(byte mode)
{
	/** Number of steps of landscape generation */
	enum GenLandscapeSteps {
		GLS_HEIGHTMAP    =  3, ///< Loading a heightmap
		GLS_TERRAGENESIS =  5, ///< Terragenesis generator
		GLS_ORIGINAL     =  2, ///< Original generator
		GLS_TROPIC       = 12, ///< Extra steps needed for tropic landscape
		GLS_OTHER        =  0, ///< Extra steps for other landscapes
	};
	uint steps = (_settings_game.game_creation.landscape == LT_TROPIC) ? GLS_TROPIC : GLS_OTHER;

	if (mode == GWM_HEIGHTMAP) {
		SetGeneratingWorldProgress(GWP_LANDSCAPE, steps + GLS_HEIGHTMAP);
		LoadHeightmap(_file_to_saveload.name);
		IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);
	} else if (_settings_game.game_creation.land_generator == LG_TERRAGENESIS) {
		SetGeneratingWorldProgress(GWP_LANDSCAPE, steps + GLS_TERRAGENESIS);
		GenerateTerrainPerlin();
	} else {
		SetGeneratingWorldProgress(GWP_LANDSCAPE, steps + GLS_ORIGINAL);
		if (_settings_game.construction.freeform_edges) {
			for (uint x = 0; x < MapSizeX(); x++) MakeVoid(TileXY(x, 0));
			for (uint y = 0; y < MapSizeY(); y++) MakeVoid(TileXY(0, y));
		}
		switch (_settings_game.game_creation.landscape) {
			case LT_ARCTIC: {
				uint32 r = Random();

				for (uint i = ScaleByMapSize(GB(r, 0, 7) + 950); i != 0; --i) {
					GenerateTerrain(2, 0);
				}

				uint flag = GB(r, 7, 2) | 4;
				for (uint i = ScaleByMapSize(GB(r, 9, 7) + 450); i != 0; --i) {
					GenerateTerrain(4, flag);
				}
				break;
			}

			case LT_TROPIC: {
				uint32 r = Random();

				for (uint i = ScaleByMapSize(GB(r, 0, 7) + 170); i != 0; --i) {
					GenerateTerrain(0, 0);
				}

				uint flag = GB(r, 7, 2) | 4;
				for (uint i = ScaleByMapSize(GB(r, 9, 8) + 1700); i != 0; --i) {
					GenerateTerrain(0, flag);
				}

				flag ^= 2;

				for (uint i = ScaleByMapSize(GB(r, 17, 7) + 410); i != 0; --i) {
					GenerateTerrain(3, flag);
				}
				break;
			}

			default: {
				uint32 r = Random();

				assert(_settings_game.difficulty.quantity_sea_lakes != CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY);
				uint i = ScaleByMapSize(GB(r, 0, 7) + (3 - _settings_game.difficulty.quantity_sea_lakes) * 256 + 100);
				for (; i != 0; --i) {
					/* Make sure we do not overflow. */
					GenerateTerrain(Clamp(_settings_game.difficulty.terrain_type, 0, 3), 0);
				}
				break;
			}
		}
	}

	/* Do not call IncreaseGeneratingWorldProgress() before FixSlopes(),
	 * it allows screen redraw. Drawing of broken slopes crashes the game */
	FixSlopes();
	IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);
	ConvertGroundTilesIntoWaterTiles();
	IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	if (_settings_game.game_creation.landscape == LT_TROPIC) CreateDesertOrRainForest();

	CreateRivers();
}

void OnTick_Town();
void OnTick_Trees();
void OnTick_Station();
void OnTick_Industry();

void OnTick_Companies();
void OnTick_LinkGraph();

void CallLandscapeTick()
{
	OnTick_Town();
	OnTick_Trees();
	OnTick_Station();
	OnTick_Industry();

	OnTick_Companies();
	OnTick_LinkGraph();
}
