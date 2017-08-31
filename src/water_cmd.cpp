/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file water_cmd.cpp Handling of water tiles. */

#include "stdafx.h"
#include "cmd_helper.h"
#include "landscape.h"
#include "viewport_func.h"
#include "command_func.h"
#include "town.h"
#include "news_func.h"
#include "depot_base.h"
#include "depot_func.h"
#include "water.h"
#include "map/industry.h"
#include "newgrf_canal.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "company_func.h"
#include "map/ground.h"
#include "map/slope.h"
#include "aircraft.h"
#include "effectvehicle_func.h"
#include "map/bridge.h"
#include "bridge.h"
#include "station_base.h"
#include "ai/ai.hpp"
#include "game/game.hpp"
#include "core/random_func.hpp"
#include "core/backup_type.hpp"
#include "date_func.h"
#include "company_base.h"
#include "company_gui.h"
#include "newgrf_generic.h"
#include "signalbuffer.h"

#include "table/strings.h"

/**
 * Describes from which directions a specific slope can be flooded (if the tile is floodable at all).
 */
static const uint8 _flood_from_dirs[] = {
	(1 << DIR_NW) | (1 << DIR_SW) | (1 << DIR_SE) | (1 << DIR_NE), // SLOPE_FLAT
	(1 << DIR_NE) | (1 << DIR_SE),                                 // SLOPE_W
	(1 << DIR_NW) | (1 << DIR_NE),                                 // SLOPE_S
	(1 << DIR_NE),                                                 // SLOPE_SW
	(1 << DIR_NW) | (1 << DIR_SW),                                 // SLOPE_E
	0,                                                             // SLOPE_EW
	(1 << DIR_NW),                                                 // SLOPE_SE
	(1 << DIR_N ) | (1 << DIR_NW) | (1 << DIR_NE),                 // SLOPE_WSE, SLOPE_STEEP_S
	(1 << DIR_SW) | (1 << DIR_SE),                                 // SLOPE_N
	(1 << DIR_SE),                                                 // SLOPE_NW
	0,                                                             // SLOPE_NS
	(1 << DIR_E ) | (1 << DIR_NE) | (1 << DIR_SE),                 // SLOPE_NWS, SLOPE_STEEP_W
	(1 << DIR_SW),                                                 // SLOPE_NE
	(1 << DIR_S ) | (1 << DIR_SW) | (1 << DIR_SE),                 // SLOPE_ENW, SLOPE_STEEP_N
	(1 << DIR_W ) | (1 << DIR_SW) | (1 << DIR_NW),                 // SLOPE_SEN, SLOPE_STEEP_E
};

/**
 * Marks the tiles around a tile as dirty, if they are canals or rivers.
 *
 * @param tile The center of the tile where all other tiles are marked as dirty
 * @ingroup dirty
 */
static void MarkCanalsAndRiversAroundDirty(TileIndex tile)
{
	for (Direction dir = DIR_BEGIN; dir < DIR_END; dir++) {
		TileIndex neighbour = tile + TileOffsByDir (dir);
		if (IsWaterTile (neighbour)
				&& (IsCanal(neighbour) || IsRiver(neighbour))) {
			MarkTileDirtyByTile (neighbour);
		}
	}
}


/**
 * Build a ship depot.
 * @param tile tile where ship depot is built
 * @param flags type of operation
 * @param p1 bit 0 depot orientation (Axis)
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildShipDepot(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Axis axis = Extract<Axis, 0, 1>(p1);

	TileIndex tile2 = tile + (axis == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));

	if (!HasTileWaterGround(tile) || !HasTileWaterGround(tile2)) {
		return_cmd_error(STR_ERROR_MUST_BE_BUILT_ON_WATER);
	}

	if (HasBridgeAbove(tile) || HasBridgeAbove(tile2)) return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);

	if (!IsTileFlat(tile) || !IsTileFlat(tile2)) {
		/* Prevent depots on rapids */
		return_cmd_error(STR_ERROR_SITE_UNSUITABLE);
	}

	if (!Depot::CanAllocateItem()) return CMD_ERROR;

	WaterClass wc1 = GetWaterClass(tile);
	WaterClass wc2 = GetWaterClass(tile2);
	CommandCost cost = CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_DEPOT_SHIP]);

	bool add_cost = !IsPlainWaterTile(tile);
	CommandCost ret = DoCommand(tile, 0, 0, flags | DC_AUTO, CMD_LANDSCAPE_CLEAR);
	if (ret.Failed()) return ret;
	if (add_cost) {
		cost.AddCost(ret);
	}
	add_cost = !IsPlainWaterTile(tile2);
	ret = DoCommand(tile2, 0, 0, flags | DC_AUTO, CMD_LANDSCAPE_CLEAR);
	if (ret.Failed()) return ret;
	if (add_cost) {
		cost.AddCost(ret);
	}

	if (flags & DC_EXEC) {
		Depot *depot = new Depot(tile);
		depot->build_date = _date;

		if (wc1 == WATER_CLASS_CANAL || wc2 == WATER_CLASS_CANAL) {
			/* Update infrastructure counts after the unconditional clear earlier. */
			Company::Get(_current_company)->infrastructure.water += wc1 == WATER_CLASS_CANAL && wc2 == WATER_CLASS_CANAL ? 2 : 1;
		}
		Company::Get(_current_company)->infrastructure.water += 2 * LOCK_DEPOT_TILE_FACTOR;
		DirtyCompanyInfrastructureWindows(_current_company);

		MakeShipDepot(tile,  _current_company, depot->index, ReverseDiagDir(AxisToDiagDir(axis)), wc1);
		MakeShipDepot(tile2, _current_company, depot->index, AxisToDiagDir(axis), wc2);
		MarkTileDirtyByTile(tile);
		MarkTileDirtyByTile(tile2);
		MakeDefaultName(depot);
	}

	return cost;
}

