/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file roadveh_cmd.cpp Handling of road vehicles. */

#include "stdafx.h"
#include "roadveh.h"
#include "command_func.h"
#include "news_func.h"
#include "station_base.h"
#include "company_func.h"
#include "articulated_vehicles.h"
#include "newgrf_sound.h"
#include "pathfinder/yapf/yapf.h"
#include "strings_func.h"
#include "map/road.h"
#include "map/depot.h"
#include "map/tunnelbridge.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "ai/ai.hpp"
#include "game/game.hpp"
#include "effectvehicle_func.h"
#include "roadstop_base.h"
#include "spritecache.h"
#include "core/random_func.hpp"
#include "company_base.h"
#include "core/backup_type.hpp"
#include "newgrf.h"
#include "zoom_func.h"
#include "bridge.h"
#include "station_func.h"

#include "table/strings.h"

static const uint16 _roadveh_images[] = {
	0xCD4, 0xCDC, 0xCE4, 0xCEC, 0xCF4, 0xCFC, 0xD0C, 0xD14,
	0xD24, 0xD1C, 0xD2C, 0xD04, 0xD1C, 0xD24, 0xD6C, 0xD74,
	0xD7C, 0xC14, 0xC1C, 0xC24, 0xC2C, 0xC34, 0xC3C, 0xC4C,
	0xC54, 0xC64, 0xC5C, 0xC6C, 0xC44, 0xC5C, 0xC64, 0xCAC,
	0xCB4, 0xCBC, 0xD94, 0xD9C, 0xDA4, 0xDAC, 0xDB4, 0xDBC,
	0xDCC, 0xDD4, 0xDE4, 0xDDC, 0xDEC, 0xDC4, 0xDDC, 0xDE4,
	0xE2C, 0xE34, 0xE3C, 0xC14, 0xC1C, 0xC2C, 0xC3C, 0xC4C,
	0xC5C, 0xC64, 0xC6C, 0xC74, 0xC84, 0xC94, 0xCA4
};

static const uint16 _roadveh_full_adder[] = {
	 0,  88,   0,   0,   0,   0,  48,  48,
	48,  48,   0,   0,  64,  64,   0,  16,
	16,   0,  88,   0,   0,   0,   0,  48,
	48,  48,  48,   0,   0,  64,  64,   0,
	16,  16,   0,  88,   0,   0,   0,   0,
	48,  48,  48,  48,   0,   0,  64,  64,
	 0,  16,  16,   0,   8,   8,   8,   8,
	 0,   0,   0,   8,   8,   8,   8
};
assert_compile(lengthof(_roadveh_images) == lengthof(_roadveh_full_adder));

template <>
bool IsValidImageIndex<VEH_ROAD>(uint8 image_index)
{
	return image_index < lengthof(_roadveh_images);
}

static const Trackdir _road_reverse_table[DIAGDIR_END] = {
	TRACKDIR_RVREV_NE, TRACKDIR_RVREV_SE, TRACKDIR_RVREV_SW, TRACKDIR_RVREV_NW
};


/**
 * Check whether a roadvehicle is a bus
 * @return true if bus
 */
bool RoadVehicle::IsBus() const
{
	assert(this->IsFrontEngine());
	return IsCargoInClass(this->cargo_type, CC_PASSENGERS);
}

/**
 * Get the width of a road vehicle image in the GUI.
 * @param offset Additional offset for positioning the sprite; set to NULL if not needed
 * @return Width in pixels
 */
int RoadVehicle::GetDisplayImageWidth(Point *offset) const
{
	int reference_width = ROADVEHINFO_DEFAULT_VEHICLE_WIDTH;

	if (offset != NULL) {
		offset->x = UnScaleByZoom(2 * reference_width, ZOOM_LVL_GUI);
		offset->y = 0;
	}
	return UnScaleByZoom(4 * this->gcache.cached_veh_length * reference_width / VEHICLE_LENGTH, ZOOM_LVL_GUI);
}

static SpriteID GetRoadVehIcon(EngineID engine, EngineImageType image_type)
{
	const Engine *e = Engine::Get(engine);
	uint8 spritenum = e->u.road.image_index;

	if (is_custom_sprite(spritenum)) {
		SpriteID sprite = GetCustomVehicleIcon(engine, DIR_W, image_type);
		if (sprite != 0) return sprite;

		spritenum = e->original_image_index;
	}

	assert(IsValidImageIndex<VEH_ROAD>(spritenum));
	return DIR_W + _roadveh_images[spritenum];
}

SpriteID RoadVehicle::GetImage(Direction direction, EngineImageType image_type) const
{
	uint8 spritenum = this->spritenum;
	SpriteID sprite;

	if (is_custom_sprite(spritenum)) {
		sprite = GetCustomVehicleSprite(this, (Direction)(direction + 4 * IS_CUSTOM_SECONDHEAD_SPRITE(spritenum)), image_type);
		if (sprite != 0) return sprite;

		spritenum = this->GetEngine()->original_image_index;
	}

	assert(IsValidImageIndex<VEH_ROAD>(spritenum));
	sprite = direction + _roadveh_images[spritenum];

	if (this->cargo.StoredCount() >= this->cargo_cap / 2U) sprite += _roadveh_full_adder[spritenum];

	return sprite;
}

/**
 * Draw a road vehicle engine.
 * @param left Left edge to draw within.
 * @param right Right edge to draw within.
 * @param preferred_x Preferred position of the engine.
 * @param y Vertical position of the engine.
 * @param engine Engine to draw
 * @param pal Palette to use.
 */
void DrawRoadVehEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type)
{
	SpriteID sprite = GetRoadVehIcon(engine, image_type);
	const Sprite *real_sprite = GetSprite(sprite, ST_NORMAL);
	preferred_x = Clamp(preferred_x, left - UnScaleByZoom(real_sprite->x_offs, ZOOM_LVL_GUI), right - UnScaleByZoom(real_sprite->width, ZOOM_LVL_GUI) - UnScaleByZoom(real_sprite->x_offs, ZOOM_LVL_GUI));
	DrawSprite(sprite, pal, preferred_x, y);
}

/**
 * Get the size of the sprite of a road vehicle sprite heading west (used for lists).
 * @param engine The engine to get the sprite from.
 * @param[out] width The width of the sprite.
 * @param[out] height The height of the sprite.
 * @param[out] xoffs Number of pixels to shift the sprite to the right.
 * @param[out] yoffs Number of pixels to shift the sprite downwards.
 * @param image_type Context the sprite is used in.
 */
void GetRoadVehSpriteSize(EngineID engine, uint &width, uint &height, int &xoffs, int &yoffs, EngineImageType image_type)
{
	const Sprite *spr = GetSprite(GetRoadVehIcon(engine, image_type), ST_NORMAL);

	width  = UnScaleByZoom(spr->width, ZOOM_LVL_GUI);
	height = UnScaleByZoom(spr->height, ZOOM_LVL_GUI);
	xoffs  = UnScaleByZoom(spr->x_offs, ZOOM_LVL_GUI);
	yoffs  = UnScaleByZoom(spr->y_offs, ZOOM_LVL_GUI);
}

/**
 * Get length of a road vehicle.
 * @param v Road vehicle to query length.
 * @return Length of the given road vehicle.
 */
static uint GetRoadVehLength(const RoadVehicle *v)
{
	const Engine *e = v->GetEngine();
	uint length = VEHICLE_LENGTH;

	uint16 veh_len = CALLBACK_FAILED;
	if (e->GetGRF() != NULL && e->GetGRF()->grf_version >= 8) {
		/* Use callback 36 */
		veh_len = GetVehicleProperty(v, PROP_ROADVEH_SHORTEN_FACTOR, CALLBACK_FAILED);
		if (veh_len != CALLBACK_FAILED && veh_len >= VEHICLE_LENGTH) ErrorUnknownCallbackResult(e->GetGRFID(), CBID_VEHICLE_LENGTH, veh_len);
	} else {
		/* Use callback 11 */
		veh_len = GetVehicleCallback(CBID_VEHICLE_LENGTH, 0, 0, v->engine_type, v);
	}
	if (veh_len == CALLBACK_FAILED) veh_len = e->u.road.shorten_factor;
	if (veh_len != 0) {
		length -= Clamp(veh_len, 0, VEHICLE_LENGTH - 1);
	}

	return length;
}

/**
 * Update the cache of a road vehicle.
 * @param v Road vehicle needing an update of its cache.
 * @param same_length should length of vehicles stay the same?
 * @pre \a v must be first road vehicle.
 */
void RoadVehUpdateCache(RoadVehicle *v, bool same_length)
{
	assert(v->type == VEH_ROAD);
	assert(v->IsFrontEngine());

	v->InvalidateNewGRFCacheOfChain();

	v->gcache.cached_total_length = 0;

	for (RoadVehicle *u = v; u != NULL; u = u->Next()) {
		/* Check the v->first cache. */
		assert(u->First() == v);

		/* Update the 'first engine' */
		u->gcache.first_engine = (v == u) ? INVALID_ENGINE : v->engine_type;

		/* Update the length of the vehicle. */
		uint veh_len = GetRoadVehLength(u);
		/* Verify length hasn't changed. */
		if (same_length && veh_len != u->gcache.cached_veh_length) VehicleLengthChanged(u);

		u->gcache.cached_veh_length = veh_len;
		v->gcache.cached_total_length += u->gcache.cached_veh_length;

		/* Update visual effect */
		u->UpdateVisualEffect();

		/* Update cargo aging period. */
		u->vcache.cached_cargo_age_period = GetVehicleProperty(u, PROP_ROADVEH_CARGO_AGE_PERIOD, EngInfo(u->engine_type)->cargo_age_period);
	}

	uint max_speed = GetVehicleProperty(v, PROP_ROADVEH_SPEED, 0);
	v->vcache.cached_max_speed = (max_speed != 0) ? max_speed * 4 : RoadVehInfo(v->engine_type)->max_speed;
}

