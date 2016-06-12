/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file misctile_cmd.cpp Handling of misc tiles. */

#include "stdafx.h"
#include "map/zoneheight.h"
#include "map/road.h"
#include "tile_cmd.h"
#include "bridge.h"
#include "signalbuffer.h"
#include "command_func.h"
#include "company_func.h"
#include "vehicle_func.h"
#include "pbs.h"
#include "train.h"
#include "roadveh.h"
#include "depot_base.h"
#include "viewport_func.h"
#include "newgrf_railtype.h"
#include "elrail_func.h"
#include "depot_func.h"
#include "autoslope.h"
#include "road_cmd.h"
#include "town.h"
#include "tunnelbridge.h"
#include "ship.h"
#include "company_base.h"
#include "strings_func.h"
#include "company_gui.h"
#include "cheat_type.h"
#include "sound_func.h"
#include "newgrf_sound.h"

#include "pathfinder/yapf/yapf.h"

#include "table/strings.h"
#include "table/track_land.h"
#include "table/road_land.h"


/**
 * Draws a tunnel tile.
 * @param ti TileInfo of the structure to draw
 * Please note that in this code, "roads" are treated as railtype 1, whilst the real railtypes are 0, 2 and 3
 */
static void DrawTunnel(TileInfo *ti)
{
	TransportType transport_type = GetTunnelTransportType(ti->tile);
	DiagDirection tunnelbridge_direction = GetTunnelBridgeDirection(ti->tile);

	/* Front view of tunnel bounding boxes:
	 *
	 *   122223  <- BB_Z_SEPARATOR
	 *   1    3
	 *   1    3                1,3 = empty helper BB
	 *   1    3                  2 = SpriteCombine of tunnel-roof and catenary (tram & elrail)
	 *
	 */

	static const int _tunnel_BB[4][12] = {
		/*  tunnnel-roof  |  Z-separator  | tram-catenary
		 * w  h  bb_x bb_y| x   y   w   h |bb_x bb_y w h */
		{  1,  0, -15, -14,  0, 15, 16,  1, 0, 1, 16, 15 }, // NE
		{  0,  1, -14, -15, 15,  0,  1, 16, 1, 0, 15, 16 }, // SE
		{  1,  0, -15, -14,  0, 15, 16,  1, 0, 1, 16, 15 }, // SW
		{  0,  1, -14, -15, 15,  0,  1, 16, 1, 0, 15, 16 }, // NW
	};
	const int *BB_data = _tunnel_BB[tunnelbridge_direction];

	bool catenary = false;

	SpriteID image;
	SpriteID railtype_overlay = 0;
	if (transport_type == TRANSPORT_RAIL) {
		const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));
		image = rti->base_sprites.tunnel;
		if (rti->UsesOverlay()) {
			/* Check if the railtype has custom tunnel portals. */
			railtype_overlay = GetCustomRailSprite(rti, ti->tile, RTSG_TUNNEL_PORTAL);
			if (railtype_overlay != 0) image = SPR_RAILTYPE_TUNNEL_BASE; // Draw blank grass tunnel base.
		}
	} else {
		image = SPR_TUNNEL_ENTRY_REAR_ROAD;
	}

	if (IsOnSnow(ti->tile)) image += railtype_overlay != 0 ? 8 : 32;

	image += tunnelbridge_direction * 2;
	DrawGroundSprite(image, PAL_NONE);

	/* PBS debugging, draw reserved tracks darker */
	if (_game_mode != GM_MENU && _settings_client.gui.show_track_reservation && (transport_type == TRANSPORT_RAIL && HasTunnelHeadReservation(ti->tile))) {
		const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));
		Axis axis = DiagDirToAxis (tunnelbridge_direction);
		SpriteID image = rti->UsesOverlay() ?
				GetCustomRailSprite (rti, ti->tile, RTSG_OVERLAY) + RTO_X + axis :
				rti->base_sprites.single[AxisToTrack(axis)];
		DrawGroundSprite (image, PALETTE_CRASH);
	}

	if (transport_type == TRANSPORT_ROAD) {
		RoadTypes rts = GetRoadTypes(ti->tile);

		if (HasBit(rts, ROADTYPE_TRAM)) {
			static const SpriteID tunnel_sprites[2][4] = { { 28, 78, 79, 27 }, {  5, 76, 77,  4 } };

			DrawGroundSprite(SPR_TRAMWAY_BASE + tunnel_sprites[rts - ROADTYPES_TRAM][tunnelbridge_direction], PAL_NONE);

			/* Do not draw wires if they are invisible */
			if (!IsInvisibilitySet(TO_CATENARY)) {
				catenary = true;
				StartSpriteCombine();
				AddSortableSpriteToDraw(SPR_TRAMWAY_TUNNEL_WIRES + tunnelbridge_direction, PAL_NONE, ti->x, ti->y, BB_data[10], BB_data[11], TILE_HEIGHT, ti->z, IsTransparencySet(TO_CATENARY), BB_data[8], BB_data[9], BB_Z_SEPARATOR);
			}
		}
	} else {
		const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));
		if (rti->UsesOverlay()) {
			SpriteID surface = GetCustomRailSprite(rti, ti->tile, RTSG_TUNNEL);
			if (surface != 0) DrawGroundSprite(surface + tunnelbridge_direction, PAL_NONE);
		}

		if (HasCatenaryDrawn (rti)) {
			/* Maybe draw pylons on the entry side */
			DrawCatenary(ti);

			catenary = true;
			StartSpriteCombine();
			/* Draw wire above the ramp */
			DrawCatenaryOnTunnel(ti);
		}

		if (maptile_has_tunnel_signals(ti->tile)) {
			static const struct {
				Point pos[2][2]; // signal position (outwards, inwards), (left side, right side)
				uint image; // offset from base signal sprite
			} SignalData[DIAGDIR_END] = {
				{ { { { 0,  3}, { 0, 13} }, { {15,  3}, {15, 13} } }, 0 }, // DIAGDIR_NE
				{ { { { 3, 15}, {13, 15} }, { { 3,  0}, {13,  0} } }, 2 }, // DIAGDIR_SE
				{ { { {15, 13}, {15,  3} }, { { 0, 13}, { 0,  3} } }, 1 }, // DIAGDIR_SW
				{ { { {13,  0}, { 3,  0} }, { {13, 15}, { 3, 15} } }, 3 }, // DIAGDIR_NW
			};

			assert(maptile_has_tunnel_signal(ti->tile, true) != maptile_has_tunnel_signal(ti->tile, false));

			DiagDirection dd = tunnelbridge_direction;
			bool inwards = maptile_has_tunnel_signal(ti->tile, true);
			if (!inwards) dd = ReverseDiagDir(dd);

			SignalType type       = maptile_get_tunnel_signal_type(ti->tile);
			SignalVariant variant = maptile_get_tunnel_signal_variant(ti->tile);
			SignalState condition = maptile_get_tunnel_signal_state(ti->tile, inwards);

			assert(type == SIGTYPE_NORMAL || (!inwards && type == SIGTYPE_PBS_ONEWAY));

			SpriteID sprite = GetCustomSignalSprite(GetRailTypeInfo(GetRailType(ti->tile)), ti->tile, type, variant, condition);
			uint image = SignalData[dd].image;
			if (sprite != 0) {
				sprite += image;
			} else {
				/* Normal electric signals are stored in a different sprite block than all other signals. */
				sprite = (type == SIGTYPE_NORMAL && variant == SIG_ELECTRIC) ? SPR_ORIGINAL_SIGNALS_BASE : SPR_SIGNALS_BASE - 16;
				sprite += (type == SIGTYPE_NORMAL ? SIGTYPE_NORMAL * 16 : SIGTYPE_PBS_ONEWAY * 16 + 64) + variant * 64 + image * 2 + condition;
			}

			bool side = (_settings_game.construction.train_signal_side +
					(_settings_game.vehicle.road_side != 0)) > 1;

			uint x = TileX(ti->tile) * TILE_SIZE + SignalData[dd].pos[inwards][side].x;
			uint y = TileY(ti->tile) * TILE_SIZE + SignalData[dd].pos[inwards][side].y;

			AddSortableSpriteToDraw(sprite, PAL_NONE, x, y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, ti->z);
		}
	}

	if (railtype_overlay != 0 && !catenary) StartSpriteCombine();

	AddSortableSpriteToDraw(image + 1, PAL_NONE, ti->x + TILE_SIZE - 1, ti->y + TILE_SIZE - 1, BB_data[0], BB_data[1], TILE_HEIGHT, ti->z, false, BB_data[2], BB_data[3], BB_Z_SEPARATOR);
	/* Draw railtype tunnel portal overlay if defined. */
	if (railtype_overlay != 0) AddSortableSpriteToDraw(railtype_overlay + tunnelbridge_direction, PAL_NONE, ti->x + TILE_SIZE - 1, ti->y + TILE_SIZE - 1, BB_data[0], BB_data[1], TILE_HEIGHT, ti->z, false, BB_data[2], BB_data[3], BB_Z_SEPARATOR);

	if (catenary || railtype_overlay != 0) EndSpriteCombine();

	/* Add helper BB for sprite sorting that separates the tunnel from things beside of it. */
	AddSortableSpriteToDraw(SPR_EMPTY_BOUNDING_BOX, PAL_NONE, ti->x,              ti->y,              BB_data[6], BB_data[7], TILE_HEIGHT, ti->z);
	AddSortableSpriteToDraw(SPR_EMPTY_BOUNDING_BOX, PAL_NONE, ti->x + BB_data[4], ti->y + BB_data[5], BB_data[6], BB_data[7], TILE_HEIGHT, ti->z);

	DrawBridgeMiddle(ti);
}