void MakeWaterKeepingClass(TileIndex tile, Owner o)
{
	WaterClass wc = GetWaterClass(tile);

	/* Autoslope might turn an originally canal or river tile into land */
	int z;
	Slope slope = GetTileSlope(tile, &z);

	if (slope != SLOPE_FLAT) {
		if (wc == WATER_CLASS_CANAL) {
			/* If we clear the canal, we have to remove it from the infrastructure count as well. */
			Company *c = Company::GetIfValid(o);
			if (c != NULL) {
				c->infrastructure.water--;
				DirtyCompanyInfrastructureWindows(c->index);
			}
			/* Sloped canals are locks and no natural water remains whatever the slope direction */
			wc = WATER_CLASS_INVALID;
		}

		/* Only river water should be restored on appropriate slopes. Other water would be invalid on slopes */
		if (wc != WATER_CLASS_RIVER || GetInclinedSlopeDirection(slope) == INVALID_DIAGDIR) {
			wc = WATER_CLASS_INVALID;
		}
	}

	if (wc == WATER_CLASS_SEA && z > 0) {
		/* Update company infrastructure count. */
		Company *c = Company::GetIfValid(o);
		if (c != NULL) {
			c->infrastructure.water++;
			DirtyCompanyInfrastructureWindows(c->index);
		}

		wc = WATER_CLASS_CANAL;
	}

	/* Zero map array and terminate animation */
	DoClearSquare(tile);

	/* Maybe change to water */
	switch (wc) {
		case WATER_CLASS_SEA:   MakeSea(tile);                break;
		case WATER_CLASS_CANAL: MakeCanal(tile, o, Random()); break;
		case WATER_CLASS_RIVER: MakeRiver(tile, Random());    break;
		default: break;
	}

	MarkTileDirtyByTile(tile);
}

static CommandCost RemoveShipDepot(TileIndex tile, DoCommandFlag flags)
{
	if (!IsShipDepot(tile)) return CMD_ERROR;

	CommandCost ret = CheckTileOwnership(tile);
	if (ret.Failed()) return ret;

	TileIndex tile2 = GetOtherShipDepotTile(tile);

	/* do not check for ship on tile when company goes bankrupt */
	if (!(flags & DC_BANKRUPT)) {
		StringID str = CheckVehicleOnGround (tile);
		if (str == STR_NULL) str = CheckVehicleOnGround (tile2);
		if (str != STR_NULL) return_cmd_error(str);
	}

	if (flags & DC_EXEC) {
		delete Depot::GetByTile(tile);

		Company *c = Company::GetIfValid(GetTileOwner(tile));
		if (c != NULL) {
			c->infrastructure.water -= 2 * LOCK_DEPOT_TILE_FACTOR;
			DirtyCompanyInfrastructureWindows(c->index);
		}

		MakeWaterKeepingClass(tile,  GetTileOwner(tile));
		MakeWaterKeepingClass(tile2, GetTileOwner(tile2));
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_CLEAR_DEPOT_SHIP]);
}

/**
 * Ensure there is no vehicle at the ground in the given lock area.
 * @param tile Central tile of lock area to examine.
 * @param delta Lock direction.
 * @return STR_NULL (ground is free) or an error message (a vehicle is found).
 */
static StringID CheckLockAreaFree (TileIndex tile, TileIndexDiff delta)
{
	StringID str = CheckVehicleOnGround (tile);
	if (str == STR_NULL) str = CheckVehicleOnGround (tile + delta);
	if (str == STR_NULL) str = CheckVehicleOnGround (tile - delta);
	return str;
}

/**
 * Builds a lock.
 * @param tile tile where to place the lock
 * @param flags type of operation
 * @param p1 unused
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildLock(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile));
	if (dir == INVALID_DIAGDIR) return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);

	CommandCost cost(EXPENSES_CONSTRUCTION);

	int delta = TileOffsByDiagDir(dir);
	StringID str = CheckLockAreaFree (tile, delta);
	if (str != STR_NULL) return_cmd_error(str);

	/* middle tile */
	WaterClass wc_middle = IsPlainWaterTile(tile) ? GetWaterClass(tile) : WATER_CLASS_CANAL;
	CommandCost ret = DoCommand (tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (ret.Failed()) return ret;
	cost.AddCost(ret);

	/* lower tile */
	if (!IsPlainWaterTile(tile - delta)) {
		ret = DoCommand(tile - delta, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (ret.Failed()) return ret;
		cost.AddCost(ret);
		cost.AddCost(_price[PR_BUILD_CANAL]);
	}
	if (!IsTileFlat(tile - delta)) {
		return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
	}
	WaterClass wc_lower = IsPlainWaterTile(tile - delta) ? GetWaterClass(tile - delta) : WATER_CLASS_CANAL;

	/* upper tile */
	if (!IsPlainWaterTile(tile + delta)) {
		ret = DoCommand(tile + delta, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (ret.Failed()) return ret;
		cost.AddCost(ret);
		cost.AddCost(_price[PR_BUILD_CANAL]);
	}
	if (!IsTileFlat(tile + delta)) {
		return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
	}
	WaterClass wc_upper = IsPlainWaterTile(tile + delta) ? GetWaterClass(tile + delta) : WATER_CLASS_CANAL;

	if (HasBridgeAbove(tile) || HasBridgeAbove(tile - delta) || HasBridgeAbove(tile + delta)) {
		return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);
	}

	if (flags & DC_EXEC) {
		/* Update company infrastructure counts. */
		Company *c = Company::GetIfValid(_current_company);
		if (c != NULL) {
			/* Counts for the water. */
			if (!IsPlainWaterTile(tile - delta)) c->infrastructure.water++;
			if (!IsPlainWaterTile(tile + delta)) c->infrastructure.water++;
			/* Count for the lock itself. */
			c->infrastructure.water += 3 * LOCK_DEPOT_TILE_FACTOR; // Lock is three tiles.
			DirtyCompanyInfrastructureWindows(_current_company);
		}

		MakeLock(tile, _current_company, dir, wc_lower, wc_upper, wc_middle);
		MarkTileDirtyByTile(tile);
		MarkTileDirtyByTile(tile - delta);
		MarkTileDirtyByTile(tile + delta);
		MarkCanalsAndRiversAroundDirty(tile - delta);
		MarkCanalsAndRiversAroundDirty(tile + delta);
	}
	cost.AddCost(_price[PR_BUILD_LOCK]);

	return cost;
}

/**
 * Remove a lock.
 * @param tile Central tile of the lock.
 * @param flags Operation to perform.
 * @return The cost in case of success, or an error code if it failed.
 */
static CommandCost RemoveLock(TileIndex tile, DoCommandFlag flags)
{
	if (GetTileOwner(tile) != OWNER_NONE) {
		CommandCost ret = CheckTileOwnership(tile);
		if (ret.Failed()) return ret;
	}

	TileIndexDiff delta = TileOffsByDiagDir(GetLockDirection(tile));

	/* make sure no vehicle is on the tile. */
	StringID str = CheckLockAreaFree (tile, delta);
	if (str != STR_NULL) return_cmd_error(str);

	if (flags & DC_EXEC) {
		/* Remove middle part from company infrastructure count. */
		Company *c = Company::GetIfValid(GetTileOwner(tile));
		if (c != NULL) {
			c->infrastructure.water -= 3 * LOCK_DEPOT_TILE_FACTOR; // three parts of the lock.
			DirtyCompanyInfrastructureWindows(c->index);
		}

		if (GetWaterClass(tile) == WATER_CLASS_RIVER) {
			MakeRiver(tile, Random());
		} else {
			DoClearSquare(tile);
		}
		MakeWaterKeepingClass(tile + delta, GetTileOwner(tile + delta));
		MakeWaterKeepingClass(tile - delta, GetTileOwner(tile - delta));
		MarkCanalsAndRiversAroundDirty(tile);
		MarkCanalsAndRiversAroundDirty(tile - delta);
		MarkCanalsAndRiversAroundDirty(tile + delta);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_CLEAR_LOCK]);
}