/**
 * Build a road vehicle.
 * @param tile     tile of the depot where road vehicle is built.
 * @param flags    type of operation.
 * @param e        the engine to build.
 * @param data     unused.
 * @param ret[out] the vehicle that has been built.
 * @return the cost of this operation or an error.
 */
CommandCost CmdBuildRoadVehicle(TileIndex tile, DoCommandFlag flags, const Engine *e, uint16 data, Vehicle **ret)
{
	if (HasTileRoadType(tile, ROADTYPE_TRAM) != HasBit(e->info.misc_flags, EF_ROAD_TRAM)) return_cmd_error(STR_ERROR_DEPOT_WRONG_DEPOT_TYPE);

	if (flags & DC_EXEC) {
		const RoadVehicleInfo *rvi = &e->u.road;

		RoadVehicle *v = new RoadVehicle();
		*ret = v;
		v->direction = DiagDirToDir(GetGroundDepotDirection(tile));
		v->owner = _current_company;

		v->tile = tile;
		int x = TileX(tile) * TILE_SIZE + TILE_SIZE / 2;
		int y = TileY(tile) * TILE_SIZE + TILE_SIZE / 2;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = GetSlopePixelZ(x, y);

		v->state = RVSB_IN_DEPOT;
		v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;

		v->spritenum = rvi->image_index;
		v->cargo_type = e->GetDefaultCargoType();
		v->cargo_cap = rvi->capacity;
		v->refit_cap = 0;

		v->last_station_visited = INVALID_STATION;
		v->last_loading_station = INVALID_STATION;
		v->engine_type = e->index;
		v->gcache.first_engine = INVALID_ENGINE; // needs to be set before first callback

		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->GetLifeLengthInDays();
		_new_vehicle_id = v->index;

		v->SetServiceInterval(Company::Get(v->owner)->settings.vehicle.servint_roadveh);

		v->date_of_last_service = _date;
		v->build_year = _cur_year;

		v->cur_image = SPR_IMG_QUERY;
		v->random_bits = VehicleRandomBits();
		v->SetFrontEngine();

		v->roadtype = HasBit(e->info.misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD;
		v->compatible_roadtypes = RoadTypeToRoadTypes(v->roadtype);
		v->gcache.cached_veh_length = VEHICLE_LENGTH;

		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) SetBit(v->vehicle_flags, VF_BUILT_AS_PROTOTYPE);
		v->SetServiceIntervalIsPercent(Company::Get(_current_company)->settings.vehicle.servint_ispercent);

		AddArticulatedParts(v);
		v->InvalidateNewGRFCacheOfChain();

		/* Call various callbacks after the whole consist has been constructed */
		for (RoadVehicle *u = v; u != NULL; u = u->Next()) {
			u->cargo_cap = u->GetEngine()->DetermineCapacity(u);
			u->refit_cap = 0;
			v->InvalidateNewGRFCache();
			u->InvalidateNewGRFCache();
		}
		RoadVehUpdateCache(v);
		/* Initialize cached values for realistic acceleration. */
		if (_settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL) v->CargoChanged();

		v->UpdatePosition();

		CheckConsistencyOfArticulatedVehicle(v);
	}

	return CommandCost();
}

static TileIndex FindClosestRoadDepot(const RoadVehicle *v, bool nearby)
{
	if (IsRoadDepotTile(v->tile) && v->state == DiagDirToDiagTrackdir(ReverseDiagDir(GetGroundDepotDirection(v->tile)))) {
		return v->tile;
	}

	return YapfRoadVehicleFindNearestDepot(v,
		nearby ? _settings_game.pf.yapf.maximum_go_to_depot_penalty : 0);
}

bool RoadVehicle::FindClosestDepot(TileIndex *location, DestinationID *destination, bool *reverse)
{
	TileIndex rfdd = FindClosestRoadDepot(this, false);
	if (rfdd == INVALID_TILE) return false;

	if (location    != NULL) *location    = rfdd;
	if (destination != NULL) *destination = GetDepotIndex(rfdd);

	return true;
}

/**
 * Turn a roadvehicle around.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 vehicle ID to turn
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdTurnRoadVeh(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	RoadVehicle *v = RoadVehicle::GetIfValid(p1);
	if (v == NULL) return CMD_ERROR;

	if (!v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if ((v->vehstatus & VS_STOPPED) ||
			(v->vehstatus & VS_CRASHED) ||
			v->breakdown_ctr != 0 ||
			v->overtaking != 0 ||
			v->state == RVSB_WORMHOLE ||
			v->IsInDepot() ||
			v->current_order.IsType(OT_LOADING)) {
		return CMD_ERROR;
	}

	if (IsNormalRoadTile(v->tile) && GetDisallowedRoadDirections(v->tile) != DRD_NONE) return CMD_ERROR;

	if ((IsTunnelTile(v->tile) || IsBridgeHeadTile(v->tile)) && DirToDiagDir(v->direction) == GetTunnelBridgeDirection(v->tile)) return CMD_ERROR;

	if (flags & DC_EXEC) v->reverse_ctr = 180;

	return CommandCost();
}


void RoadVehicle::MarkDirty()
{
	for (RoadVehicle *v = this; v != NULL; v = v->Next()) {
		v->colourmap = PAL_NONE;
		v->UpdateViewport(true, false);
	}
	this->CargoChanged();
}

void RoadVehicle::UpdateDeltaXY(Direction direction)
{
	static const int8 _delta_xy_table[8][10] = {
		/* y_extent, x_extent, y_offs, x_offs, y_bb_offs, x_bb_offs, y_extent_shorten, x_extent_shorten, y_bb_offs_shorten, x_bb_offs_shorten */
		{3, 3, -1, -1,  0,  0, -1, -1, -1, -1}, // N
		{3, 7, -1, -3,  0, -1,  0, -1,  0,  0}, // NE
		{3, 3, -1, -1,  0,  0,  1, -1,  1, -1}, // E
		{7, 3, -3, -1, -1,  0,  0,  0,  1,  0}, // SE
		{3, 3, -1, -1,  0,  0,  1,  1,  1,  1}, // S
		{3, 7, -1, -3,  0, -1,  0,  0,  0,  1}, // SW
		{3, 3, -1, -1,  0,  0, -1,  1, -1,  1}, // W
		{7, 3, -3, -1, -1,  0, -1,  0,  0,  0}, // NW
	};

	int shorten = VEHICLE_LENGTH - this->gcache.cached_veh_length;
	if (!IsDiagonalDirection(direction)) shorten >>= 1;

	const int8 *bb = _delta_xy_table[direction];
	this->x_bb_offs     = bb[5] + bb[9] * shorten;
	this->y_bb_offs     = bb[4] + bb[8] * shorten;;
	this->x_offs        = bb[3];
	this->y_offs        = bb[2];
	this->x_extent      = bb[1] + bb[7] * shorten;
	this->y_extent      = bb[0] + bb[6] * shorten;
	this->z_extent      = 6;
}

/**
 * Calculates the maximum speed of the vehicle under its current conditions.
 * @return Maximum speed of the vehicle.
 */
inline int RoadVehicle::GetCurrentMaxSpeed() const
{
	int max_speed = this->vcache.cached_max_speed;

	/* Limit speed to 50% while reversing, 75% in curves. */
	for (const RoadVehicle *u = this; u != NULL; u = u->Next()) {
		if (_settings_game.vehicle.roadveh_acceleration_model == AM_REALISTIC) {
			if (this->state <= RVSB_TRACKDIR_MASK && IsReversingRoadTrackdir((Trackdir)this->state)) {
				max_speed = this->vcache.cached_max_speed / 2;
				break;
			} else if ((u->direction & 1) == 0) {
				max_speed = this->vcache.cached_max_speed * 3 / 4;
			}
		}

		/* Vehicle is on the middle part of a bridge. */
		if (u->state == RVSB_WORMHOLE && !(u->vehstatus & VS_HIDDEN)) {
			max_speed = min(max_speed, GetBridgeSpec(GetRoadBridgeType(u->tile))->speed * 2);
		}
	}

	return min(max_speed, this->current_order.GetMaxSpeed() * 2);
}

/**
 * Delete last vehicle of a chain road vehicles.
 * @param v First roadvehicle.
 */
static void DeleteLastRoadVeh(RoadVehicle *v)
{
	RoadVehicle *first = v->First();
	Vehicle *u = v;
	for (; v->Next() != NULL; v = v->Next()) u = v;
	u->SetNext(NULL);
	v->last_station_visited = first->last_station_visited; // for PreDestructor

	/* Only leave the road stop when we're really gone. */
	if (IsInsideMM(v->state, RVSB_IN_ROAD_STOP, RVSB_IN_ROAD_STOP_END)) RoadStop::GetByTile(v->tile, GetRoadStopType(v->tile))->Leave(v);

	delete v;
}

static void RoadVehSetRandomDirection(RoadVehicle *v)
{
	static const DirDiff delta[] = {
		DIRDIFF_45LEFT, DIRDIFF_SAME, DIRDIFF_SAME, DIRDIFF_45RIGHT
	};

	do {
		uint32 r = Random();

		v->direction = ChangeDir(v->direction, delta[r & 3]);
		v->UpdateViewport(true, true);
	} while ((v = v->Next()) != NULL);
}

/**
 * Road vehicle chain has crashed.
 * @param v First roadvehicle.
 * @return whether the chain still exists.
 */
static bool RoadVehIsCrashed(RoadVehicle *v)
{
	v->crashed_ctr++;
	if (v->crashed_ctr == 2) {
		CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);
	} else if (v->crashed_ctr <= 45) {
		if ((v->tick_counter & 7) == 0) RoadVehSetRandomDirection(v);
	} else if (v->crashed_ctr >= 2220 && !(v->tick_counter & 0x1F)) {
		bool ret = v->Next() != NULL;
		DeleteLastRoadVeh(v);
		return ret;
	}

	return true;
}