static void DrawTrainDepotGroundSprite (DiagDirection dir, SpriteID image_x,
	SpriteID image_y, PaletteID pal)
{
	switch (dir) {
		case DIAGDIR_NE: if (!IsInvisibilitySet (TO_BUILDINGS)) break; // else FALL THROUGH
		case DIAGDIR_SW: DrawGroundSprite (image_x, pal); break;
		case DIAGDIR_NW: if (!IsInvisibilitySet (TO_BUILDINGS)) break; // else FALL THROUGH
		case DIAGDIR_SE: DrawGroundSprite (image_y, pal); break;
		default: break;
	}
}

static void DrawTrainDepot(TileInfo *ti)
{
	assert(IsRailDepotTile(ti->tile));

	const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));

	uint32 palette = COMPANY_SPRITE_COLOUR(GetTileOwner(ti->tile));

	/* draw depot */

	if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

	DiagDirection dir = GetGroundDepotDirection (ti->tile);

	const DrawTileSprites *dts = IsInvisibilitySet(TO_BUILDINGS) ?
		/* Draw rail instead of depot */
		_depot_invisible_gfx_table : _depot_gfx_table;
	dts = &dts[dir];

	SpriteID image;
	if (rti->UsesOverlay()) {
		image = SPR_FLAT_GRASS_TILE;
	} else {
		image = dts->ground.sprite;
		if (image != SPR_FLAT_GRASS_TILE) image += rti->GetRailtypeSpriteOffset();
	}

	/* adjust ground tile for desert
	 * don't adjust for snow, because snow in depots looks weird */
	if (IsOnSnow(ti->tile) && _settings_game.game_creation.landscape == LT_TROPIC) {
		if (image != SPR_FLAT_GRASS_TILE) {
			image += rti->snow_offset; // tile with tracks
		} else {
			image = SPR_FLAT_SNOW_DESERT_TILE; // flat ground
		}
	}

	DrawGroundSprite (image, GroundSpritePaletteTransform (image, PAL_NONE, palette));

	if (rti->UsesOverlay()) {
		SpriteID ground = GetCustomRailSprite(rti, ti->tile, RTSG_GROUND);
		DrawTrainDepotGroundSprite (dir,
				ground + RTO_X, ground + RTO_Y, PAL_NONE);

		if (_settings_client.gui.show_track_reservation && HasDepotReservation(ti->tile)) {
			SpriteID overlay = GetCustomRailSprite(rti, ti->tile, RTSG_OVERLAY);
			DrawTrainDepotGroundSprite (dir,
					overlay + RTO_X, overlay + RTO_Y, PALETTE_CRASH);
		}
	} else {
		/* PBS debugging, draw reserved tracks darker */
		if (_game_mode != GM_MENU && _settings_client.gui.show_track_reservation && HasDepotReservation(ti->tile)) {
			DrawTrainDepotGroundSprite (dir,
					rti->base_sprites.single[TRACK_X], rti->base_sprites.single[TRACK_Y], PALETTE_CRASH);
		}
	}

	if (HasCatenaryDrawn (rti)) DrawCatenary (ti);

	int depot_sprite = GetCustomRailSprite(rti, ti->tile, RTSG_DEPOT);
	SpriteID relocation = depot_sprite != 0 ? depot_sprite - SPR_RAIL_DEPOT_SE_1 : rti->GetRailtypeSpriteOffset();
	DrawRailTileSeq(ti, dts, TO_BUILDINGS, relocation, 0, palette);
}