/** Create non-desert around a river tile. */
void RiverModifyDesertZone (TileIndex tile)
{
	TileArea ta (tile);
	ta.expand (2);

	TILE_AREA_LOOP(t, ta) {
		if (GetTropicZone(t) == TROPICZONE_DESERT) SetTropicZone (t, TROPICZONE_NORMAL);
	}
}

/**
 * Build a piece of canal.
 * @param tile end tile of stretch-dragging
 * @param flags type of operation
 * @param p1 start tile of stretch-dragging
 * @param p2 waterclass to build. sea and river can only be built in scenario editor
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildCanal(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	WaterClass wc = Extract<WaterClass, 0, 2>(p2);
	if (p1 >= MapSize() || wc == WATER_CLASS_INVALID) return CMD_ERROR;

	/* Outside of the editor you can only build canals, not oceans */
	if (wc != WATER_CLASS_CANAL && _game_mode != GM_EDITOR) return CMD_ERROR;

	TileArea ta(tile, p1);

	/* Outside the editor you can only drag canals, and not areas */
	if (_game_mode != GM_EDITOR && ta.w != 1 && ta.h != 1) return CMD_ERROR;

	CommandCost cost(EXPENSES_CONSTRUCTION);
	TILE_AREA_LOOP(tile, ta) {
		CommandCost ret;

		Slope slope = GetTileSlope(tile);
		if (slope != SLOPE_FLAT && (wc != WATER_CLASS_RIVER || !IsInclinedSlope(slope))) {
			return_cmd_error(STR_ERROR_FLAT_LAND_REQUIRED);
		}

		/* can't make water of water! */
		if (IsWaterTile(tile) && (!IsTileOwner(tile, OWNER_WATER) || wc == WATER_CLASS_SEA)) continue;

		bool water = IsPlainWaterTile(tile);
		ret = DoCommand(tile, 0, 0, flags | DC_FORCE_CLEAR_TILE, CMD_LANDSCAPE_CLEAR);
		if (ret.Failed()) return ret;

		if (!water) cost.AddCost(ret);

		if (flags & DC_EXEC) {
			switch (wc) {
				case WATER_CLASS_RIVER:
					MakeRiver(tile, Random());
					if (_game_mode == GM_EDITOR) {
						RiverModifyDesertZone (tile);
					}
					break;

				case WATER_CLASS_SEA:
					if (TileHeight(tile) == 0) {
						MakeSea(tile);
						break;
					}
					/* FALL THROUGH */

				default:
					MakeCanal(tile, _current_company, Random());
					if (Company::IsValidID(_current_company)) {
						Company::Get(_current_company)->infrastructure.water++;
						DirtyCompanyInfrastructureWindows(_current_company);
					}
					break;
			}
			MarkTileDirtyByTile(tile);
			MarkCanalsAndRiversAroundDirty(tile);
		}

		cost.AddCost(_price[PR_BUILD_CANAL]);
	}

	if (cost.GetCost() == 0) {
		return_cmd_error(STR_ERROR_ALREADY_BUILT);
	} else {
		return cost;
	}
}

static CommandCost ClearTile_Water(TileIndex tile, DoCommandFlag flags)
{
	switch (GetWaterTileType(tile)) {
		case WATER_TILE_CLEAR: {
			if (flags & DC_NO_WATER) return_cmd_error(STR_ERROR_CAN_T_BUILD_ON_WATER);

			Money base_cost = IsCanal(tile) ? _price[PR_CLEAR_CANAL] : _price[PR_CLEAR_WATER];
			/* Make sure freeform edges are allowed or it's not an edge tile. */
			if (!_settings_game.construction.freeform_edges && (!IsInsideMM(TileX(tile), 1, MapMaxX() - 1) ||
					!IsInsideMM(TileY(tile), 1, MapMaxY() - 1))) {
				return_cmd_error(STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP);
			}

			/* Make sure no vehicle is on the tile */
			StringID str = CheckVehicleOnGround (tile);
			if (str != STR_NULL) return_cmd_error(str);

			Owner owner = GetTileOwner(tile);
			if (owner != OWNER_WATER && owner != OWNER_NONE) {
				CommandCost ret = CheckTileOwnership(tile);
				if (ret.Failed()) return ret;
			}

			if (flags & DC_EXEC) {
				if (IsCanal(tile) && Company::IsValidID(owner)) {
					Company::Get(owner)->infrastructure.water--;
					DirtyCompanyInfrastructureWindows(owner);
				}
				DoClearSquare(tile);
				MarkCanalsAndRiversAroundDirty(tile);
			}

			return CommandCost(EXPENSES_CONSTRUCTION, base_cost);
		}

		case WATER_TILE_COAST: {
			Slope slope = GetTileSlope(tile);

			/* Make sure no vehicle is on the tile */
			StringID str = CheckVehicleOnGround (tile);
			if (str != STR_NULL) return_cmd_error(str);

			if (flags & DC_EXEC) {
				DoClearSquare(tile);
				MarkCanalsAndRiversAroundDirty(tile);
			}
			bool half = IsSlopeWithOneCornerRaised (slope);
			return CommandCost (EXPENSES_CONSTRUCTION,
				_price[half ? PR_CLEAR_WATER : PR_CLEAR_ROUGH]);
		}

		case WATER_TILE_DEPOT:
			if (flags & DC_AUTO) return_cmd_error(STR_ERROR_BUILDING_MUST_BE_DEMOLISHED);
			return RemoveShipDepot(tile, flags);

		case WATER_TILE_LOCK_MIDDLE:
			if (flags & DC_AUTO) return_cmd_error(STR_ERROR_BUILDING_MUST_BE_DEMOLISHED);
			if (_current_company == OWNER_WATER) return CMD_ERROR;
			return RemoveLock(tile, flags);

		case WATER_TILE_LOCK_LOWER:
			if (flags & DC_AUTO) return_cmd_error(STR_ERROR_BUILDING_MUST_BE_DEMOLISHED);
			if (_current_company == OWNER_WATER) return CMD_ERROR;
			/* move to the middle tile.. */
			tile += TileOffsByDiagDir(GetLockDirection(tile));
			return RemoveLock(tile, flags);

		case WATER_TILE_LOCK_UPPER:
			if (flags & DC_AUTO) return_cmd_error(STR_ERROR_BUILDING_MUST_BE_DEMOLISHED);
			if (_current_company == OWNER_WATER) return CMD_ERROR;
			/* move to the middle tile.. */
			tile -= TileOffsByDiagDir(GetLockDirection(tile));
			return RemoveLock(tile, flags);

		default:
			NOT_REACHED();
	}
}