/**
 * Check routine whether a road and a train vehicle have collided.
 * @param v    %Train vehicle to test.
 * @param data Road vehicle to test.
 * @return %Train vehicle if the vehicles collided, else \c NULL.
 */
static Vehicle *EnumCheckRoadVehCrashTrain(Vehicle *v, void *data)
{
	const Vehicle *u = (Vehicle*)data;

	return (v->type == VEH_TRAIN &&
			abs(v->z_pos - u->z_pos) <= 6 &&
			abs(v->x_pos - u->x_pos) <= 4 &&
			abs(v->y_pos - u->y_pos) <= 4) ? v : NULL;
}

uint RoadVehicle::Crash(bool flooded)
{
	uint pass = this->GroundVehicleBase::Crash(flooded);
	if (this->IsFrontEngine()) {
		pass += 1; // driver

		/* If we're in a drive through road stop we ought to leave it */
		if (IsInsideMM(this->state, RVSB_IN_DT_ROAD_STOP, RVSB_IN_DT_ROAD_STOP_END)) {
			RoadStop::GetByTile(this->tile, GetRoadStopType(this->tile))->Leave(this);
		}
	}
	this->crashed_ctr = flooded ? 2000 : 1; // max 2220, disappear pretty fast when flooded
	return pass;
}

static void RoadVehCrash(RoadVehicle *v)
{
	uint pass = v->Crash();

	AI::NewEvent(v->owner, new ScriptEventVehicleCrashed(v->index, v->tile, ScriptEventVehicleCrashed::CRASH_RV_LEVEL_CROSSING));
	Game::NewEvent(new ScriptEventVehicleCrashed(v->index, v->tile, ScriptEventVehicleCrashed::CRASH_RV_LEVEL_CROSSING));

	SetDParam(0, pass);
	AddVehicleNewsItem(
		(pass == 1) ?
			STR_NEWS_ROAD_VEHICLE_CRASH_DRIVER : STR_NEWS_ROAD_VEHICLE_CRASH,
		NT_ACCIDENT,
		v->index
	);

	ModifyStationRatingAround(v->tile, v->owner, -160, 22);
	if (_settings_client.sound.disaster) SndPlayVehicleFx(SND_12_EXPLOSION, v);
}

static bool RoadVehCheckTrainCrash(RoadVehicle *v)
{
	for (RoadVehicle *u = v; u != NULL; u = u->Next()) {
		if (u->state == RVSB_WORMHOLE) continue;

		TileIndex tile = u->tile;

		if (!IsLevelCrossingTile(tile)) continue;

		if (HasVehicleOnPosXY(v->x_pos, v->y_pos, u, EnumCheckRoadVehCrashTrain)) {
			RoadVehCrash(v);
			return true;
		}
	}

	return false;
}

TileIndex RoadVehicle::GetOrderStationLocation(StationID station)
{
	if (station == this->last_station_visited) this->last_station_visited = INVALID_STATION;

	const Station *st = Station::Get(station);
	if (!CanVehicleUseStation(this, st)) {
		/* There is no stop left at the station, so don't even TRY to go there */
		this->IncrementRealOrderIndex();
		return 0;
	}

	return st->xy;
}

static void StartRoadVehSound(const RoadVehicle *v)
{
	if (!PlayVehicleSound(v, VSE_START)) {
		SoundID s = RoadVehInfo(v->engine_type)->sfx;
		if (s == SND_19_BUS_START_PULL_AWAY && (v->tick_counter & 3) == 0) {
			s = SND_1A_BUS_START_PULL_AWAY_WITH_HORN;
		}
		SndPlayVehicleFx(s, v);
	}
}

struct RoadVehFindData {
	int x;
	int y;
	const Vehicle *veh;
	Vehicle *best;
	uint best_diff;
	Direction dir;
};

static void RoadVehFindCloseToCheck (RoadVehFindData *rvf, Vehicle *v)
{
	static const int8 dist_x[] = { -4, -8, -4, -1, 4, 8, 4, 1 };
	static const int8 dist_y[] = { -4, -1, 4, 8, 4, 1, -4, -8 };

	if (v->type == VEH_ROAD &&
			!v->IsInDepot() &&
			abs(v->z_pos - rvf->veh->z_pos) < 6 &&
			v->direction == rvf->dir &&
			rvf->veh->First() != v->First()) {

		short x_diff = v->x_pos - rvf->x;
		short y_diff = v->y_pos - rvf->y;

		if ((dist_x[v->direction] > 0 ?
					(x_diff >= 0 && x_diff < dist_x[v->direction]) :
					(x_diff <= 0 && x_diff > dist_x[v->direction])) &&
				(dist_y[v->direction] > 0 ?
					(y_diff >= 0 && y_diff < dist_y[v->direction]) :
					(y_diff <= 0 && y_diff > dist_y[v->direction]))) {
			uint diff = abs(x_diff) + abs(y_diff);

			if (diff < rvf->best_diff || (diff == rvf->best_diff && v->index < rvf->best->index)) {
				rvf->best = v;
				rvf->best_diff = diff;
			}
		}
	}
}

static Vehicle *EnumCheckRoadVehClose(Vehicle *v, void *data)
{
	RoadVehFindCloseToCheck ((RoadVehFindData*)data, v);

	return NULL;
}

static RoadVehicle *RoadVehFindCloseTo(RoadVehicle *v, int x, int y, Direction dir, bool update_blocked_ctr = true)
{
	RoadVehFindData rvf;
	RoadVehicle *front = v->First();

	if (front->reverse_ctr != 0) return NULL;

	rvf.x = x;
	rvf.y = y;
	rvf.dir = dir;
	rvf.veh = v;
	rvf.best_diff = UINT_MAX;

	if (front->state == RVSB_WORMHOLE) {
		VehicleTileIterator iter1 (v->tile);
		while (!iter1.finished()) {
			RoadVehFindCloseToCheck (&rvf, iter1.next());
		}
		VehicleTileIterator iter2 (GetOtherTunnelBridgeEnd(v->tile));
		while (!iter2.finished()) {
			RoadVehFindCloseToCheck (&rvf, iter2.next());
		}
	} else {
		FindVehicleOnPosXY(x, y, &rvf, EnumCheckRoadVehClose);
	}

	/* This code protects a roadvehicle from being blocked for ever
	 * If more than 1480 / 74 days a road vehicle is blocked, it will
	 * drive just through it. The ultimate backup-code of TTD.
	 * It can be disabled. */
	if (rvf.best_diff == UINT_MAX) {
		front->blocked_ctr = 0;
		return NULL;
	}

	if (update_blocked_ctr && ++front->blocked_ctr > 1480) return NULL;

	return RoadVehicle::From(rvf.best);
}

/**
 * A road vehicle arrives at a station. If it is the first time, create a news item.
 * @param v  Road vehicle that arrived.
 * @param st Station where the road vehicle arrived.
 */
static void RoadVehArrivesAt(const RoadVehicle *v, Station *st)
{
	if (v->IsBus()) {
		/* Check if station was ever visited before */
		if (!(st->had_vehicle_of_type & HVOT_BUS)) {
			st->had_vehicle_of_type |= HVOT_BUS;
			SetDParam(0, st->index);
			AddVehicleNewsItem(
				v->roadtype == ROADTYPE_ROAD ? STR_NEWS_FIRST_BUS_ARRIVAL : STR_NEWS_FIRST_PASSENGER_TRAM_ARRIVAL,
				(v->owner == _local_company) ? NT_ARRIVAL_COMPANY : NT_ARRIVAL_OTHER,
				v->index,
				st->index
			);
			AI::NewEvent(v->owner, new ScriptEventStationFirstVehicle(st->index, v->index));
			Game::NewEvent(new ScriptEventStationFirstVehicle(st->index, v->index));
		}
	} else {
		/* Check if station was ever visited before */
		if (!(st->had_vehicle_of_type & HVOT_TRUCK)) {
			st->had_vehicle_of_type |= HVOT_TRUCK;
			SetDParam(0, st->index);
			AddVehicleNewsItem(
				v->roadtype == ROADTYPE_ROAD ? STR_NEWS_FIRST_TRUCK_ARRIVAL : STR_NEWS_FIRST_CARGO_TRAM_ARRIVAL,
				(v->owner == _local_company) ? NT_ARRIVAL_COMPANY : NT_ARRIVAL_OTHER,
				v->index,
				st->index
			);
			AI::NewEvent(v->owner, new ScriptEventStationFirstVehicle(st->index, v->index));
			Game::NewEvent(new ScriptEventStationFirstVehicle(st->index, v->index));
		}
	}
}

/**
 * This function looks at the vehicle and updates its speed (cur_speed
 * and subspeed) variables. Furthermore, it returns the distance that
 * the vehicle can drive this tick. #Vehicle::GetAdvanceDistance() determines
 * the distance to drive before moving a step on the map.
 * @return distance to drive.
 */
int RoadVehicle::UpdateSpeed()
{
	switch (_settings_game.vehicle.roadveh_acceleration_model) {
		default: NOT_REACHED();
		case AM_ORIGINAL:
			return this->DoUpdateSpeed(this->overtaking != 0 ? 512 : 256, 0, this->GetCurrentMaxSpeed());

		case AM_REALISTIC:
			return this->DoUpdateSpeed(this->GetAcceleration() + (this->overtaking != 0 ? 256 : 0), this->GetAccelerationStatus() == AS_BRAKE ? 0 : 4, this->GetCurrentMaxSpeed());
	}
}


static Direction RoadVehGetNewDirection(const RoadVehicle *v, int x, int y)
{
	static const Direction _roadveh_new_dir[] = {
		DIR_N , DIR_NW, DIR_W , INVALID_DIR,
		DIR_NE, DIR_N , DIR_SW, INVALID_DIR,
		DIR_E , DIR_SE, DIR_S
	};

	x = x - v->x_pos + 1;
	y = y - v->y_pos + 1;

	if ((uint)x > 2 || (uint)y > 2) return v->direction;
	return _roadveh_new_dir[y * 4 + x];
}