void DrawTrainDepotSprite(int x, int y, int dir, RailType railtype)
{
	const DrawTileSprites *dts = &_depot_gfx_table[dir];
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);
	SpriteID image = rti->UsesOverlay() ? SPR_FLAT_GRASS_TILE : dts->ground.sprite;
	uint32 offset = rti->GetRailtypeSpriteOffset();

	if (image != SPR_FLAT_GRASS_TILE) image += offset;
	PaletteID palette = COMPANY_SPRITE_COLOUR(_local_company);

	DrawSprite(image, PAL_NONE, x, y);

	if (rti->UsesOverlay()) {
		SpriteID ground = GetCustomRailSprite(rti, INVALID_TILE, RTSG_GROUND);

		switch (dir) {
			case DIAGDIR_SW: DrawSprite(ground + RTO_X, PAL_NONE, x, y); break;
			case DIAGDIR_SE: DrawSprite(ground + RTO_Y, PAL_NONE, x, y); break;
			default: break;
		}
	}

	int depot_sprite = GetCustomRailSprite(rti, INVALID_TILE, RTSG_DEPOT);
	if (depot_sprite != 0) offset = depot_sprite - SPR_RAIL_DEPOT_SE_1;

	DrawRailTileSeqInGUI(x, y, dts, offset, 0, palette);
}

static void DrawRoadDepot(TileInfo *ti)
{
	assert(IsRoadDepotTile(ti->tile));

	if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

	PaletteID palette = COMPANY_SPRITE_COLOUR(GetTileOwner(ti->tile));

	const DrawTileSprites *dts;
	if (HasTileRoadType(ti->tile, ROADTYPE_TRAM)) {
		dts =  &_tram_depot[GetGroundDepotDirection(ti->tile)];
	} else {
		dts =  &_road_depot[GetGroundDepotDirection(ti->tile)];
	}

	DrawGroundSprite(dts->ground.sprite, PAL_NONE);
	DrawOrigTileSeq(ti, dts, TO_BUILDINGS, palette);
}

/**
 * Draw the road depot sprite.
 * @param x   The x offset to draw at.
 * @param y   The y offset to draw at.
 * @param dir The direction the depot must be facing.
 * @param rt  The road type of the depot to draw.
 */
void DrawRoadDepotSprite(int x, int y, DiagDirection dir, RoadType rt)
{
	PaletteID palette = COMPANY_SPRITE_COLOUR(_local_company);
	const DrawTileSprites *dts = (rt == ROADTYPE_TRAM) ? &_tram_depot[dir] : &_road_depot[dir];

	DrawSprite(dts->ground.sprite, PAL_NONE, x, y);
	DrawOrigTileSeqInGUI(x, y, dts, palette);
}

static void DrawTile_Misc(TileInfo *ti)
{
	switch (GetTileSubtype(ti->tile)) {
		default: NOT_REACHED();

		case TT_MISC_CROSSING:
			DrawLevelCrossing(ti);
			break;

		case TT_MISC_AQUEDUCT:
			DrawAqueductRamp(ti);
			DrawBridgeMiddle(ti);
			break;

		case TT_MISC_TUNNEL:
			DrawTunnel(ti);
			break;

		case TT_MISC_DEPOT:
			if (IsRailDepot(ti->tile)) {
				DrawTrainDepot(ti);
			} else {
				DrawRoadDepot(ti);
			}
			break;
	}
}

static int GetSlopePixelZ_Misc(TileIndex tile, uint x, uint y)
{
	switch (GetTileSubtype(tile)) {
		case TT_MISC_AQUEDUCT: {
			int z;
			Slope tileh = GetTilePixelSlope(tile, &z);

			x &= 0xF;
			y &= 0xF;

			DiagDirection dir = GetTunnelBridgeDirection(tile);

			z += ApplyPixelFoundationToSlope(GetBridgeFoundation(tileh, DiagDirToAxis(dir)), &tileh);

			/* On the bridge ramp? */
			uint pos = (DiagDirToAxis(dir) == AXIS_X ? y : x);
			if (5 <= pos && pos <= 10) {
				return z + ((tileh == SLOPE_FLAT) ? GetBridgePartialPixelZ(dir, x, y) : TILE_HEIGHT);
			}

			return z + GetPartialPixelZ(x, y, tileh);
		}

		case TT_MISC_TUNNEL: {
			int z;
			Slope tileh = GetTilePixelSlope(tile, &z);

			x &= 0xF;
			y &= 0xF;

			/* In the tunnel entrance? */
			uint pos = (DiagDirToAxis(GetTunnelBridgeDirection(tile)) == AXIS_X ? y : x);
			if (5 <= pos && pos <= 10) return z;

			return z + GetPartialPixelZ(x, y, tileh);
		}

		default: // TT_MISC_CROSSING, TT_MISC_DEPOT
			return GetTileMaxPixelZ(tile);
	}
}