/**
 * return true if a tile is a water tile wrt. a certain direction.
 *
 * @param tile The tile of interest.
 * @param from The direction of interest.
 * @return true iff the tile is water in the view of 'from'.
 *
 */
bool IsWateredTile(TileIndex tile, Direction from)
{
	if (IsIndustryTile(tile)) {
		/* Do not draw waterborders inside of industries.
		 * Note: There is no easy way to detect the industry of an oilrig tile. */
		TileIndex src_tile = tile + TileOffsByDir(from);
		if ((IsStationTile(src_tile) && IsOilRig(src_tile)) ||
		    (IsIndustryTile(src_tile) && GetIndustryIndex(src_tile) == GetIndustryIndex(tile))) return true;

		return IsTileOnWater(tile);
	}

	switch (GetTileType(tile)) {
		case TT_GROUND: return IsTileSubtype(tile, TT_GROUND_VOID); // consider map border as water, esp. for rivers

		case TT_WATER:
			switch (GetWaterTileType(tile)) {
				case WATER_TILE_CLEAR:
				case WATER_TILE_DEPOT:
					return true;

				case WATER_TILE_COAST:
					switch (GetTileSlope(tile)) {
						case SLOPE_W: return (from == DIR_SE) || (from == DIR_E) || (from == DIR_NE);
						case SLOPE_S: return (from == DIR_NE) || (from == DIR_N) || (from == DIR_NW);
						case SLOPE_E: return (from == DIR_NW) || (from == DIR_W) || (from == DIR_SW);
						case SLOPE_N: return (from == DIR_SW) || (from == DIR_S) || (from == DIR_SE);
						default: return false;
					}

				default:
					return DiagDirToAxis(GetLockDirection(tile)) == DiagDirToAxis(DirToDiagDir(from));
			}

		case TT_RAILWAY:
			if (IsTileSubtype(tile, TT_TRACK) && GetRailGroundType(tile) == RAIL_GROUND_WATER) {
				switch (GetTileSlope(tile)) {
					case SLOPE_W: return (from == DIR_SE) || (from == DIR_E) || (from == DIR_NE);
					case SLOPE_S: return (from == DIR_NE) || (from == DIR_N) || (from == DIR_NW);
					case SLOPE_E: return (from == DIR_NW) || (from == DIR_W) || (from == DIR_SW);
					case SLOPE_N: return (from == DIR_SW) || (from == DIR_S) || (from == DIR_SE);
					default: return false;
				}
			}
			return false;

		case TT_MISC:
			return IsTileSubtype(tile, TT_MISC_AQUEDUCT) && ReverseDiagDir(GetTunnelBridgeDirection(tile)) == DirToDiagDir(from);

		case TT_STATION:
			if (IsOilRig(tile)) {
				/* Do not draw waterborders inside of industries.
				 * Note: There is no easy way to detect the industry of an oilrig tile. */
				TileIndex src_tile = tile + TileOffsByDir(from);
				if ((IsStationTile(src_tile) && IsOilRig(src_tile)) ||
				    (IsIndustryTile(src_tile))) return true;

				return IsTileOnWater(tile);
			}
			return (IsDock(tile) && IsTileFlat(tile)) || IsBuoy(tile);

		case TT_OBJECT: return IsTileOnWater(tile);

		default:          return false;
	}
}

/**
 * Draw a water sprite, potentially with a NewGRF-modified sprite offset.
 * @param ti TileInfo of the tile to draw
 * @param base    Sprite base.
 * @param offset  Sprite offset.
 * @param feature The type of sprite that is drawn.
 */
static void DrawWaterSprite (const TileInfo *ti, SpriteID base, uint offset, CanalFeature feature)
{
	if (base != SPR_FLAT_WATER_TILE) {
		/* Only call offset callback if the sprite is NewGRF-provided. */
		offset = GetCanalSpriteOffset (feature, ti->tile, offset);
	}
	DrawGroundSprite (ti, base + offset, PAL_NONE);
}

/**
 * Draw canal or river edges.
 * @param ti TileInfo of the tile to draw
 * @param canal  True if canal edges should be drawn, false for river edges.
 * @param offset Sprite offset.
 */
static void DrawWaterEdges (const TileInfo *ti, bool canal, uint offset)
{
	TileIndex tile = ti->tile;

	CanalFeature feature;
	SpriteID base = 0;
	if (canal) {
		feature = CF_DIKES;
		base = GetCanalSprite(CF_DIKES, tile);
		if (base == 0) base = SPR_CANAL_DIKES_BASE;
	} else {
		feature = CF_RIVER_EDGE;
		base = GetCanalSprite(CF_RIVER_EDGE, tile);
		if (base == 0) return; // Don't draw if no sprites provided.
	}

	uint wa;

	/* determine the edges around with water. */
	wa  = IsWateredTile(TILE_ADDXY(tile, -1,  0), DIR_SW) << 0;
	wa += IsWateredTile(TILE_ADDXY(tile,  0,  1), DIR_NW) << 1;
	wa += IsWateredTile(TILE_ADDXY(tile,  1,  0), DIR_NE) << 2;
	wa += IsWateredTile(TILE_ADDXY(tile,  0, -1), DIR_SE) << 3;

	if (!(wa & 1)) DrawWaterSprite (ti, base, offset,     feature);
	if (!(wa & 2)) DrawWaterSprite (ti, base, offset + 1, feature);
	if (!(wa & 4)) DrawWaterSprite (ti, base, offset + 2, feature);
	if (!(wa & 8)) DrawWaterSprite (ti, base, offset + 3, feature);

	/* right corner */
	switch (wa & 0x03) {
		case 0: DrawWaterSprite (ti, base, offset + 4, feature); break;
		case 3: if (!IsWateredTile(TILE_ADDXY(tile, -1, 1), DIR_W)) DrawWaterSprite (ti, base, offset + 8, feature); break;
	}

	/* bottom corner */
	switch (wa & 0x06) {
		case 0: DrawWaterSprite (ti, base, offset + 5, feature); break;
		case 6: if (!IsWateredTile(TILE_ADDXY(tile, 1, 1), DIR_N)) DrawWaterSprite (ti, base, offset + 9, feature); break;
	}

	/* left corner */
	switch (wa & 0x0C) {
		case  0: DrawWaterSprite (ti, base, offset + 6, feature); break;
		case 12: if (!IsWateredTile(TILE_ADDXY(tile, 1, -1), DIR_E)) DrawWaterSprite (ti, base, offset + 10, feature); break;
	}

	/* upper corner */
	switch (wa & 0x09) {
		case 0: DrawWaterSprite (ti, base, offset + 7, feature); break;
		case 9: if (!IsWateredTile(TILE_ADDXY(tile, -1, -1), DIR_S)) DrawWaterSprite (ti, base, offset + 11, feature); break;
	}
}