static Direction RoadVehGetSlidingDirection(const RoadVehicle *v, int x, int y)
{
	Direction new_dir = RoadVehGetNewDirection(v, x, y);
	Direction old_dir = v->direction;
	DirDiff delta;

	if (new_dir == old_dir) return old_dir;
	delta = (DirDifference(new_dir, old_dir) > DIRDIFF_REVERSE ? DIRDIFF_45LEFT : DIRDIFF_45RIGHT);
	return ChangeDir(old_dir, delta);
}

struct OvertakeData {
	const RoadVehicle *u;
	const RoadVehicle *v;
	TileIndex tile;
	Trackdir trackdir;
};

/**
 * Check if overtaking is possible on a piece of track
 *
 * @param od Information about the tile and the involved vehicles
 * @return true if we have to abort overtaking
 */
static bool CheckRoadBlockedForOvertaking(OvertakeData *od)
{
	TrackStatus ts = GetTileRoadStatus(od->tile, od->v->compatible_roadtypes);
	TrackdirBits trackdirbits = TrackStatusToTrackdirBits(ts);
	TrackdirBits red_signals = TrackStatusToRedSignals(ts); // barred level crossing
	TrackBits trackbits = TrackdirBitsToTrackBits(trackdirbits);

	/* Track does not continue along overtaking direction || track has junction || levelcrossing is barred */
	if (!HasBit(trackdirbits, od->trackdir) || (trackbits & ~TRACK_BIT_CROSS) || (red_signals != TRACKDIR_BIT_NONE)) return true;

	/* Are there more vehicles on the tile except the two vehicles involved in overtaking */
	VehicleTileFinder iter (od->tile);
	while (!iter.finished()) {
		Vehicle *v = iter.next();
		if (v->type == VEH_ROAD && v->First() == v && v != od->u && v != od->v) {
			iter.set_found();
		}
	}
	return iter.was_found();
}

static void RoadVehCheckOvertake(RoadVehicle *v, RoadVehicle *u)
{
	OvertakeData od;

	od.v = v;
	od.u = u;

	if (u->vcache.cached_max_speed >= v->vcache.cached_max_speed &&
			!(u->vehstatus & VS_STOPPED) &&
			u->cur_speed != 0) {
		return;
	}

	/* Trams can't overtake other trams */
	if (v->roadtype == ROADTYPE_TRAM) return;

	/* Don't overtake in stations */
	if (IsStationTile(v->tile) || IsStationTile(u->tile)) return;

	/* For now, articulated road vehicles can't overtake anything. */
	if (v->HasArticulatedPart()) return;

	/* Vehicles are not driving in same direction || direction is not a diagonal direction */
	if (v->direction != u->direction || !(v->direction & 1)) return;

	/* Check if vehicle is in a road stop, depot, tunnel or bridge or not on a straight road */
	if (v->state >= RVSB_IN_ROAD_STOP || !IsStraightRoadTrackdir((Trackdir)(v->state & RVSB_TRACKDIR_MASK))) return;

	od.trackdir = DiagDirToDiagTrackdir(DirToDiagDir(v->direction));

	/* Are the current and the next tile suitable for overtaking?
	 *  - Does the track continue along od.trackdir
	 *  - No junctions
	 *  - No barred levelcrossing
	 *  - No other vehicles in the way
	 */
	od.tile = v->tile;
	if (CheckRoadBlockedForOvertaking(&od)) return;

	od.tile = v->tile + TileOffsByDiagDir(DirToDiagDir(v->direction));
	if (CheckRoadBlockedForOvertaking(&od)) return;

	/* When the vehicle in front of us is stopped we may only take
	 * half the time to pass it than when the vehicle is moving. */
	v->overtaking_ctr = (od.u->cur_speed == 0 || (od.u->vehstatus & VS_STOPPED)) ? RV_OVERTAKE_TIMEOUT / 2 : 0;
	v->overtaking = 1;
}

static void controller_set_pos (RoadVehicle *v, int x, int y, bool new_tile, bool update_delta)
{
	v->x_pos = x;
	v->y_pos = y;
	v->UpdatePosition();

	int old_z = v->UpdateInclination (new_tile, update_delta);

	if (old_z == v->z_pos || _settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL) return;

	if (old_z < v->z_pos) {
		v->cur_speed = v->cur_speed * 232 / 256; // slow down by ~10%
	} else {
		uint16 spd = v->cur_speed + 2;
		if (spd <= v->vcache.cached_max_speed) v->cur_speed = spd;
	}
}

static int PickRandomBit(uint bits)
{
	uint i;
	uint num = RandomRange(CountBits(bits));

	for (i = 0; !(bits & 1) || (int)--num >= 0; bits >>= 1, i++) {}
	return i;
}

/** Return values for RoadChoosePath, other than a simple trackdir. */
enum RoadChoosePathEnum {
	CHOOSE_PATH_NONE = TRACKDIR_END, ///< no path (attempt to turn around)
	CHOOSE_PATH_WAIT,                ///< path blocked (barred crossing)
	CHOOSE_PATH_SINGLE_PIECE,        ///< single-piece road tile (long turn)
};

/**
 * Return the trackdir to follow on a new tile, or a special marker value.
 * @param v        the Vehicle to do the pathfinding for
 * @param tile     the where to start the pathfinding
 * @param enterdir the direction the vehicle enters the tile from
 * @param tsdir    the direction to use for GetTileRoadStatus (INVALID_DIAGDIR if just reversed)
 * @return the Trackdir to take
 */
static RoadChoosePathEnum RoadChoosePath (RoadVehicle *v, TileIndex tile,
	DiagDirection enterdir, DiagDirection tsdir)
{
	assert ((tsdir == INVALID_DIAGDIR) || (tsdir == ReverseDiagDir (enterdir)));

	switch (GetTileType (tile)) {
		default: return CHOOSE_PATH_NONE;

		case TT_ROAD: {
			TrackStatus ts = GetTileRoadStatus (tile, v->compatible_roadtypes, tsdir);
			assert (TrackStatusToRedSignals(ts) == TRACKDIR_BIT_NONE);
			/* Remove tracks unreachable from the enter dir */
			TrackdirBits trackdirs = TrackStatusToTrackdirBits(ts) & DiagdirReachesTrackdirs(enterdir);
			if (trackdirs == TRACKDIR_BIT_NONE) {
				/* Single-piece road tile? */
				return GetRoadBits (tile, v->roadtype) == DiagDirToRoadBits (ReverseDiagDir (enterdir)) &&
						(!IsTileSubtype (tile, TT_TRACK) || !HasRoadWorks (tile)) ?
					CHOOSE_PATH_SINGLE_PIECE : CHOOSE_PATH_NONE;
			} else if (HasAtMostOneBit (trackdirs)) {
				/* Only one track to choose between? */
				return (RoadChoosePathEnum) FindFirstTrackdir (trackdirs);
			} else if (v->dest_tile == 0) {
				/* Pick a random track if we've got no destination. */
				return (RoadChoosePathEnum) PickRandomBit (trackdirs);
			} else {
				/* This is the only case where we have to
				 * call the pathfinder. */
				bool path_found;
				Trackdir trackdir = YapfRoadVehicleChooseTrack (v, tile, enterdir, trackdirs, path_found);
				v->HandlePathfindingResult (path_found);
				return (RoadChoosePathEnum) trackdir;
			}
		}

		case TT_MISC: {
			switch (GetTileSubtype(tile)) {
				default: return CHOOSE_PATH_NONE;

				case TT_MISC_CROSSING: {
					if ((GetRoadTypes (tile) & v->compatible_roadtypes) == 0) return CHOOSE_PATH_NONE;
					if (GetCrossingRoadAxis (tile) != DiagDirToAxis (enterdir)) return CHOOSE_PATH_NONE;
					if (IsCrossingBarred (tile)) return CHOOSE_PATH_WAIT;
					break;
				}

				case TT_MISC_TUNNEL: {
					if (GetTunnelTransportType (tile) != TRANSPORT_ROAD) return CHOOSE_PATH_NONE;
					if ((GetRoadTypes (tile) & v->compatible_roadtypes) == 0) return CHOOSE_PATH_NONE;

					DiagDirection dir = GetTunnelBridgeDirection (tile);
					if ((enterdir != dir) && (tsdir != INVALID_DIAGDIR || enterdir != ReverseDiagDir (dir))) return CHOOSE_PATH_NONE;
					break;
				}

				case TT_MISC_DEPOT: {
					if (!IsRoadDepot (tile)) return CHOOSE_PATH_NONE;
					if ((GetRoadTypes (tile) & v->compatible_roadtypes) == 0) return CHOOSE_PATH_NONE;
					if (!IsTileOwner (tile, v->owner)) return CHOOSE_PATH_NONE;
					if (GetGroundDepotDirection (tile) != ReverseDiagDir (enterdir)) return CHOOSE_PATH_NONE;
					break;
				}
			}
			break;
		}

		case TT_STATION:
			if (!IsRoadStop (tile)) return CHOOSE_PATH_NONE;
			if ((GetRoadTypes (tile) & v->compatible_roadtypes) == 0) return CHOOSE_PATH_NONE;

			if (IsStandardRoadStopTile (tile)) {
				if (!IsTileOwner (tile, v->owner)) return CHOOSE_PATH_NONE;
				if (v->HasArticulatedPart()) return CHOOSE_PATH_NONE;

				if (GetRoadStopDir (tile) != ReverseDiagDir (enterdir)) return CHOOSE_PATH_NONE;

				RoadStopType rstype = v->IsBus() ? ROADSTOP_BUS : ROADSTOP_TRUCK;
				if (GetRoadStopType (tile) != rstype) return CHOOSE_PATH_NONE;

				if (!_settings_game.pf.roadveh_queue &&
						!RoadStop::GetByTile (tile, rstype)->HasFreeBay()) {
					/* Station is full and RV queuing is off */
					return CHOOSE_PATH_NONE;
				}
			} else {
				if (GetRoadStopAxis (tile) != DiagDirToAxis (enterdir)) return CHOOSE_PATH_NONE;
			}
			break;
	}

	return (RoadChoosePathEnum) DiagDirToDiagTrackdir (enterdir);
}