/**
 * Remove a tunnel from the game.
 * @param tile Tile containing one of the endpoints.
 * @param flags Command flags.
 * @return Succeeded or failed command.
 */
static CommandCost RemoveTunnel(TileIndex tile, DoCommandFlag flags)
{
	if (flags & DC_AUTO) return_cmd_error(STR_ERROR_MUST_DEMOLISH_TUNNEL_FIRST);

	if (_current_company != OWNER_WATER && _game_mode != GM_EDITOR) {
		if (GetTunnelTransportType(tile) == TRANSPORT_RAIL) {
			CommandCost ret = CheckOwnership(GetTileOwner(tile));
			if (ret.Failed()) return ret;
		} else {
			RoadTypes rts = GetRoadTypes(tile);
			Owner road_owner = _current_company;
			Owner tram_owner = _current_company;

			if (HasBit(rts, ROADTYPE_ROAD)) road_owner = GetRoadOwner(tile, ROADTYPE_ROAD);
			if (HasBit(rts, ROADTYPE_TRAM)) tram_owner = GetRoadOwner(tile, ROADTYPE_TRAM);

			/* We can remove unowned road and if the town allows it */
			if (road_owner == OWNER_TOWN && !(_settings_game.construction.extra_dynamite || _cheats.magic_bulldozer.value)) {
				CommandCost ret = CheckTileOwnership(tile);
				if (ret.Failed()) return ret;
			} else {
				if (road_owner == OWNER_NONE || road_owner == OWNER_TOWN) road_owner = _current_company;
				if (tram_owner == OWNER_NONE) tram_owner = _current_company;

				CommandCost ret = CheckOwnership(road_owner, tile);
				if (ret.Failed()) return ret;
				ret = CheckOwnership(tram_owner, tile);
				if (ret.Failed()) return ret;
			}
		}
	}

	TileIndex endtile = GetOtherTunnelEnd(tile);

	CommandCost ret = TunnelBridgeIsFree(tile, endtile);
	if (ret.Failed()) return ret;

	_build_tunnel_endtile = endtile;

	Town *t = NULL;
	if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR) {
		t = ClosestTownFromTile(tile); // town penalty rating

		/* Check if you are allowed to remove the tunnel owned by a town
		 * Removal depends on difficulty settings */
		CommandCost ret = CheckforTownRating(flags, t, TUNNELBRIDGE_REMOVE);
		if (ret.Failed()) return ret;
	}

	/* checks if the owner is town then decrease town rating by RATING_TUNNEL_BRIDGE_DOWN_STEP until
	 * you have a "Poor" (0) town rating */
	if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR) {
		ChangeTownRating(t, RATING_TUNNEL_BRIDGE_DOWN_STEP, RATING_TUNNEL_BRIDGE_MINIMUM, flags);
	}

	uint len = GetTunnelBridgeLength(tile, endtile) + 2; // Don't forget the end tiles.
	uint nsignals = GetTunnelTransportType(tile) != TRANSPORT_RAIL ? 0 :
		(maptile_has_tunnel_signals(tile) ? 1 : 0) + (maptile_has_tunnel_signals(endtile) ? 1 : 0);

	if (flags & DC_EXEC) {
		if (GetTunnelTransportType(tile) == TRANSPORT_RAIL) {
			/* We first need to request values before calling DoClearSquare */
			DiagDirection dir = GetTunnelBridgeDirection(tile);
			Track track = DiagDirToDiagTrack(dir);
			Owner owner = GetTileOwner(tile);

			Train *v1 = NULL;
			Train *v2 = NULL;

			if (HasTunnelHeadReservation(tile)) {
				v1 = GetTrainForReservation (tile, track, true);
			}

			if (HasTunnelHeadReservation(endtile)) {
				v2 = GetTrainForReservation (endtile, track, true);
			}

			if (Company::IsValidID(owner)) {
				Company::Get(owner)->infrastructure.rail[GetRailType(tile)] -= len * TUNNELBRIDGE_TRACKBIT_FACTOR;
				DirtyCompanyInfrastructureWindows(owner);
			}

			DoClearSquare(tile);
			DoClearSquare(endtile);

			/* cannot use INVALID_DIAGDIR for signal update because the tunnel doesn't exist anymore */
			AddSideToSignalBuffer(tile,    ReverseDiagDir(dir), owner);
			AddSideToSignalBuffer(endtile, dir,                 owner);

			YapfNotifyTrackLayoutChange();

			if (v1 != NULL) TryPathReserve(v1);
			if (v2 != NULL) TryPathReserve(v2);
		} else {
			RoadType rt;
			FOR_EACH_SET_ROADTYPE(rt, GetRoadTypes(tile)) {
				/* A full diagonal road tile has two road bits. */
				Company *c = Company::GetIfValid(GetRoadOwner(tile, rt));
				if (c != NULL) {
					c->infrastructure.road[rt] -= len * 2 * TUNNELBRIDGE_TRACKBIT_FACTOR;
					DirtyCompanyInfrastructureWindows(c->index);
				}
			}

			DoClearSquare(tile);
			DoClearSquare(endtile);
		}
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_CLEAR_TUNNEL] * len + _price[PR_CLEAR_SIGNALS] * nsignals);
}