#include "table/water_land.h"

/**
 * Draw a build sprite sequence for water tiles.
 * If buildings are invisible, nothing will be drawn.
 * @param ti      Tile info.
 * @param dtss     Sprite sequence to draw.
 * @param base    Base sprite.
 * @param offset  Additional sprite offset.
 * @param palette Palette to use.
 */
static void DrawWaterTileStruct(const TileInfo *ti, const DrawTileSeqStruct *dtss, SpriteID base, uint offset, PaletteID palette, CanalFeature feature)
{
	/* Don't draw if buildings are invisible. */
	if (IsInvisibilitySet(TO_BUILDINGS)) return;

	for (; !dtss->IsTerminator(); dtss++) {
		uint tile_offs = offset + dtss->image.sprite;
		if (feature < CF_END) tile_offs = GetCanalSpriteOffset(feature, ti->tile, tile_offs);
		AddSortableSpriteToDraw (ti->vd, base + tile_offs, palette,
			ti->x + dtss->delta_x, ti->y + dtss->delta_y,
			dtss->size_x, dtss->size_y,
			dtss->size_z, ti->z + dtss->delta_z,
			IsTransparencySet(TO_BUILDINGS));
	}
}

static uint DrawRiverWater (const TileInfo *ti)
{
	SpriteID image = SPR_FLAT_WATER_TILE;
	uint     edges_offset = 0;

	if (ti->tileh != SLOPE_FLAT || HasBit(_water_feature[CF_RIVER_SLOPE].flags, CFF_HAS_FLAT_SPRITE)) {
		image = GetCanalSprite(CF_RIVER_SLOPE, ti->tile);
		if (image == 0) {
			switch (ti->tileh) {
				case SLOPE_NW: image = SPR_WATER_SLOPE_Y_DOWN; break;
				case SLOPE_SW: image = SPR_WATER_SLOPE_X_UP;   break;
				case SLOPE_SE: image = SPR_WATER_SLOPE_Y_UP;   break;
				case SLOPE_NE: image = SPR_WATER_SLOPE_X_DOWN; break;
				default:       image = SPR_FLAT_WATER_TILE;    break;
			}
		} else {
			/* Flag bit 0 indicates that the first sprite is flat water. */
			uint offset = HasBit(_water_feature[CF_RIVER_SLOPE].flags, CFF_HAS_FLAT_SPRITE) ? 1 : 0;

			switch (ti->tileh) {
				case SLOPE_SE:              edges_offset += 12; break;
				case SLOPE_NE: offset += 1; edges_offset += 24; break;
				case SLOPE_SW: offset += 2; edges_offset += 36; break;
				case SLOPE_NW: offset += 3; edges_offset += 48; break;
				default:       offset  = 0; break;
			}

			image += GetCanalSpriteOffset (CF_RIVER_SLOPE, ti->tile, offset);
		}
	}

	DrawGroundSprite (ti, image, PAL_NONE);

	return edges_offset;
}

void DrawShoreTile (const TileInfo *ti)
{
	/* Converts the enum Slope into an offset based on SPR_SHORE_BASE.
	 * This allows to calculate the proper sprite to display for this Slope */
	static const byte tileh_to_shoresprite[32] = {
		0, 1, 2, 3, 4, 16, 6, 7, 8, 9, 17, 11, 12, 13, 14, 0,
		0, 0, 0, 0, 0,  0, 0, 0, 0, 0,  0,  5,  0, 10, 15, 0,
	};

	Slope tileh = ti->tileh;

	assert(!IsHalftileSlope(tileh)); // Halftile slopes need to get handled earlier.
	assert(tileh != SLOPE_FLAT);     // Shore is never flat

	assert((tileh != SLOPE_EW) && (tileh != SLOPE_NS)); // No suitable sprites for current flooding behaviour

	DrawGroundSprite (ti, SPR_SHORE_BASE + tileh_to_shoresprite[tileh], PAL_NONE);
}

void DrawWaterClassGround(const TileInfo *ti)
{
	uint edges_offset;
	bool canal;

	switch (GetWaterClass(ti->tile)) {
		case WATER_CLASS_SEA:
			DrawGroundSprite (ti, SPR_FLAT_WATER_TILE, PAL_NONE);
			/* No edges drawn for sea tiles. */
			return;

		case WATER_CLASS_CANAL: {
			SpriteID image = SPR_FLAT_WATER_TILE;
			if (HasBit(_water_feature[CF_WATERSLOPE].flags, CFF_HAS_FLAT_SPRITE)) {
				/* First water slope sprite is flat water. */
				image = GetCanalSprite (CF_WATERSLOPE, ti->tile);
				if (image == 0) image = SPR_FLAT_WATER_TILE;
			}
			DrawWaterSprite (ti, image, 0, CF_WATERSLOPE);
			edges_offset = 0;
			canal = true;
			break;
		}

		case WATER_CLASS_RIVER:
			edges_offset = DrawRiverWater (ti);
			canal = false;
			break;

		default: NOT_REACHED();
	}

	/* Draw river edges if available. */
	DrawWaterEdges (ti, canal, edges_offset);
}