#include "table/roadveh_movement.h"

static bool RoadVehLeaveDepot(RoadVehicle *v, bool first)
{
	/* Don't leave unless v and following wagons are in the depot. */
	for (const RoadVehicle *u = v; u != NULL; u = u->Next()) {
		if (u->state != RVSB_IN_DEPOT || u->tile != v->tile) return false;
	}

	DiagDirection dir = GetGroundDepotDirection(v->tile);
	v->direction = DiagDirToDir(dir);

	Trackdir tdir = DiagDirToDiagTrackdir(dir);
	const RoadDriveEntry *rdp = _road_drive_data[_settings_game.vehicle.road_side][tdir];

	int x = TileX(v->tile) * TILE_SIZE + rdp[RVC_DEPOT_START_FRAME].x;
	int y = TileY(v->tile) * TILE_SIZE + rdp[RVC_DEPOT_START_FRAME].y;

	if (first) {
		/* We are leaving a depot, but have to go to the exact same one; re-enter */
		if (v->current_order.IsType(OT_GOTO_DEPOT) && v->tile == v->dest_tile) {
			VehicleEnterDepot(v);
			return true;
		}

		if (RoadVehFindCloseTo(v, x, y, v->direction, false) != NULL) return true;

		VehicleServiceInDepot(v);

		StartRoadVehSound(v);

		/* Vehicle is about to leave a depot */
		v->cur_speed = 0;
	}

	v->vehstatus &= ~VS_HIDDEN;
	v->state = tdir;
	v->frame = RVC_DEPOT_START_FRAME;

	v->x_pos = x;
	v->y_pos = y;
	v->UpdatePosition();
	v->UpdateInclination(true, true);

	InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);

	return true;
}

static Trackdir FollowPreviousRoadVehicle (const RoadVehicle *prev, DiagDirection entry_dir)
{
	byte prev_state = prev->state;

	assert (prev_state != RVSB_WORMHOLE);

	Trackdir dir;
	if (prev_state == RVSB_IN_DEPOT) {
		dir = DiagDirToDiagTrackdir (ReverseDiagDir (GetGroundDepotDirection (prev->tile)));
	} else if (HasBit (prev_state, RVS_IN_DT_ROAD_STOP)) {
		dir = (Trackdir)(prev_state & RVSB_ROAD_STOP_TRACKDIR_MASK);
	} else {
		assert (prev_state < TRACKDIR_END);
		dir = (Trackdir)prev_state;

		/* Some bends are so short that the vehicle ahead has already
		 * left the tile when we reach it, in which case it is no
		 * longer at the entered tile and this function is not called.
		 * However, if the vehicle ahead turned around at the tile
		 * edge instead of moving forward, it is still in this tile
		 * but has switched to a reversing trackdir. In such a case,
		 * we must not use its trackdir, but head in the direction
		 * of the tile side at which it is reversing. */
		if (IsReversingRoadTrackdir (dir)) {
			DiagDirection side = TrackdirToExitdir (dir);
			assert (entry_dir != side);
			side = ReverseDiagDir (side);
			if (entry_dir != side) dir = EnterdirExitdirToTrackdir (entry_dir, side);
		}
	}

	/* Do some sanity checking. */
	if (!IsReversingRoadTrackdir(dir)) {
		static const RoadBits required_roadbits[TRACK_END] = {
			ROAD_X, ROAD_Y, ROAD_N, ROAD_S, ROAD_W, ROAD_E,
		};

		RoadBits required = required_roadbits[TrackdirToTrack(dir)];

		assert ((required & GetAnyRoadBits (prev->tile, prev->roadtype, true)) != ROAD_NONE);
	}

	return dir;
}

/**
 * Can a tram track build without destruction on the given tile?
 * @param c the company that would be building the tram tracks
 * @param t the tile to build on.
 * @param r the road bits needed.
 * @return true when a track track can be build on 't'
 */
static bool CanBuildTramTrackOnTile(CompanyID c, TileIndex t, RoadBits r)
{
	/* The 'current' company is not necessarily the owner of the vehicle. */
	Backup<CompanyByte> cur_company(_current_company, c, FILE_LINE);

	CommandCost ret = DoCommand(t, ROADTYPE_TRAM << 4 | r, 0, DC_NO_WATER, CMD_BUILD_ROAD);

	cur_company.Restore();
	return ret.Succeeded();
}

/**
 * Controller for a road vehicle that is about to enter a wormhole.
 * @param v The road vehicle to move.
 * @param end The other end of the wormhole.
 * @param gp The new vehicle position, as returned by GetNewVehiclePos.
 * @param is_bridge Whether the wormhole is a bridge, as opposed to a tunnel.
 */
static void controller_enter_wormhole (RoadVehicle *v, TileIndex end, const FullPosTile &gp, bool is_bridge)
{
	/* This should really bring us to a new virtual tile... */
	assert (gp.tile != v->tile);
	/* ...and there should really be a wormhole part. */
	assert (gp.tile != end);

	v->tile  = end;
	v->state = RVSB_WORMHOLE;
	v->x_pos = gp.xx;
	v->y_pos = gp.yy;

	if (is_bridge) {
		ClrBit(v->gv_flags, GVF_GOINGUP_BIT);
		ClrBit(v->gv_flags, GVF_GOINGDOWN_BIT);

		RoadVehicle *first = v->First();
		first->cur_speed = min(first->cur_speed, GetBridgeSpec(GetRoadBridgeType(end))->speed * 2);

		v->UpdatePositionAndViewport();
	} else {
		v->UpdatePosition();
	}
}

/**
 * Controller for a road vehicle that is about to enter a new tile.
 * @param v The road vehicle to move.
 * @param tile The tile to be entered.
 * @param td The trackdir to take on the new tile.
 * @param x The new x position for the vehicle.
 * @param y The new y position for the vehicle.
 * @param dir The new direction the vehicle is facing.
 */
static void controller_new_tile (RoadVehicle *v, TileIndex tile, Trackdir td,
	uint frame, int x, int y, Direction dir)
{
	if (IsRoadBridgeTile (tile)) {
		RoadVehicle *first = v->First();
		first->cur_speed = min (first->cur_speed, GetBridgeSpec(GetRoadBridgeType(tile))->speed * 2);
	}

	v->tile = tile;
	v->state = td;
	v->frame = frame;

	if (dir != v->direction) {
		v->direction = dir;
		if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) v->cur_speed -= v->cur_speed >> 2;
	}

	controller_set_pos (v, x, y, true, true);
}

/**
 * Controller for a front road vehicle that is about to enter a new tile.
 * @param v The road vehicle to move.
 * @param tile The tile to be entered.
 * @param enterdir The direction in which the tile is entered.
 * @param tsdir The direction to use to get the track status on the new tile.
 * @return Whether the vehicle has moved.
 */
static bool controller_front_new_tile (RoadVehicle *v, TileIndex tile,
	DiagDirection enterdir, DiagDirection tsdir)
{
	Trackdir dir;
	uint start_frame;
	RoadChoosePathEnum path;

	if (v->reverse_ctr != 0) {
		v->reverse_ctr = 0;
		goto short_turn;
	}

	path = RoadChoosePath (v, tile, enterdir, tsdir);

	switch (path) {
		default:
			assert (path < (RoadChoosePathEnum) TRACKDIR_END);
			dir = (Trackdir) path;
			start_frame = RVC_DEFAULT_START_FRAME;
			break;

		case CHOOSE_PATH_NONE:
			if ((v->roadtype == ROADTYPE_TRAM) && CanBuildTramTrackOnTile (v->owner, tile, DiagDirToRoadBits (ReverseDiagDir (enterdir)))) {
				v->cur_speed = 0;
				return false;
			}
		short_turn:
			v->overtaking = 0;
			tile = v->tile;
			dir = _road_reverse_table[enterdir];
			start_frame = RVC_SHORT_TURN_START_FRAME;
			break;

		case CHOOSE_PATH_WAIT:
			v->cur_speed = 0;
			return false;

		case CHOOSE_PATH_SINGLE_PIECE:
			/* Non-tram vehicles can take a shortcut. */
			if (v->roadtype == ROADTYPE_ROAD) goto short_turn;
			v->overtaking = 0;
			dir = _road_reverse_table[enterdir];
			start_frame = RVC_LONG_TURN_START_FRAME;
			break;
	}

	/* Get position data for first frame on the new tile */
	RoadDriveEntry rd = _road_drive_data[_settings_game.vehicle.road_side ^ v->overtaking][dir][start_frame];

	int x = TileX(tile) * TILE_SIZE + rd.x;
	int y = TileY(tile) * TILE_SIZE + rd.y;

	Direction new_dir = RoadVehGetSlidingDirection(v, x, y);

	Vehicle *u = RoadVehFindCloseTo(v, x, y, new_dir);
	if (u != NULL) {
		v->cur_speed = u->First()->cur_speed;
		return false;
	}

	if (IsInsideMM(v->state, RVSB_IN_ROAD_STOP, RVSB_IN_DT_ROAD_STOP_END) && IsStationTile(v->tile)) {
		if (IsReversingRoadTrackdir(dir) && IsInsideMM(v->state, RVSB_IN_ROAD_STOP, RVSB_IN_ROAD_STOP_END)) {
			/* New direction is trying to turn vehicle around.
			 * We can't turn at the exit of a road stop so wait.*/
			v->cur_speed = 0;
			return false;
		}

		/* If we are a drive through road stop and the next tile is of
		 * the same road stop and the next tile isn't this one (i.e. we
		 * are not reversing), then keep the reservation and state.
		 * This way we will not be shortly unregister from the road
		 * stop. It also makes it possible to load when on the edge of
		 * two road stops; otherwise you could get vehicles that should
		 * be loading but are not actually loading. */
		if (IsDriveThroughStopTile(v->tile) &&
				RoadStop::IsDriveThroughRoadStopContinuation(v->tile, tile) &&
				v->tile != tile) {
			/* So, keep 'our' state */
			dir = (Trackdir)v->state;
		} else if (IsRoadStop(v->tile)) {
			/* We're not continuing our drive through road stop, so leave. */
			RoadStop::GetByTile(v->tile, GetRoadStopType(v->tile))->Leave(v);
		}
	}

	controller_new_tile (v, tile, dir, start_frame, x, y, new_dir);
	return true;
}