static CommandCost RemoveTrainDepot(TileIndex tile, DoCommandFlag flags)
{
	if (_current_company != OWNER_WATER) {
		CommandCost ret = CheckTileOwnership(tile);
		if (ret.Failed()) return ret;
	}

	CommandCost ret = EnsureNoVehicleOnGround(tile);
	if (ret.Failed()) return ret;

	if (flags & DC_EXEC) {
		/* read variables before the depot is removed */
		DiagDirection dir = GetGroundDepotDirection(tile);
		Owner owner = GetTileOwner(tile);
		Train *v = NULL;

		if (HasDepotReservation(tile)) {
			v = GetTrainForReservation (tile, DiagDirToDiagTrack (dir), true);
		}

		Company::Get(owner)->infrastructure.rail[GetRailType(tile)]--;
		DirtyCompanyInfrastructureWindows(owner);

		delete Depot::GetByTile(tile);
		DoClearSquare(tile);
		AddSideToSignalBuffer(tile, dir, owner);
		YapfNotifyTrackLayoutChange();
		if (v != NULL) TryPathReserve(v, true);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_CLEAR_DEPOT_TRAIN]);
}

static CommandCost RemoveRoadDepot(TileIndex tile, DoCommandFlag flags)
{
	if (_current_company != OWNER_WATER) {
		CommandCost ret = CheckTileOwnership(tile);
		if (ret.Failed()) return ret;
	}

	CommandCost ret = EnsureNoVehicleOnGround(tile);
	if (ret.Failed()) return ret;

	if (flags & DC_EXEC) {
		Company *c = Company::GetIfValid(GetTileOwner(tile));
		if (c != NULL) {
			/* A road depot has two road bits. */
			c->infrastructure.road[FIND_FIRST_BIT(GetRoadTypes(tile))] -= 2;
			DirtyCompanyInfrastructureWindows(c->index);
		}

		delete Depot::GetByTile(tile);
		DoClearSquare(tile);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_CLEAR_DEPOT_ROAD]);
}

extern CommandCost RemoveRoad(TileIndex tile, DoCommandFlag flags, RoadBits pieces, RoadType rt, bool crossing_check, bool town_check = true);

static CommandCost ClearTile_Misc(TileIndex tile, DoCommandFlag flags)
{
	switch (GetTileSubtype(tile)) {
		default: NOT_REACHED();

		case TT_MISC_CROSSING: {
			RoadTypes rts = GetRoadTypes(tile);
			CommandCost ret(EXPENSES_CONSTRUCTION);

			if (flags & DC_AUTO) return_cmd_error(STR_ERROR_MUST_REMOVE_ROAD_FIRST);

			/* Must iterate over the roadtypes in a reverse manner because
			 * tram tracks must be removed before the road bits. */
			RoadType rt = ROADTYPE_TRAM;
			do {
				if (HasBit(rts, rt)) {
					CommandCost tmp_ret = RemoveRoad(tile, flags, GetCrossingRoadBits(tile), rt, false);
					if (tmp_ret.Failed()) return tmp_ret;
					ret.AddCost(tmp_ret);
				}
			} while (rt-- != ROADTYPE_ROAD);

			if (flags & DC_EXEC) {
				DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}
			return ret;
		}

		case TT_MISC_AQUEDUCT: {
			if (flags & DC_AUTO) return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);

			if (_current_company != OWNER_WATER && _game_mode != GM_EDITOR) {
				Owner owner = GetTileOwner(tile);
				if (owner != OWNER_NONE) {
					CommandCost ret = CheckOwnership(owner);
					if (ret.Failed()) return ret;
				}
			}

			TileIndex endtile = GetOtherBridgeEnd(tile);

			CommandCost ret = TunnelBridgeIsFree(tile, endtile);
			if (ret.Failed()) return ret;

			uint len = GetTunnelBridgeLength(tile, endtile) + 2; // Don't forget the end tiles.

			if (flags & DC_EXEC) {
				/* Update company infrastructure counts. */
				Owner owner = GetTileOwner(tile);
				if (Company::IsValidID(owner)) Company::Get(owner)->infrastructure.water -= len * TUNNELBRIDGE_TRACKBIT_FACTOR;
				DirtyCompanyInfrastructureWindows(owner);

				RemoveBridgeMiddleTiles(tile, endtile);
				DoClearSquare(tile);
				DoClearSquare(endtile);
			}

			return CommandCost(EXPENSES_CONSTRUCTION, len * _price[PR_CLEAR_AQUEDUCT]);
		}

		case TT_MISC_TUNNEL:
			return RemoveTunnel(tile, flags);

		case TT_MISC_DEPOT:
			if (flags & DC_AUTO) {
				if (!IsTileOwner(tile, _current_company)) {
					return_cmd_error(STR_ERROR_AREA_IS_OWNED_BY_ANOTHER);
				}
				return_cmd_error(STR_ERROR_BUILDING_MUST_BE_DEMOLISHED);
			}
			return IsRailDepot(tile) ? RemoveTrainDepot(tile, flags) : RemoveRoadDepot(tile, flags);
	}
}