static void DrawTile_Water(TileInfo *ti)
{
	WaterTileType type = GetWaterTileType (ti->tile);
	switch (type) {
		case WATER_TILE_CLEAR:
			DrawWaterClassGround(ti);
			DrawBridgeMiddle(ti);
			break;

		case WATER_TILE_COAST: {
			DrawShoreTile (ti);
			DrawBridgeMiddle(ti);
			break;
		}

		case WATER_TILE_DEPOT:
			DrawWaterClassGround (ti);
			DrawWaterTileStruct (ti, _shipdepot_display_data[GetShipDepotDirection(ti->tile)],
					0, 0, COMPANY_SPRITE_COLOUR(GetTileOwner(ti->tile)), CF_END);
			break;

		default:
			uint part = type - WATER_TILE_LOCK_MIDDLE;
			DiagDirection dir = GetLockDirection (ti->tile);

			/* Draw ground sprite. */
			bool has_flat_water = HasBit(_water_feature[CF_WATERSLOPE].flags, CFF_HAS_FLAT_SPRITE);
			bool use_default = true;
			SpriteID image;
			if (has_flat_water || (part == 0)) {
				image = GetCanalSprite (CF_WATERSLOPE, ti->tile);
				if (image != 0) {
					/* NewGRF supplies a flat sprite as first sprite? */
					if (part == 0) image += has_flat_water;
					use_default = false;
				}
			}

			if (use_default) {
				/* Use default sprites. */
				image = (part != 0) ? SPR_FLAT_WATER_TILE : SPR_CANALS_BASE;
			}

			static uint8 lock_middle_offset[DIAGDIR_END] = { 1, 0, 2, 3 };
			if (part == 0) image += lock_middle_offset[dir];
			DrawGroundSprite (ti, image, PAL_NONE);

			const DrawTileSeqStruct *dts = _lock_display_data[part][dir];

			/* Draw structures. */
			uint     zoffs = 0;
			SpriteID base  = GetCanalSprite (CF_LOCKS, ti->tile);

			if (base == 0) {
				/* If no custom graphics, use defaults. */
				base = SPR_LOCK_BASE;
				bool upper = part == (WATER_TILE_LOCK_UPPER - WATER_TILE_LOCK_MIDDLE);
				uint8 z_threshold = upper ? 8 : 0;
				zoffs = ti->z > z_threshold ? 24 : 0;
			}

			DrawWaterTileStruct (ti, dts, base, zoffs, PAL_NONE, CF_LOCKS);
			break;
	}
}

void DrawShipDepotSprite (BlitArea *dpi, int x, int y, DiagDirection dir)
{
	DrawSprite (dpi, SPR_FLAT_WATER_TILE, PAL_NONE, x, y);
	DrawOrigTileSeqInGUI (dpi, x, y, _shipdepot_display_data[dir],
				COMPANY_SPRITE_COLOUR(_local_company));
}


static int GetSlopePixelZ_Water(TileIndex tile, uint x, uint y)
{
	int z;
	Slope tileh = GetTilePixelSlope(tile, &z);

	return z + GetPartialPixelZ(x & 0xF, y & 0xF, tileh);
}

static Foundation GetFoundation_Water(TileIndex tile, Slope tileh)
{
	return FOUNDATION_NONE;
}

static void GetTileDesc_Water(TileIndex tile, TileDesc *td)
{
	switch (GetWaterTileType(tile)) {
		case WATER_TILE_CLEAR:
			switch (GetWaterClass(tile)) {
				case WATER_CLASS_SEA:   td->str = STR_LAI_WATER_DESCRIPTION_WATER; break;
				case WATER_CLASS_CANAL: td->str = STR_LAI_WATER_DESCRIPTION_CANAL; break;
				case WATER_CLASS_RIVER: td->str = STR_LAI_WATER_DESCRIPTION_RIVER; break;
				default: NOT_REACHED(); break;
			}
			break;
		case WATER_TILE_COAST: td->str = STR_LAI_WATER_DESCRIPTION_COAST_OR_RIVERBANK; break;
		case WATER_TILE_DEPOT:
			td->str = STR_LAI_WATER_DESCRIPTION_SHIP_DEPOT;
			td->build_date = Depot::GetByTile(tile)->build_date;
			break;
		default: td->str = STR_LAI_WATER_DESCRIPTION_LOCK; break;
	}

	td->owner[0] = GetTileOwner(tile);
}

/**
 * Handle the flooding of a vehicle. This sets the vehicle state to crashed,
 * creates a newsitem and dirties the necessary windows.
 * @param v The vehicle to flood.
 */
static void FloodVehicle(Vehicle *v)
{
	uint pass = v->Crash(true);

	AI::NewEvent(v->owner, new ScriptEventVehicleCrashed(v->index, v->tile, ScriptEventVehicleCrashed::CRASH_FLOODED));
	Game::NewEvent(new ScriptEventVehicleCrashed(v->index, v->tile, ScriptEventVehicleCrashed::CRASH_FLOODED));
	AddNewsItem<VehicleNewsItem> (STR_NEWS_DISASTER_FLOOD_VEHICLE,
		NT_ACCIDENT, v->index, pass);
	CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);
	if (_settings_client.sound.disaster) SndPlayVehicleFx(SND_12_EXPLOSION, v);
}

/**
 * Flood vehicles on a tile if we are allowed to flood them, i.e. when they are on the ground.
 * @param tile The tile to flood.
 * @param z The z of level to flood.
 */
static void FloodTileVehicles(TileIndex tile, int z)
{
	VehicleTileIterator iter (tile);
	while (!iter.finished()) {
		Vehicle *v = iter.next();

		if ((v->vehstatus & VS_CRASHED) != 0) continue;

		switch (v->type) {
			default: break;

			case VEH_TRAIN:
			case VEH_ROAD:
				if (v->z_pos <= z) FloodVehicle(v->First());
		}
	}
}

/**
 * Finds a vehicle to flood.
 * It does not find vehicles that are already crashed on bridges, i.e. flooded.
 * @param tile the tile where to find a vehicle to flood
 */
static void FloodVehicles(TileIndex tile)
{
	if (IsAirportTile(tile)) {
		if (GetTileMaxZ(tile) != 0) return;

		const Station *st = Station::GetByTile(tile);
		TILE_AREA_LOOP(tile, st->airport) {
			if (st->TileBelongsToAirport(tile)) {
				VehicleTileIterator iter (tile);
				while (!iter.finished()) {
					Vehicle *v = iter.next();

					if (v->type != VEH_AIRCRAFT) continue;
					if (v->subtype == AIR_SHADOW) continue;
					if ((v->vehstatus & VS_CRASHED) != 0) continue;

					/* We compare v->z_pos against delta_z + 1 because the shadow
					 * is at delta_z and the actual aircraft at delta_z + 1. */
					const AirportFTAClass *airport = st->airport.GetFTA();
					if (v->z_pos == airport->delta_z + 1) FloodVehicle(v);
				}
			}
		}

		/* No vehicle could be flooded on this airport anymore */
		return;
	}

	if (!IsBridgeHeadTile(tile)) {
		FloodTileVehicles (tile, 0);
		return;
	}

	int z = GetBridgePixelHeight(tile);
	FloodTileVehicles (tile, z);
	FloodTileVehicles (GetOtherBridgeEnd(tile), z);
}