/**
 * Check if leaving a tile in a given direction leads into a wormhole.
 * @param tile The tile being left.
 * @param enterdir The direction in which it is left.
 * @param next Set to the next tile if there is no wormhole,
 *     else to the other wormhole end.
 * @param data Set to the direction to be used as track status direction
 *     on the new tile if there is no wormhole, else to 1 iff the wormhole
 *     is a bridge.
 * @return Whether leaving the tile leads to a wormhole.
 */
static bool controller_tile_check (TileIndex tile, DiagDirection enterdir, TileIndex *next, uint *data)
{
	TileIndex next_tile = TileAddByDiagDir (tile, enterdir);

	if (IsTunnelTile(tile) && GetTunnelBridgeDirection(tile) == enterdir) {
		TileIndex end_tile = GetOtherTunnelEnd (tile);
		if (end_tile != next_tile) {
			/* Entering a tunnel */
			*next = end_tile;
			*data = 0;
			return true;
		}
		*data = INVALID_DIAGDIR;
	} else if (IsRoadBridgeTile(tile) && GetTunnelBridgeDirection(tile) == enterdir) {
		TileIndex end_tile = GetOtherBridgeEnd (tile);
		if (end_tile != next_tile) {
			/* Entering a bridge */
			*next = end_tile;
			*data = 1;
			return true;
		}
		*data = INVALID_DIAGDIR;
	} else {
		*data = ReverseDiagDir (enterdir);
	}

	*next = next_tile;
	return false;
}

/**
 * Controller for a road vehicle leaving a tile.
 * @param v The road vehicle to move.
 * @param enterdir The direction in which it is leaving its current tile.
 * @return Whether the vehicle has moved.
 */
static bool controller_front_next_tile (RoadVehicle *v, DiagDirection enterdir)
{
	TileIndex next;
	uint data;

	if (controller_tile_check (v->tile, enterdir, &next, &data)) {
		FullPosTile gp = GetNewVehiclePos(v);

		const Vehicle *u = RoadVehFindCloseTo (v, gp.xx, gp.yy, v->direction);
		if (u != NULL) {
			v->cur_speed = u->First()->cur_speed;
			return false;
		}

		controller_enter_wormhole (v, next, gp, data);
		return true;
	} else {
		return controller_front_new_tile (v, next, enterdir, (DiagDirection)data);
	}
}

/**
 * Controller for a road vehicle that has just turned around.
 * @param v The road vehicle to move.
 * @param td The trackdir to use after reversal.
 * @param x The new x position for the vehicle.
 * @param y The new y position for the vehicle.
 * @param dir The new direction the vehicle is facing.
 */
static void controller_turned (RoadVehicle *v, Trackdir td, int x, int y, Direction dir)
{
	if (IsRoadBridgeTile (v->tile)) {
		RoadVehicle *first = v->First();
		first->cur_speed = min (first->cur_speed, GetBridgeSpec(GetRoadBridgeType(v->tile))->speed * 2);
	}

	v->state = td;
	v->frame = RVC_AFTER_TURN_START_FRAME;

	if (dir != v->direction) {
		v->direction = dir;
		if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) v->cur_speed -= v->cur_speed >> 2;
	}

	controller_set_pos (v, x, y, true, true);
}

/**
 * Controller for a road vehicle moving within a tile.
 * @param v The road vehicle to move.
 * @param x The new x position for the vehicle.
 * @param y The new y position for the vehicle.
 * @param dir The new direction the vehicle is facing.
 */
static void controller_midtile (RoadVehicle *v, int x, int y, Direction dir)
{
	if (dir != v->direction) {
		v->direction = dir;
		if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) v->cur_speed -= v->cur_speed >> 2;
	}

	if (IsRoadBridgeTile (v->tile)) {
		RoadVehicle *first = v->First();
		first->cur_speed = min (first->cur_speed, GetBridgeSpec(GetRoadBridgeType(v->tile))->speed * 2);
	}

	if (IsTunnelTile (v->tile)) {
		extern const byte _tunnel_visibility_frame[DIAGDIR_END];

		/* Direction into the wormhole */
		const DiagDirection dir = GetTunnelBridgeDirection (v->tile);

		if (v->direction == DiagDirToDir(dir)) {
			uint frame = DistanceFromTileEdge (ReverseDiagDir(dir), x & 0xF, y & 0xF);
			if (frame == _tunnel_visibility_frame[dir]) {
				/* Frame should be equal to the next frame number in the RV's movement */
				assert ((int)frame == v->frame + 1);
				v->vehstatus |= VS_HIDDEN;
			}
		} else if (v->direction == ReverseDir(DiagDirToDir(dir))) {
			uint frame = DistanceFromTileEdge (dir, x & 0xF, y & 0xF);
			if (frame == TILE_SIZE - _tunnel_visibility_frame[dir]) {
				assert ((int)frame == v->frame + 1);
				v->vehstatus &= ~VS_HIDDEN;
			}
		}

	}

	if (IsGroundDepotTile (v->tile)) {
		assert (IsRoadDepot (v->tile));

		Trackdir out = DiagDirToDiagTrackdir (GetGroundDepotDirection (v->tile));
		if (v->state == out) {
			/* Check if it is time to active the next part. */
			if ((v->Next() != NULL) && (v->frame == v->gcache.cached_veh_length + RVC_DEPOT_START_FRAME)) {
				RoadVehLeaveDepot (v->Next(), false);
			}
		} else if (v->state == ReverseTrackdir (out)) {
			/* Check if we have entered the depot. */
			if (v->frame == RVC_DEPOT_STOP_FRAME) {
				v->state = RVSB_IN_DEPOT;
				v->vehstatus |= VS_HIDDEN;
				v->direction = ReverseDir (v->direction);
				if (v->Next() == NULL) VehicleEnterDepot (v->First());

				InvalidateWindowData (WC_VEHICLE_DEPOT, v->tile);
			}
		}
	}

	v->frame++;
	controller_set_pos (v, x, y, false, true);
}

/**
 * Controller for a road vehicle in a standard road stop.
 * @param v The road vehicle to move.
 * @return Whether the vehicle has moved.
 */
static bool controller_standard_stop (RoadVehicle *v)
{
	assert (v->roadtype == ROADTYPE_ROAD);
	assert (v->Next() == NULL);
	assert (v->overtaking == 0);

	if (v->frame == 0) {
		assert (v->state < RVSB_IN_ROAD_STOP);
		assert (IsDiagonalTrackdir ((Trackdir)v->state));

		/* A vehicle should not proceed beyond frame 0 in a
		 * standard stop until it has been allocated a bay. */
		if (!RoadStop::GetByTile (v->tile, GetRoadStopType (v->tile))->Enter (v)) {
			v->cur_speed = 0;
			return false;
		}
	}

	assert (v->state >= RVSB_IN_ROAD_STOP);
	assert (v->state <= RVSB_IN_ROAD_STOP_END);

	/* Get move position data for next frame. */
	RoadDriveEntry rd = _road_drive_data[_settings_game.vehicle.road_side]
			[v->state][v->frame + 1];

	if (rd.x == RDE_NEXT_TILE) {
		return controller_front_next_tile (v, (DiagDirection)(rd.y));
	}

	assert (rd.x != RDE_TURNED);

	/* Calculate new position for the vehicle */
	int x = (v->x_pos & ~15) + rd.x;
	int y = (v->y_pos & ~15) + rd.y;

	Direction new_dir = RoadVehGetSlidingDirection (v, x, y);

	if (v->frame == _road_stop_stop_frame[_settings_game.vehicle.road_side][v->state & RVSB_TRACKDIR_MASK]) {
		/* Vehicle is at the stopping frame. */
		if (new_dir != v->direction) {
			/* Vehicle is still turning around, so wait. */
			v->direction = new_dir;
			v->UpdateInclination (false, true);
			return true;
		}

		RoadStop *rs = RoadStop::GetByTile (v->tile, GetRoadStopType (v->tile));
		Station *st = Station::GetByTile (v->tile);

		/* Vehicle is at the stop position (at a bay) in a road stop.
		 * Note, if vehicle is loading/unloading it has already been handled,
		 * so if we get here the vehicle has just arrived or is just ready to leave. */
		if (!HasBit (v->state, RVS_ENTERED_STOP)) {
			/* Vehicle has arrived at a bay in a road stop */
			rs->SetEntranceBusy (false);
			SetBit (v->state, RVS_ENTERED_STOP);

			v->last_station_visited = st->index;

			if (v->current_order.IsType (OT_GOTO_STATION) && v->current_order.GetDestination() == st->index) {
				RoadVehArrivesAt (v, st);
				v->BeginLoading();
				return false;
			}
		} else {
			/* Vehicle is ready to leave a bay in a road stop */
			if (rs->IsEntranceBusy()) {
				/* Road stop entrance is busy, so wait as there is nowhere else to go */
				v->cur_speed = 0;
				return false;
			}
			if (v->current_order.IsType (OT_LEAVESTATION)) v->current_order.Free();
		}

		rs->SetEntranceBusy (true);

		StartRoadVehSound (v);
		SetWindowWidgetDirty (WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);

	} else if (new_dir != v->direction) {
		v->direction = new_dir;
		if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) v->cur_speed -= v->cur_speed >> 2;
	}

	v->frame++;
	controller_set_pos (v, x, y, false, true);
	return true;
}