static void GetTileDesc_Misc(TileIndex tile, TileDesc *td)
{
	switch (GetTileSubtype(tile)) {
		default: NOT_REACHED();

		case TT_MISC_CROSSING: {
			td->str = STR_LAI_ROAD_DESCRIPTION_ROAD_RAIL_LEVEL_CROSSING;

			RoadTypes rts = GetRoadTypes(tile);
			Owner road_owner = HasBit(rts, ROADTYPE_ROAD) ? GetRoadOwner(tile, ROADTYPE_ROAD) : INVALID_OWNER;
			Owner tram_owner = HasBit(rts, ROADTYPE_TRAM) ? GetRoadOwner(tile, ROADTYPE_TRAM) : INVALID_OWNER;
			Owner rail_owner = GetTileOwner(tile);

			td->rail_speed = GetRailTypeInfo(GetRailType(tile))->max_speed;

			Owner first_owner = (road_owner == INVALID_OWNER ? tram_owner : road_owner);
			bool mixed_owners = (tram_owner != INVALID_OWNER && tram_owner != first_owner) || (rail_owner != INVALID_OWNER && rail_owner != first_owner);

			if (mixed_owners) {
				/* Multiple owners */
				td->owner_type[0] = (rail_owner == INVALID_OWNER ? STR_NULL : STR_LAND_AREA_INFORMATION_RAIL_OWNER);
				td->owner[0] = rail_owner;
				td->owner_type[1] = (road_owner == INVALID_OWNER ? STR_NULL : STR_LAND_AREA_INFORMATION_ROAD_OWNER);
				td->owner[1] = road_owner;
				td->owner_type[2] = (tram_owner == INVALID_OWNER ? STR_NULL : STR_LAND_AREA_INFORMATION_TRAM_OWNER);
				td->owner[2] = tram_owner;
			} else {
				/* One to rule them all */
				td->owner[0] = first_owner;
			}

			break;
		}

		case TT_MISC_AQUEDUCT:
			td->str = STR_LAI_BRIDGE_DESCRIPTION_AQUEDUCT;
			td->owner[0] = GetTileOwner(tile);
			break;

		case TT_MISC_TUNNEL:
			td->owner[0] = GetTileOwner(tile);

			if (GetTunnelTransportType(tile) == TRANSPORT_RAIL) {
				td->str = STR_LAI_TUNNEL_DESCRIPTION_RAILROAD;
				td->rail_speed = GetRailTypeInfo(GetRailType(tile))->max_speed;
			} else {
				td->str = STR_LAI_TUNNEL_DESCRIPTION_ROAD;

				Owner road_owner = INVALID_OWNER;
				Owner tram_owner = INVALID_OWNER;
				RoadTypes rts = GetRoadTypes(tile);
				if (HasBit(rts, ROADTYPE_ROAD)) road_owner = GetRoadOwner(tile, ROADTYPE_ROAD);
				if (HasBit(rts, ROADTYPE_TRAM)) tram_owner = GetRoadOwner(tile, ROADTYPE_TRAM);

				/* Is there a mix of owners? */
				if ((tram_owner != INVALID_OWNER && tram_owner != td->owner[0]) ||
						(road_owner != INVALID_OWNER && road_owner != td->owner[0])) {
					uint i = 1;
					if (road_owner != INVALID_OWNER) {
						td->owner_type[i] = STR_LAND_AREA_INFORMATION_ROAD_OWNER;
						td->owner[i] = road_owner;
						i++;
					}
					if (tram_owner != INVALID_OWNER) {
						td->owner_type[i] = STR_LAND_AREA_INFORMATION_TRAM_OWNER;
						td->owner[i] = tram_owner;
					}
				}
			}

			break;

		case TT_MISC_DEPOT:
			td->owner[0] = GetTileOwner(tile);
			td->build_date = Depot::GetByTile(tile)->build_date;

			if (IsRailDepot(tile)) {
				td->str = STR_LAI_RAIL_DESCRIPTION_TRAIN_DEPOT;

				const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(tile));
				SetDParamX(td->dparam, 0, rti->strings.name);
				td->rail_speed = rti->max_speed;

				if (_settings_game.vehicle.train_acceleration_model != AM_ORIGINAL) {
					if (td->rail_speed > 0) {
						td->rail_speed = min(td->rail_speed, 61);
					} else {
						td->rail_speed = 61;
					}
				}
			} else {
				td->str = STR_LAI_ROAD_DESCRIPTION_ROAD_VEHICLE_DEPOT;
			}

			break;
	}
}


static TrackStatus GetTileRailwayStatus_Misc(TileIndex tile, DiagDirection side)
{
	switch (GetTileSubtype(tile)) {
		default: NOT_REACHED();

		case TT_MISC_CROSSING:
			return CombineTrackStatus(TrackBitsToTrackdirBits(GetCrossingRailBits(tile)), TRACKDIR_BIT_NONE);

		case TT_MISC_AQUEDUCT:
			return 0;

		case TT_MISC_TUNNEL: {
			if (GetTunnelTransportType(tile) != TRANSPORT_RAIL) return 0;

			DiagDirection dir = GetTunnelBridgeDirection(tile);
			if (side != INVALID_DIAGDIR && side != ReverseDiagDir(dir)) return 0;

			TrackdirBits trackdirs = TrackBitsToTrackdirBits(DiagDirToDiagTrackBits(dir));
			TrackdirBits red_signals;
			switch (maptile_get_tunnel_present_signals(tile)) {
				default: NOT_REACHED();

				case 0: red_signals = TRACKDIR_BIT_NONE; break;

				case 1:
					red_signals = maptile_get_tunnel_signal_state(tile, false) == SIGNAL_STATE_RED ? trackdirs :
						TrackdirToTrackdirBits(DiagDirToDiagTrackdir(dir));
					break;

				case 2:
					red_signals = maptile_get_tunnel_signal_state(tile, true) == SIGNAL_STATE_RED ? trackdirs :
						TrackdirToTrackdirBits(DiagDirToDiagTrackdir(ReverseDiagDir(dir)));
					break;
			}

			return CombineTrackStatus(trackdirs, red_signals);
		}

		case TT_MISC_DEPOT: {
			if (!IsRailDepot(tile)) return 0;

			DiagDirection dir = GetGroundDepotDirection(tile);
			if (side != INVALID_DIAGDIR && side != dir) return 0;
			return CombineTrackStatus(TrackBitsToTrackdirBits(DiagDirToDiagTrackBits(dir)), TRACKDIR_BIT_NONE);
		}
	}
}

