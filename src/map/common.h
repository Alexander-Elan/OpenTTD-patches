/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file map/common.h Map tile accessors common to several tile types. */

#ifndef MAP_COMMON_H
#define MAP_COMMON_H

#include "../stdafx.h"
#include "../tile/common.h"
#include "../tile/misc.h"
#include "map.h"
#include "coord.h"
#include "class.h"
#include "../company_type.h"
#include "../direction_type.h"

/**
 * Returns the owner of a tile
 *
 * This function returns the owner of a tile. This cannot used
 * for tiles whose type is one of void, house or industry,
 * as no company owned any of these buildings.
 *
 * @param tile The tile to check
 * @return The owner of the tile
 * @pre IsValidTile(tile)
 * @pre The tile must not be a house, an industry or void
 */
static inline Owner GetTileOwner(TileIndex tile)
{
	assert(IsValidTile(tile));
	return tile_get_owner(&_mc[tile]);
}

/**
 * Sets the owner of a tile
 *
 * This function sets the owner status of a tile. Note that you cannot
 * set a owner for tiles of type house, void or industry.
 *
 * @param tile The tile to change the owner status.
 * @param owner The new owner.
 * @pre IsValidTile(tile)
 * @pre The tile must not be a house, an industry or void
 */
static inline void SetTileOwner(TileIndex tile, Owner owner)
{
	assert(IsValidTile(tile));
	tile_set_owner(&_mc[tile], owner);
}

/**
 * Checks if a tile belongs to the given owner
 *
 * @param tile The tile to check
 * @param owner The owner to check against
 * @return True if a tile belongs the the given owner
 */
static inline bool IsTileOwner(TileIndex tile, Owner owner)
{
	return tile_is_owner(&_mc[tile], owner);
}


/** Check if a tile has snow/desert. */
#define IsOnDesert IsOnSnow
/**
 * Check if a tile has snow/desert.
 * @param t The tile to query.
 * @return True if the tile has snow/desert.
 */
static inline bool IsOnSnow(TileIndex t)
{
	assert((IsRailwayTile(t) && !IsTileSubtype(t, TT_TRACK)) ||
		IsRoadTile(t) || IsTileType(t, TT_MISC));
	return HasBit(_mc[t].m3, 4);
}

/** Set whether a tile has snow/desert. */
#define SetDesert SetSnow
/**
 * Set whether a tile has snow/desert.
 * @param t The tile to set.
 * @param set Whether to set snow/desert.
 */
static inline void SetSnow(TileIndex t, bool set)
{
	assert((IsRailwayTile(t) && !IsTileSubtype(t, TT_TRACK)) ||
		IsRoadTile(t) || IsTileType(t, TT_MISC));
	if (set) {
		SetBit(_mc[t].m3, 4);
	} else {
		ClrBit(_mc[t].m3, 4);
	}
}

/** Toggle the snow/desert state of a tile. */
#define ToggleDesert ToggleSnow
/**
 * Toggle the snow/desert state of a tile.
 * @param t The tile to change.
 */
static inline void ToggleSnow(TileIndex t)
{
	assert((IsRailwayTile(t) && !IsTileSubtype(t, TT_TRACK)) ||
		IsRoadTile(t) || IsTileType(t, TT_MISC));
	ToggleBit(_mc[t].m3, 4);
}


/**
 * Get the direction pointing to the other end.
 *
 * Tunnel: Get the direction facing into the tunnel
 * Bridge: Get the direction pointing onto the bridge
 * @param t The tile to analyze
 * @pre IsTunnelTile(t) || IsBridgeHeadTile(t)
 * @return the above mentioned direction
 */
static inline DiagDirection GetTunnelBridgeDirection(TileIndex t)
{
	return tile_get_tunnelbridge_direction(&_mc[t]);
}


/**
 * Get the current animation frame
 * @param t the tile
 * @pre IsHouseTile(t) || IsObjectTile(t) || IsIndustryTile(t) || IsStationTile(t)
 * @return frame number
 */
static inline byte GetAnimationFrame(TileIndex t)
{
	return tile_get_frame(&_mc[t]);
}

/**
 * Set a new animation frame
 * @param t the tile
 * @param frame the new frame number
 * @pre IsHouseTile(t) || IsObjectTile(t) || IsIndustryTile(t) || IsStationTile(t)
 */
static inline void SetAnimationFrame(TileIndex t, byte frame)
{
	tile_set_frame(&_mc[t], frame);
}

#endif /* MAP_COMMON_H */