/**
 * Controller for a road vehicle in a drive-through road stop.
 * @param v The road vehicle to move.
 * @return Whether the vehicle has moved.
 */
static bool controller_drivethrough_stop (RoadVehicle *v)
{
	assert (v->overtaking == 0);

	if (!HasBit (v->state, RVS_IN_DT_ROAD_STOP)) {
		assert (v->state <= RVSB_TRACKDIR_MASK);
		assert (IsStraightRoadTrackdir ((Trackdir)v->state));

		if (!RoadStop::GetByTile (v->tile, GetRoadStopType (v->tile))->Enter (v)) NOT_REACHED();
	}

	assert (v->state >= RVSB_IN_DT_ROAD_STOP);
	assert (v->state <= RVSB_IN_DT_ROAD_STOP_END);

	/* Get move position data for next frame. */
	RoadDriveEntry rd = _road_drive_data[_settings_game.vehicle.road_side]
			[v->state & RVSB_ROAD_STOP_TRACKDIR_MASK][v->frame + 1];

	if (rd.x == RDE_NEXT_TILE) {
		return controller_front_next_tile (v, (DiagDirection)(rd.y));
	}

	assert (rd.x != RDE_TURNED);

	/* Calculate new position for the vehicle */
	int x = (v->x_pos & ~15) + rd.x;
	int y = (v->y_pos & ~15) + rd.y;

	Direction new_dir = RoadVehGetSlidingDirection (v, x, y);
	assert (new_dir == DiagDirToDir (TrackdirToExitdir ((Trackdir)(v->state & RVSB_ROAD_STOP_TRACKDIR_MASK))));

	/* Check for a nearby vehicle ahead of us. */
	RoadVehicle *u = RoadVehFindCloseTo (v, x, y, new_dir);
	if (u != NULL) {
		v->cur_speed = u->First()->cur_speed;

		/* In case an RV is stopped in a road stop, why not try to load? */
		if (v->cur_speed == 0 &&
				v->current_order.ShouldStopAtStation (v, GetStationIndex (v->tile)) &&
				v->owner == GetTileOwner (v->tile) && !v->current_order.IsType (OT_LEAVESTATION) &&
				GetRoadStopType (v->tile) == (v->IsBus() ? ROADSTOP_BUS : ROADSTOP_TRUCK)) {
			Station *st = Station::GetByTile (v->tile);
			v->last_station_visited = st->index;
			RoadVehArrivesAt (v, st);
			v->BeginLoading();
		}
		return false;
	}

	assert_compile (RVC_DRIVE_THROUGH_STOP_FRAME > RVC_AFTER_TURN_START_FRAME);

	/* If this is the destination station and it's the correct type of
	 * stop (bus or truck) and the frame equals the stop frame...
	 * (the station test and stop type test ensure that other vehicles,
	 * using the road stop as a through route, do not stop) */
	if (v->frame == RVC_DRIVE_THROUGH_STOP_FRAME &&
			v->current_order.ShouldStopAtStation (v, GetStationIndex (v->tile)) &&
			v->owner == GetTileOwner (v->tile) &&
			GetRoadStopType (v->tile) == (v->IsBus() ? ROADSTOP_BUS : ROADSTOP_TRUCK)) {
		assert (new_dir == v->direction);

		Station *st = Station::GetByTile (v->tile);

		/* Vehicle is at the stop position (at a bay) in a road stop.
		 * Note, if vehicle is loading/unloading it has already been handled,
		 * so if we get here the vehicle has just arrived or is just ready to leave. */
		if (!HasBit (v->state, RVS_ENTERED_STOP)) {
			/* Vehicle has arrived at a bay in a road stop */

			/* Check if next inline bay is free and has compatible road. */
			TileIndex next_tile = TILE_ADD (v->tile, TileOffsByDir (v->direction));
			if (RoadStop::IsDriveThroughRoadStopContinuation (v->tile, next_tile) && (GetRoadTypes (next_tile) & v->compatible_roadtypes) != 0) {
				v->frame++;
				controller_set_pos (v, x, y, true, false);
				return true;
			}

			SetBit (v->state, RVS_ENTERED_STOP);

			v->last_station_visited = st->index;
			RoadVehArrivesAt (v, st);
			v->BeginLoading();
			return false;
		}

		StartRoadVehSound (v);
		SetWindowWidgetDirty (WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);

	} else if (new_dir != v->direction) {
		assert (v->frame == RVC_AFTER_TURN_START_FRAME);
		v->direction = new_dir;
		if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) v->cur_speed -= v->cur_speed >> 2;
	}

	if (v->current_order.IsType (OT_LEAVESTATION)) {
		v->current_order.Free();
	}

	v->frame++;
	controller_set_pos (v, x, y, false, true);
	return true;
}

/**
 * Controller for the front part of a road vehicle in a wormhole.
 * @param v The road vehicle to move.
 * @return Whether the vehicle has moved.
 */
static bool controller_front_wormhole (RoadVehicle *v)
{
	/* Vehicle is on a bridge or in a tunnel */
	FullPosTile gp = GetNewVehiclePos(v);

	const Vehicle *u = RoadVehFindCloseTo (v, gp.xx, gp.yy, v->direction);
	if (u != NULL) {
		v->cur_speed = u->First()->cur_speed;
		return false;
	}

	if (gp.tile != v->tile) {
		/* Still in the wormhole */
		v->x_pos = gp.xx;
		v->y_pos = gp.yy;
		v->UpdatePosition();
		if ((v->vehstatus & VS_HIDDEN) == 0) v->Vehicle::UpdateViewport(true);
		return true;
	}

	/* Vehicle has just exited a bridge or tunnel */
	DiagDirection bridge_dir = GetTunnelBridgeDirection (gp.tile);
	return controller_front_new_tile (v, gp.tile, ReverseDiagDir(bridge_dir), INVALID_DIAGDIR);
}

/**
 * Controller for the front part of a road vehicle.
 * @param v The road vehicle to move.
 * @return Whether the vehicle has moved.
 */
static bool controller_front (RoadVehicle *v)
{
	if (v->overtaking != 0)  {
		if (IsStationTile(v->tile)) {
			/* Force us to be not overtaking! */
			v->overtaking = 0;
		} else if (++v->overtaking_ctr >= RV_OVERTAKE_TIMEOUT) {
			/* If overtaking just aborts at a random moment, we can have a out-of-bound problem,
			 *  if the vehicle started a corner. To protect that, only allow an abort of
			 *  overtake if we are on straight roads */
			if (v->state < RVSB_IN_ROAD_STOP && IsStraightRoadTrackdir((Trackdir)v->state)) {
				v->overtaking = 0;
			}
		}
	}

	if (v->state == RVSB_WORMHOLE) return controller_front_wormhole (v);

	if (v->state == RVSB_IN_DEPOT) return true;

	if (IsStationTile (v->tile)) {
		assert (IsRoadStopTile (v->tile));

		if (IsStandardRoadStopTile (v->tile)) {
			return controller_standard_stop (v);
		} else if (HasBit (v->state, RVS_IN_DT_ROAD_STOP) || !IsReversingRoadTrackdir((Trackdir)v->state)) {
			return controller_drivethrough_stop (v);
		}
	}

	assert (v->state <= RVSB_TRACKDIR_MASK);

	/* Get move position data for next frame. */
	RoadDriveEntry rd = _road_drive_data[_settings_game.vehicle.road_side ^ v->overtaking]
			[v->state][v->frame + 1];

	if (rd.x == RDE_NEXT_TILE) {
		return controller_front_next_tile (v, (DiagDirection)(rd.y));
	}

	if (rd.x == RDE_TURNED) {
		/* Vehicle has finished turning around, it will now head back onto the same tile */
		v->reverse_ctr = 0;

		DiagDirection enterdir = (DiagDirection)(rd.y);
		RoadChoosePathEnum path = RoadChoosePath (v, v->tile, enterdir, INVALID_DIAGDIR);
		Trackdir td;
		switch (path) {
			default:
				assert (path < (RoadChoosePathEnum) TRACKDIR_END);
				td = (Trackdir) path;
				break;

			case CHOOSE_PATH_NONE:
				/* Long turn at a single-piece road tile. */
				assert (IsRoadTile (v->tile));
				assert (GetRoadBits (v->tile, v->roadtype) == DiagDirToRoadBits (enterdir));
				td = DiagDirToDiagTrackdir (enterdir);
				break;

			case CHOOSE_PATH_WAIT:
				v->cur_speed = 0;
				return false;

			case CHOOSE_PATH_SINGLE_PIECE:
				td = _road_reverse_table[enterdir];
				break;
		}

		RoadDriveEntry rd = _road_drive_data[_settings_game.vehicle.road_side]
				[td][RVC_AFTER_TURN_START_FRAME];

		int x = TileX(v->tile) * TILE_SIZE + rd.x;
		int y = TileY(v->tile) * TILE_SIZE + rd.y;

		Direction new_dir = RoadVehGetSlidingDirection (v, x, y);
		if (RoadVehFindCloseTo (v, x, y, new_dir) != NULL) return false;

		controller_turned (v, td, x, y, new_dir);
		return true;
	}

	/* Calculate new position for the vehicle */
	int x = (v->x_pos & ~15) + rd.x;
	int y = (v->y_pos & ~15) + rd.y;

	Direction new_dir = RoadVehGetSlidingDirection (v, x, y);

	/* Vehicle is not in a road stop.
	 * Check for another vehicle to overtake */
	RoadVehicle *u = RoadVehFindCloseTo (v, x, y, new_dir);

	if (u != NULL) {
		u = u->First();
		/* There is a vehicle in front overtake it if possible */
		if (v->overtaking == 0) RoadVehCheckOvertake (v, u);
		if (v->overtaking == 0) v->cur_speed = u->cur_speed;
		return false;
	}

	controller_midtile (v, x, y, new_dir);
	return true;
}