static TrackStatus GetTileRoadStatus_Misc(TileIndex tile, uint sub_mode, DiagDirection side)
{
	switch (GetTileSubtype(tile)) {
		default: NOT_REACHED();

		case TT_MISC_CROSSING: {
			if ((GetRoadTypes(tile) & sub_mode) == 0) return 0;

			Axis axis = GetCrossingRoadAxis(tile);
			if (side != INVALID_DIAGDIR && axis != DiagDirToAxis(side)) return 0;

			TrackdirBits trackdirbits = TrackBitsToTrackdirBits(AxisToTrackBits(axis));
			return CombineTrackStatus(trackdirbits, IsCrossingBarred(tile) ? trackdirbits : TRACKDIR_BIT_NONE);
		}

		case TT_MISC_AQUEDUCT:
			return 0;

		case TT_MISC_TUNNEL: {
			TransportType transport_type = GetTunnelTransportType(tile);
			if (transport_type != TRANSPORT_ROAD || (GetRoadTypes(tile) & sub_mode) == 0) return 0;

			DiagDirection dir = GetTunnelBridgeDirection(tile);
			if (side != INVALID_DIAGDIR && side != ReverseDiagDir(dir)) return 0;
			return CombineTrackStatus(TrackBitsToTrackdirBits(DiagDirToDiagTrackBits(dir)), TRACKDIR_BIT_NONE);
		}

		case TT_MISC_DEPOT: {
			if (!IsRoadDepot(tile) || (GetRoadTypes(tile) & sub_mode) == 0) {
				return 0;
			}

			DiagDirection dir = GetGroundDepotDirection(tile);
			if (side != INVALID_DIAGDIR && side != dir) return 0;
			return CombineTrackStatus(TrackBitsToTrackdirBits(DiagDirToDiagTrackBits(dir)), TRACKDIR_BIT_NONE);
		}
	}
}

static TrackdirBits GetTileWaterwayStatus_Misc(TileIndex tile, DiagDirection side)
{
	if (!IsTileSubtype(tile, TT_MISC_AQUEDUCT)) return TRACKDIR_BIT_NONE;

	DiagDirection dir = GetTunnelBridgeDirection(tile);
	if (side != INVALID_DIAGDIR && side != ReverseDiagDir(dir)) return TRACKDIR_BIT_NONE;
	return TrackBitsToTrackdirBits(DiagDirToDiagTrackBits(dir));
}


static bool ClickTile_Misc(TileIndex tile)
{
	if (!IsGroundDepotTile(tile)) return false;

	ShowDepotWindow(tile, IsRailDepot(tile) ? VEH_TRAIN : VEH_ROAD);
	return true;
}


static void TileLoop_Misc(TileIndex tile)
{
	switch (_settings_game.game_creation.landscape) {
		case LT_ARCTIC: {
			int z = IsTileSubtype(tile, TT_MISC_AQUEDUCT) ? GetTileMaxZ(tile) : GetTileZ(tile);
			if (IsOnSnow(tile) != (z > GetSnowLine())) {
				ToggleSnow(tile);
				MarkTileDirtyByTile(tile);
			}
			break;
		}

		case LT_TROPIC:
			if (GetTropicZone(tile) == TROPICZONE_DESERT && !IsOnDesert(tile)) {
				SetDesert(tile, true);
				MarkTileDirtyByTile(tile);
			}
			break;
	}

	if (IsTileSubtype(tile, TT_MISC_CROSSING)) {
		const Town *t = ClosestTownFromTile(tile);
		UpdateRoadSide(tile, t != NULL ? GetTownRadiusGroup(t, tile) : HZB_TOWN_EDGE);
	}
}