/**
 * Returns the behaviour of a tile during flooding.
 *
 * @return Behaviour of the tile
 */
FloodingBehaviour GetFloodingBehaviour(TileIndex tile)
{
	/* FLOOD_ACTIVE:  'single-corner-raised'-coast, sea, sea-shipdepots, sea-buoys, sea-docks (water part), rail with flooded halftile, sea-water-industries, sea-oilrigs
	 * FLOOD_DRYUP:   coast with more than one corner raised, coast with rail-track, coast with trees
	 * FLOOD_PASSIVE: (not used)
	 * FLOOD_NONE:    canals, rivers, everything else
	 */
	if (IsIndustryTile(tile)) {
		return (GetWaterClass(tile) == WATER_CLASS_SEA) ? FLOOD_ACTIVE : FLOOD_NONE;
	}

	switch (GetTileType(tile)) {
		case TT_WATER:
			if (IsCoast(tile)) {
				Slope tileh = GetTileSlope(tile);
				return (IsSlopeWithOneCornerRaised(tileh) ? FLOOD_ACTIVE : FLOOD_DRYUP);
			}
			/* FALL THROUGH */
		case TT_STATION:
		case TT_OBJECT:
			return (GetWaterClass(tile) == WATER_CLASS_SEA) ? FLOOD_ACTIVE : FLOOD_NONE;

		case TT_RAILWAY:
			if (IsTileSubtype(tile, TT_TRACK) && GetRailGroundType(tile) == RAIL_GROUND_WATER) {
				return (IsSlopeWithOneCornerRaised(GetTileSlope(tile)) ? FLOOD_ACTIVE : FLOOD_DRYUP);
			}
			return FLOOD_NONE;

		case TT_GROUND:
			return (IsTreeTile(tile) && GetClearGround(tile) == GROUND_SHORE ? FLOOD_DRYUP : FLOOD_NONE);

		default:
			return FLOOD_NONE;
	}
}

/**
 * Floods a tile.
 */
void DoFloodTile(TileIndex target)
{
	assert(!IsWaterTile(target));

	bool flooded = false; // Will be set to true if something is changed.

	Backup<CompanyByte> cur_company(_current_company, OWNER_WATER, FILE_LINE);

	Slope tileh = GetTileSlope(target);
	if (tileh != SLOPE_FLAT) {
		/* make coast.. */
		switch (GetTileType(target)) {
			case TT_RAILWAY:
				if (IsTileSubtype(target, TT_TRACK)) {
					FloodVehicles(target);
					flooded = FloodHalftile(target);
				}
				break;

			case TT_GROUND:
				if (IsTreeTile(target) && !IsSlopeWithOneCornerRaised(tileh)) {
					SetClearGroundDensity(target, GROUND_SHORE, 3, true);
					MarkTileDirtyByTile(target);
					flooded = true;
					break;
				}
				if (DoCommand(target, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR).Succeeded()) {
					MakeShore(target);
					MarkTileDirtyByTile(target);
					flooded = true;
				}
				break;

			default:
				break;
		}
	} else {
		/* Flood vehicles */
		FloodVehicles(target);

		/* flood flat tile */
		if (DoCommand(target, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR).Succeeded()) {
			MakeSea(target);
			MarkTileDirtyByTile(target);
			flooded = true;
		}
	}

	if (flooded) {
		/* Mark surrounding canal tiles dirty too to avoid glitches */
		MarkCanalsAndRiversAroundDirty(target);

		/* update signals if needed */
		UpdateSignalsInBuffer();
	}

	cur_company.Restore();
}

/**
 * Drys a tile up.
 */
static void DoDryUp(TileIndex tile)
{
	Backup<CompanyByte> cur_company(_current_company, OWNER_WATER, FILE_LINE);

	switch (GetTileType(tile)) {
		case TT_RAILWAY:
			assert(IsTileSubtype(tile, TT_TRACK));
			assert(GetRailGroundType(tile) == RAIL_GROUND_WATER);

			RailGroundType new_ground;
			switch (GetTrackBits(tile)) {
				case TRACK_BIT_UPPER: new_ground = RAIL_GROUND_FENCE_HORIZ1; break;
				case TRACK_BIT_LOWER: new_ground = RAIL_GROUND_FENCE_HORIZ2; break;
				case TRACK_BIT_LEFT:  new_ground = RAIL_GROUND_FENCE_VERT1;  break;
				case TRACK_BIT_RIGHT: new_ground = RAIL_GROUND_FENCE_VERT2;  break;
				default: NOT_REACHED();
			}
			SetRailGroundType(tile, new_ground);
			MarkTileDirtyByTile(tile);
			break;

		case TT_GROUND:
			assert(IsTreeTile(tile));
			SetClearGroundDensity(tile, GROUND_GRASS, 3, true);
			MarkTileDirtyByTile(tile);
			break;

		case TT_WATER:
			assert(IsCoast(tile));

			if (DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR).Succeeded()) {
				MakeClear(tile, GROUND_GRASS, 3);
				MarkTileDirtyByTile(tile);
			}
			break;

		default: NOT_REACHED();
	}

	cur_company.Restore();
}

/**
 * Let a water tile floods its diagonal adjoining tiles
 * called from tunnelbridge_cmd, and by TileLoop_Industry() and TileLoop_Track()
 *
 * @param tile the water/shore tile that floods
 */
void TileLoop_Water(TileIndex tile)
{
	if (IsWaterTile(tile)) AmbientSoundEffect(tile);

	switch (GetFloodingBehaviour(tile)) {
		case FLOOD_ACTIVE:
			for (Direction dir = DIR_BEGIN; dir < DIR_END; dir++) {
				TileIndex dest = tile + TileOffsByDir(dir);
				if (!IsValidTile(dest)) continue;
				/* do not try to flood water tiles - increases performance a lot */
				if (IsWaterTile(dest)) continue;

				/* GROUND_SHORE is the sign of a previous flood. */
				if ((IsClearTile(dest) || IsTreeTile(dest)) && IsClearGround (dest, GROUND_SHORE)) continue;

				int z_dest;
				Slope slope_dest = GetFoundationSlope(dest, &z_dest) & ~SLOPE_HALFTILE_MASK & ~SLOPE_STEEP;
				if (z_dest > 0) continue;

				if (!HasBit(_flood_from_dirs[slope_dest], ReverseDir(dir))) continue;

				DoFloodTile(dest);
			}
			break;

		case FLOOD_DRYUP: {
			Slope slope_here = GetFoundationSlope(tile) & ~SLOPE_HALFTILE_MASK & ~SLOPE_STEEP;
			uint dir;
			FOR_EACH_SET_BIT(dir, _flood_from_dirs[slope_here]) {
				TileIndex dest = tile + TileOffsByDir((Direction)dir);
				if (!IsValidTile(dest)) continue;

				FloodingBehaviour dest_behaviour = GetFloodingBehaviour(dest);
				if ((dest_behaviour == FLOOD_ACTIVE) || (dest_behaviour == FLOOD_PASSIVE)) return;
			}
			DoDryUp(tile);
			break;
		}

		default: return;
	}
}