/**
 * Controller for a (non-front) articulated part about to enter a new tile.
 * @param v The road vehicle to move.
 * @param prev The previous articulated part in the vehicle.
 * @param tile The tile to be entered.
 * @param enterdir The direction in which the tile is entered.
 */
static void controller_follow_new_tile (RoadVehicle *v,
	const RoadVehicle *prev, TileIndex tile, DiagDirection enterdir)
{
	Trackdir dir;
	uint start_frame;

	if (prev->tile != tile) {
		DiagDirection exitdir = DiagdirBetweenTiles (tile, prev->tile);
		assert (IsValidDiagDirection (exitdir));
		dir = EnterdirExitdirToTrackdir (enterdir, exitdir);
		if (IsReversingRoadTrackdir (dir)) {
			/* The previous vehicle turned around at the tile edge. */
			assert (tile != v->tile);
			tile = v->tile;
			start_frame = RVC_SHORT_TURN_START_FRAME;
		} else {
			start_frame = RVC_DEFAULT_START_FRAME;
		}
	} else {
		dir = FollowPreviousRoadVehicle (prev, enterdir);
		assert_compile (RVC_DEFAULT_START_FRAME == RVC_LONG_TURN_START_FRAME);
		start_frame = RVC_DEFAULT_START_FRAME;
	}

	/* Get position data for first frame on the new tile */
	RoadDriveEntry rd = _road_drive_data[_settings_game.vehicle.road_side][dir][start_frame];

	int x = TileX(tile) * TILE_SIZE + rd.x;
	int y = TileY(tile) * TILE_SIZE + rd.y;

	controller_new_tile (v, tile, dir, start_frame, x, y, RoadVehGetSlidingDirection(v, x, y));
}

/**
 * Controller for a (non-front) articulated part in a road vehicle.
 * @param v The road vehicle to move.
 * @param prev The previous articulated part in the vehicle.
 */
static void controller_follow (RoadVehicle *v, const RoadVehicle *prev)
{
	if (v->state == RVSB_WORMHOLE) {
		FullPosTile gp = GetNewVehiclePos(v);
		if (gp.tile != v->tile) {
			/* Still in the wormhole */
			v->x_pos = gp.xx;
			v->y_pos = gp.yy;
			v->UpdatePosition();
			if ((v->vehstatus & VS_HIDDEN) == 0) v->Vehicle::UpdateViewport(true);
			return;
		}

		/* Vehicle has just exited a bridge or tunnel */
		DiagDirection bridge_dir = GetTunnelBridgeDirection (gp.tile);
		controller_follow_new_tile (v, prev, gp.tile, ReverseDiagDir(bridge_dir));
		return;
	}

	if (v->state == RVSB_IN_DEPOT) return;

	assert (v->state <= RVSB_TRACKDIR_MASK);
	assert (v->overtaking == 0);

	/* Get move position data for next frame. */
	RoadDriveEntry rd = _road_drive_data[_settings_game.vehicle.road_side]
			[v->state][v->frame + 1];

	if (rd.x == RDE_NEXT_TILE) {
		DiagDirection enterdir = (DiagDirection)(rd.y);
		TileIndex next;
		uint data;

		if (controller_tile_check (v->tile, enterdir, &next, &data)) {
			controller_enter_wormhole (v, next, GetNewVehiclePos(v), data);
		} else {
			controller_follow_new_tile (v, prev, next, enterdir);
		}
		return;
	}

	if (rd.x == RDE_TURNED) {
		DiagDirection enterdir = (DiagDirection)(rd.y);
		Trackdir td;
		if (prev->tile != v->tile) {
			DiagDirection exitdir = DiagdirBetweenTiles (v->tile, prev->tile);
			assert (IsValidDiagDirection (exitdir));
			assert (exitdir != ReverseDiagDir (enterdir));
			td = EnterdirExitdirToTrackdir (enterdir, exitdir);
			assert (!IsReversingRoadTrackdir (td));
		} else {
			td = FollowPreviousRoadVehicle (prev, enterdir);
		}

		RoadDriveEntry rd = _road_drive_data[_settings_game.vehicle.road_side]
				[td][RVC_AFTER_TURN_START_FRAME];

		int x = TileX(v->tile) * TILE_SIZE + rd.x;
		int y = TileY(v->tile) * TILE_SIZE + rd.y;

		controller_turned (v, td, x, y, RoadVehGetSlidingDirection (v, x, y));
		return;
	}

	/* Calculate new position for the vehicle */
	int x = (v->x_pos & ~15) + rd.x;
	int y = (v->y_pos & ~15) + rd.y;

	controller_midtile (v, x, y, RoadVehGetSlidingDirection (v, x, y));
}

static bool RoadVehController(RoadVehicle *v)
{
	/* decrease counters */
	v->current_order_time++;
	if (v->reverse_ctr != 0) v->reverse_ctr--;

	/* handle crashed */
	if (v->vehstatus & VS_CRASHED || RoadVehCheckTrainCrash(v)) {
		return RoadVehIsCrashed(v);
	}

	/* road vehicle has broken down? */
	if (v->HandleBreakdown()) return true;
	if (v->vehstatus & VS_STOPPED) return true;

	ProcessOrders(v);
	v->HandleLoading();

	if (v->current_order.IsType(OT_LOADING)) return true;

	if (v->IsInDepot() && RoadVehLeaveDepot(v, true)) return true;

	v->ShowVisualEffect();

	/* Check how far the vehicle needs to proceed */
	int j = v->UpdateSpeed();

	int adv_spd = v->GetAdvanceDistance();
	bool blocked = false;
	while (j >= adv_spd) {
		j -= adv_spd;

		if (!controller_front (v)) {
			blocked = true;
			break;
		}

		RoadVehicle *prev = v;
		RoadVehicle *u = v->Next();
		while (u != NULL) {
			controller_follow (u, prev);
			prev = u;
			u = u->Next();
		}

		/* Determine distance to next map position */
		adv_spd = v->GetAdvanceDistance();

		/* Test for a collision, but only if another movement will occur. */
		if (j >= adv_spd && RoadVehCheckTrainCrash(v)) break;
	}

	v->SetLastSpeed();

	for (RoadVehicle *u = v; u != NULL; u = u->Next()) {
		if ((u->vehstatus & VS_HIDDEN) != 0) continue;

		u->UpdateViewport(false, false);
	}

	/* If movement is blocked, set 'progress' to its maximum, so the roadvehicle does
	 * not accelerate again before it can actually move. I.e. make sure it tries to advance again
	 * on next tick to discover whether it is still blocked. */
	if (v->progress == 0) v->progress = blocked ? adv_spd - 1 : j;

	return true;
}

Money RoadVehicle::GetRunningCost() const
{
	const Engine *e = this->GetEngine();
	if (e->u.road.running_cost_class == INVALID_PRICE) return 0;

	uint cost_factor = GetVehicleProperty(this, PROP_ROADVEH_RUNNING_COST_FACTOR, e->u.road.running_cost);
	if (cost_factor == 0) return 0;

	return GetPrice(e->u.road.running_cost_class, cost_factor, e->GetGRF());
}

bool RoadVehicle::Tick()
{
	this->tick_counter++;

	if (this->IsFrontEngine()) {
		if (!(this->vehstatus & VS_STOPPED)) this->running_ticks++;
		return RoadVehController(this);
	}

	return true;
}

static void CheckIfRoadVehNeedsService(RoadVehicle *v)
{
	/* If we already got a slot at a stop, use that FIRST, and go to a depot later */
	if (Company::Get(v->owner)->settings.vehicle.servint_roadveh == 0 || !v->NeedsAutomaticServicing()) return;
	if (v->IsChainInDepot()) {
		VehicleServiceInDepot(v);
		return;
	}

	/* Only go to the depot if it is not too far out of our way. */
	TileIndex rfdd = FindClosestRoadDepot(v, true);
	if (rfdd == INVALID_TILE) {
		if (v->current_order.IsType(OT_GOTO_DEPOT)) {
			/* If we were already heading for a depot but it has
			 * suddenly moved farther away, we continue our normal
			 * schedule? */
			v->current_order.MakeDummy();
			SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);
		}
		return;
	}

	DepotID depot = GetDepotIndex(rfdd);

	if (v->current_order.IsType(OT_GOTO_DEPOT) &&
			v->current_order.GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS &&
			!Chance16(1, 20)) {
		return;
	}

	SetBit(v->gv_flags, GVF_SUPPRESS_IMPLICIT_ORDERS);
	v->current_order.MakeGoToDepot(depot, ODTFB_SERVICE);
	v->dest_tile = rfdd;
	SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);
}

void RoadVehicle::OnNewDay()
{
	AgeVehicle(this);

	if (!this->IsFrontEngine()) return;

	if ((++this->day_counter & 7) == 0) DecreaseVehicleValue(this);
	if (this->blocked_ctr == 0) CheckVehicleBreakdown(this);

	CheckIfRoadVehNeedsService(this);

	CheckOrders(this);

	if (this->running_ticks == 0) return;

	CommandCost cost(EXPENSES_ROADVEH_RUN, this->GetRunningCost() * this->running_ticks / (DAYS_IN_YEAR * DAY_TICKS));

	this->profit_this_year -= cost.GetCost();
	this->running_ticks = 0;

	SubtractMoneyFromCompanyFract(this->owner, cost);

	SetWindowDirty(WC_VEHICLE_DETAILS, this->index);
	SetWindowClassesDirty(WC_ROADVEH_LIST);
}