static void ChangeTileOwner_Misc(TileIndex tile, Owner old_owner, Owner new_owner)
{
	switch (GetTileSubtype(tile)) {
		default: NOT_REACHED();

		case TT_MISC_CROSSING:
			for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
				/* Update all roadtypes, no matter if they are present */
				if (GetRoadOwner(tile, rt) == old_owner) {
					if (HasTileRoadType(tile, rt)) {
						/* A level crossing has two road bits. No need to dirty windows here, we'll redraw the whole screen anyway. */
						Company::Get(old_owner)->infrastructure.road[rt] -= 2;
						if (new_owner != INVALID_OWNER) Company::Get(new_owner)->infrastructure.road[rt] += 2;
					}

					SetRoadOwner(tile, rt, new_owner == INVALID_OWNER ? OWNER_NONE : new_owner);
				}
			}

			if (GetTileOwner(tile) == old_owner) {
				if (new_owner == INVALID_OWNER) {
					DoCommand(tile, 0, GetCrossingRailTrack(tile), DC_EXEC | DC_BANKRUPT, CMD_REMOVE_SINGLE_RAIL);
				} else {
					/* Update infrastructure counts. No need to dirty windows here, we'll redraw the whole screen anyway. */
					RailType rt = GetRailType(tile);
					Company::Get(old_owner)->infrastructure.rail[rt] -= LEVELCROSSING_TRACKBIT_FACTOR;
					Company::Get(new_owner)->infrastructure.rail[rt] += LEVELCROSSING_TRACKBIT_FACTOR;

					SetTileOwner(tile, new_owner);
				}
			}

			break;

		case TT_MISC_AQUEDUCT: {
			if (!IsTileOwner(tile, old_owner)) return;

			TileIndex other_end = GetOtherTunnelBridgeEnd(tile);
			/* Set number of pieces to zero if it's the southern tile as we
			 * don't want to update the infrastructure counts twice. */
			uint num_pieces = tile < other_end ? (GetTunnelBridgeLength(tile, other_end) + 2) * TUNNELBRIDGE_TRACKBIT_FACTOR : 0;

			/* Update company infrastructure counts.
			 * No need to dirty windows here, we'll redraw the whole screen anyway. */
			Company::Get(old_owner)->infrastructure.water -= num_pieces;
			if (new_owner != INVALID_OWNER) {
				Company::Get(new_owner)->infrastructure.water += num_pieces;
				SetTileOwner(tile, new_owner);
			} else {
				SetTileOwner(tile, OWNER_NONE);
			}
			break;
		}

		case TT_MISC_TUNNEL: {
			TileIndex other_end = GetOtherTunnelEnd(tile);
			/* Set number of pieces to zero if it's the southern tile as we
			 * don't want to update the infrastructure counts twice. */
			uint num_pieces = tile < other_end ? (GetTunnelBridgeLength(tile, other_end) + 2) * TUNNELBRIDGE_TRACKBIT_FACTOR : 0;

			if (GetTunnelTransportType(tile) != TRANSPORT_RAIL) {
				/* A full diagonal road tile has two road bits. */
				num_pieces *= 2;
				if (new_owner == INVALID_OWNER) new_owner = OWNER_NONE;

				for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
					/* Update all roadtypes, no matter if they are present */
					if (GetRoadOwner(tile, rt) == old_owner) {
						if (HasBit(GetRoadTypes(tile), rt)) {
							/* Update company infrastructure counts.
							 * No need to dirty windows here, we'll redraw the whole screen anyway. */
							Company::Get(old_owner)->infrastructure.road[rt] -= num_pieces;
							if (new_owner != OWNER_NONE) Company::Get(new_owner)->infrastructure.road[rt] += num_pieces;
						}

						SetRoadOwner(tile, rt, new_owner);
					}
				}

				if (IsTileOwner(tile, old_owner)) {
					SetTileOwner (tile, new_owner);
				}

			} else if (IsTileOwner(tile, old_owner)) {
				/* No need to dirty windows here, we'll redraw the whole screen anyway. */
				Company::Get(old_owner)->infrastructure.rail[GetRailType(tile)] -= num_pieces;

				if (new_owner != INVALID_OWNER) {
					Company::Get(new_owner)->infrastructure.rail[GetRailType(tile)] += num_pieces;
					SetTileOwner(tile, new_owner);
				} else {
					/* Since all of our vehicles have been removed,
					 * it is safe to remove the rail tunnel. */
					CommandCost ret = DoCommand(tile, 0, 0, DC_EXEC | DC_BANKRUPT, CMD_LANDSCAPE_CLEAR);
					assert(ret.Succeeded());
				}
			}

			break;
		}

		case TT_MISC_DEPOT:
			if (!IsTileOwner(tile, old_owner)) return;

			if (new_owner != INVALID_OWNER) {
				/* Update company infrastructure counts. No need to dirty windows here, we'll redraw the whole screen anyway. */
				if (IsRailDepot(tile)) {
					RailType rt = GetRailType(tile);
					Company::Get(old_owner)->infrastructure.rail[rt]--;
					Company::Get(new_owner)->infrastructure.rail[rt]++;
				} else {
					/* A road depot has two road bits. */
					RoadType rt = (RoadType)FIND_FIRST_BIT(GetRoadTypes(tile));
					Company::Get(old_owner)->infrastructure.road[rt] -= 2;
					Company::Get(new_owner)->infrastructure.road[rt] += 2;
				}

				SetTileOwner(tile, new_owner);
			} else {
				DoCommand(tile, 0, 0, DC_EXEC | DC_BANKRUPT, CMD_LANDSCAPE_CLEAR);
			}
			break;
	}
}


/**
 * Frame when a vehicle should be hidden in a tunnel with a certain direction.
 * This differs per direction, because of visibility / bounding box issues.
 * Note that direction, in this case, is the direction leading into the tunnel.
 * When entering a tunnel, hide the vehicle when it reaches the given frame.
 * When leaving a tunnel, show the vehicle when it is one frame further
 * to the 'outside', i.e. at (TILE_SIZE-1) - (frame) + 1
 */
extern const byte _tunnel_visibility_frame[DIAGDIR_END] = {12, 8, 8, 12};


static Foundation GetFoundation_Misc(TileIndex tile, Slope tileh)
{
	switch (GetTileSubtype(tile)) {
		case TT_MISC_AQUEDUCT: return GetBridgeFoundation(tileh, DiagDirToAxis(GetTunnelBridgeDirection(tile)));
		case TT_MISC_TUNNEL:   return FOUNDATION_NONE;
		default:               return FlatteningFoundation(tileh);
	}
}


static CommandCost TerraformTile_Misc(TileIndex tile, DoCommandFlag flags, int z_new, Slope tileh_new)
{
	if (_settings_game.construction.build_on_slopes && AutoslopeEnabled()) {
		switch (GetTileSubtype(tile)) {
			default: break;

			case TT_MISC_CROSSING:
				if (!IsSteepSlope(tileh_new) && (GetTileMaxZ(tile) == z_new + GetSlopeMaxZ(tileh_new)) && HasBit(VALID_LEVEL_CROSSING_SLOPES, tileh_new)) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
				break;

			case TT_MISC_DEPOT:
				if (AutoslopeCheckForEntranceEdge(tile, z_new, tileh_new, GetGroundDepotDirection(tile))) {
					return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
				}
				break;
		}
	}

	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
}


extern const TileTypeProcs _tile_type_misc_procs = {
	DrawTile_Misc,           // draw_tile_proc
	GetSlopePixelZ_Misc,     // get_slope_z_proc
	ClearTile_Misc,          // clear_tile_proc
	NULL,                    // add_accepted_cargo_proc
	GetTileDesc_Misc,        // get_tile_desc_proc
	GetTileRailwayStatus_Misc,  // get_tile_railway_status_proc
	GetTileRoadStatus_Misc,     // get_tile_road_status_proc
	GetTileWaterwayStatus_Misc, // get_tile_waterway_status_proc
	ClickTile_Misc,          // click_tile_proc
	NULL,                    // animate_tile_proc
	TileLoop_Misc,           // tile_loop_proc
	ChangeTileOwner_Misc,    // change_tile_owner_proc
	NULL,                    // add_produced_cargo_proc
	GetFoundation_Misc,      // get_foundation_proc
	TerraformTile_Misc,      // terraform_tile_proc
};