void ConvertGroundTilesIntoWaterTiles()
{
	int z;

	for (TileIndex tile = 0; tile < MapSize(); ++tile) {
		Slope slope = GetTileSlope(tile, &z);
		if (IsGroundTile(tile) && z == 0) {
			/* Make both water for tiles at level 0
			 * and make shore, as that looks much better
			 * during the generation. */
			switch (slope) {
				case SLOPE_FLAT:
					MakeSea(tile);
					break;

				case SLOPE_N:
				case SLOPE_E:
				case SLOPE_S:
				case SLOPE_W:
					MakeShore(tile);
					break;

				default:
					uint dir;
					FOR_EACH_SET_BIT(dir, _flood_from_dirs[slope & ~SLOPE_STEEP]) {
						TileIndex dest = TILE_ADD(tile, TileOffsByDir((Direction)dir));
						Slope slope_dest = GetTileSlope(dest) & ~SLOPE_STEEP;
						if (slope_dest == SLOPE_FLAT || IsSlopeWithOneCornerRaised(slope_dest)) {
							MakeShore(tile);
							break;
						}
					}
					break;
			}
		}
	}
}

static TrackdirBits GetTileWaterwayStatus_Water(TileIndex tile, DiagDirection side)
{
	static const byte coast_tracks[] = {0, 32, 4, 0, 16, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0};

	TrackBits ts;

	switch (GetWaterTileType(tile)) {
		case WATER_TILE_CLEAR: ts = IsTileFlat(tile) ? TRACK_BIT_ALL : TRACK_BIT_NONE; break;
		case WATER_TILE_COAST: ts = (TrackBits)coast_tracks[GetTileSlope(tile) & 0xF]; break;
		case WATER_TILE_DEPOT: ts = DiagDirToDiagTrackBits(GetShipDepotDirection(tile)); break;
		default:               ts = DiagDirToDiagTrackBits(GetLockDirection(tile)); break;
	}
	if (TileX(tile) == 0) {
		/* NE border: remove tracks that connects NE tile edge */
		ts &= ~(TRACK_BIT_X | TRACK_BIT_UPPER | TRACK_BIT_RIGHT);
	}
	if (TileY(tile) == 0) {
		/* NW border: remove tracks that connects NW tile edge */
		ts &= ~(TRACK_BIT_Y | TRACK_BIT_LEFT | TRACK_BIT_UPPER);
	}
	return TrackBitsToTrackdirBits(ts);
}

static bool ClickTile_Water(TileIndex tile)
{
	if (IsShipDepot(tile)) {
		ShowDepotWindow(GetShipDepotNorthTile(tile), VEH_SHIP);
		return true;
	}
	return false;
}

static void ChangeTileOwner_Water(TileIndex tile, Owner old_owner, Owner new_owner)
{
	if (!IsTileOwner(tile, old_owner)) return;

	bool is_lock_middle = GetWaterTileType(tile) == WATER_TILE_LOCK_MIDDLE;

	/* No need to dirty company windows here, we'll redraw the whole screen anyway. */
	if (is_lock_middle) Company::Get(old_owner)->infrastructure.water -= 3 * LOCK_DEPOT_TILE_FACTOR; // Lock has three parts.
	if (new_owner != INVALID_OWNER) {
		if (is_lock_middle) Company::Get(new_owner)->infrastructure.water += 3 * LOCK_DEPOT_TILE_FACTOR; // Lock has three parts.
		/* Only subtract from the old owner here if the new owner is valid,
		 * otherwise we clear ship depots and canal water below. */
		if (GetWaterClass(tile) == WATER_CLASS_CANAL && !is_lock_middle) {
			Company::Get(old_owner)->infrastructure.water--;
			Company::Get(new_owner)->infrastructure.water++;
		}
		if (IsShipDepot(tile)) {
			Company::Get(old_owner)->infrastructure.water -= LOCK_DEPOT_TILE_FACTOR;
			Company::Get(new_owner)->infrastructure.water += LOCK_DEPOT_TILE_FACTOR;
		}

		SetTileOwner(tile, new_owner);
		return;
	}

	/* Remove depot */
	if (IsShipDepot(tile)) DoCommand(tile, 0, 0, DC_EXEC | DC_BANKRUPT, CMD_LANDSCAPE_CLEAR);

	/* Set owner of canals and locks ... and also canal under dock there was before.
	 * Check if the new owner after removing depot isn't OWNER_WATER. */
	if (IsTileOwner(tile, old_owner)) {
		if (GetWaterClass(tile) == WATER_CLASS_CANAL && !is_lock_middle) Company::Get(old_owner)->infrastructure.water--;
		SetTileOwner(tile, OWNER_NONE);
	}
}

static CommandCost TerraformTile_Water(TileIndex tile, DoCommandFlag flags, int z_new, Slope tileh_new)
{
	/* Canals can't be terraformed */
	if (IsPlainWaterTile(tile) && IsCanal(tile)) return_cmd_error(STR_ERROR_MUST_DEMOLISH_CANAL_FIRST);

	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
}


extern const TileTypeProcs _tile_type_water_procs = {
	DrawTile_Water,           // draw_tile_proc
	GetSlopePixelZ_Water,     // get_slope_z_proc
	ClearTile_Water,          // clear_tile_proc
	NULL,                     // add_accepted_cargo_proc
	GetTileDesc_Water,        // get_tile_desc_proc
	NULL,                     // get_tile_railway_status_proc
	NULL,                     // get_tile_road_status_proc
	GetTileWaterwayStatus_Water, // get_tile_waterway_status_proc
	ClickTile_Water,          // click_tile_proc
	NULL,                     // animate_tile_proc
	TileLoop_Water,           // tile_loop_proc
	ChangeTileOwner_Water,    // change_tile_owner_proc
	NULL,                     // add_produced_cargo_proc
	GetFoundation_Water,      // get_foundation_proc
	TerraformTile_Water,      // terraform_tile_proc
};
