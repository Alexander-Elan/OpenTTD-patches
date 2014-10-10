/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file follow_track.hpp Template function for track followers */

#ifndef  FOLLOW_TRACK_HPP
#define  FOLLOW_TRACK_HPP

#include "../map/road.h"
#include "../pbs.h"
#include "../roadveh.h"
#include "../station_base.h"
#include "../train.h"
#include "../tunnelbridge.h"
#include "../depot_func.h"
#include "../bridge.h"
#include "../ship.h"
#include "pos.h"
#include "railpos.h"
#include "pf_performance_timer.hpp"

/**
 * Track follower types base class
 */
struct CFollowTrackTypes
{
	enum TileFlag {
		TF_NONE,
		TF_STATION,
		TF_TUNNEL,
		TF_BRIDGE,
	};

	enum ErrorCode {
		EC_NONE,
		EC_OWNER,
		EC_RAIL_TYPE,
		EC_90DEG,
		EC_NO_WAY,
		EC_RESERVED,
	};

	enum TileResult {
		TR_NORMAL,
		TR_NO_WAY,
		TR_REVERSE,
		TR_BRIDGE,
		TR_TUNNEL,
	};
};

/**
 * Track follower common base class
 */
template <class PPos>
struct CFollowTrackBase : CFollowTrackTypes
{
	typedef PPos Pos;

	Pos                 m_old;           ///< the origin (vehicle moved from) before move
	PathMPos<Pos>       m_new;           ///< the new tile (the vehicle has entered)
	DiagDirection       m_exitdir;       ///< exit direction (leaving the old tile)
	TileFlag            m_flag;          ///< last turn passed station, tunnel or bridge
	int                 m_tiles_skipped; ///< number of skipped tunnel or station tiles
	ErrorCode           m_err;
};


/**
 * Track follower helper template class (can serve pathfinders and vehicle
 *  controllers). See 6 different typedefs below for 3 different transport
 *  types w/ or w/o 90-deg turns allowed
 */
template <class Base>
struct CFollowTrack : Base
{
	/* MSVC does not support variadic templates. Oh well... */

	inline CFollowTrack() : Base() { }

	template <typename T1>
	inline CFollowTrack (T1 t1) : Base (t1) { }

	template <typename T1, typename T2>
	inline CFollowTrack (T1 t1, T2 t2) : Base (t1, t2) { }

	template <typename T1, typename T2, typename T3>
	inline CFollowTrack (T1 t1, T2 t2, T3 t3) : Base (t1, t2, t3) { }

	template <typename T1, typename T2, typename T3, typename T4>
	inline CFollowTrack (T1 t1, T2 t2, T3 t3, T4 t4) : Base (t1, t2, t3, t4) { }

	/**
	 * Main follower routine. Attempts to follow track at the given
	 * pathfinder position. On return:
	 *  * m_old is always set to the position given as argument.
	 *  * On success, true is returned, and all fields are filled in as
	 * appropriate. m_err is guaranteed to be EC_NONE, and m_exitdir may
	 * not be the natural exit direction of m_old.td, if the track
	 * follower had to reverse.
	 *  * On failure, false is returned, and m_err is set to a value
	 * indicating why the track could not be followed. The rest of the
	 * fields should be considered undefined.
	 */
	inline bool Follow(const typename Base::Pos &pos)
	{
		Base::m_old = pos;
		Base::m_err = Base::EC_NONE;
		Base::m_exitdir = TrackdirToExitdir(Base::m_old.td);

		if (Base::m_old.in_wormhole()) {
			Base::FollowWormhole();
		} else {
			switch (Base::CheckOldTile()) {
				case Base::TR_NO_WAY:
					Base::m_err = Base::EC_NO_WAY;
					return false;
				case Base::TR_REVERSE:
					Base::m_new.set (Base::m_old.tile, ReverseTrackdir(Base::m_old.td));
					Base::m_exitdir = ReverseDiagDir(Base::m_exitdir);
					Base::m_tiles_skipped = 0;
					Base::m_flag = Base::TF_NONE;
					return true;
				case Base::TR_BRIDGE:
					/* we are entering the bridge */
					if (EnterWormhole(true)) return true;
					break;
				case Base::TR_TUNNEL:
					/* we are entering the tunnel */
					if (EnterWormhole(false)) return true;
					break;
				default:
					/* normal or station tile, do one step */
					Base::m_new.set_tile (TileAddByDiagDir (Base::m_old.tile, Base::m_exitdir));
					Base::m_tiles_skipped = 0;
					/* special handling for stations */
					Base::m_flag = Base::CheckStation() ? Base::TF_STATION : Base::TF_NONE;
					break;
			}
		}

		assert(!Base::m_new.in_wormhole());

		/* If we are not in a wormhole but m_flag is set to TF_BRIDGE
		 * or TF_TUNNEL, then we must have just exited a wormhole, in
		 * which case we can skip many checks below. */
		switch (Base::m_flag) {
			case Base::TF_BRIDGE:
				assert(IsBridgeHeadTile(Base::m_new.tile));
				assert(Base::m_exitdir == ReverseDiagDir(GetTunnelBridgeDirection(Base::m_new.tile)));

				Base::m_new.set_trackdirs (Base::GetTrackStatusTrackdirBits(Base::m_new.tile) & DiagdirReachesTrackdirs(Base::m_exitdir));
				assert(!Base::m_new.is_empty());
				return true;

			case Base::TF_TUNNEL:
				assert(IsTunnelTile(Base::m_new.tile));
				assert(Base::m_exitdir == ReverseDiagDir(GetTunnelBridgeDirection(Base::m_new.tile)));

				Base::m_new.set_trackdir (DiagDirToDiagTrackdir(Base::m_exitdir));
				assert(Base::m_new.trackdirs == (Base::GetTrackStatusTrackdirBits(Base::m_new.tile) & DiagdirReachesTrackdirs(Base::m_exitdir)));
				return true;

			default: break;
		}

		if (!Base::CheckNewTile()) {
			assert(Base::m_err != Base::EC_NONE);
			if (!Base::CheckEndOfLine()) return false;
			Base::m_err = Base::EC_NONE; // clear error set by CheckNewTile
			return true;
		}

		if (!Base::Allow90deg()) {
			TrackdirBits trackdirs = Base::m_new.trackdirs & (TrackdirBits)~(int)TrackdirCrossesTrackdirs(Base::m_old.td);
			if (trackdirs == TRACKDIR_BIT_NONE) {
				Base::m_err = Base::EC_90DEG;
				return false;
			}
			Base::m_new.set_trackdirs (trackdirs);
		}

		return true;
	}

	inline bool FollowNext()
	{
		assert(Base::m_new.is_valid());
		assert(Base::m_new.is_single());
		return Follow(Base::m_new);
	}

	inline void SetPos(const typename Base::Pos &pos)
	{
		Base::m_new.set(pos);
	}

protected:
	/** Enter a wormhole; return whether the new position is in the
	 * wormhole, so there is nothing else to do */
	inline bool EnterWormhole (bool is_bridge)
	{
		Base::m_flag = is_bridge ? Base::TF_BRIDGE : Base::TF_TUNNEL;
		TileIndex other_end = is_bridge ? GetOtherBridgeEnd(Base::m_old.tile) : GetOtherTunnelEnd(Base::m_old.tile);
		uint length = GetTunnelBridgeLength (Base::m_old.tile, other_end);

		if (length > 0 && Base::EnterWormhole (is_bridge, other_end, length)) {
			return true;
		}

		Base::m_tiles_skipped = length;
		Base::m_new.set_tile (other_end);
		return false;
	}
};


/**
 * Track follower rail base class
 */
struct CFollowTrackRailBase : CFollowTrackBase<RailPathPos>
{
	const Owner               m_veh_owner;     ///< owner of the vehicle
	const bool                m_allow_90deg;
	const RailTypes           m_railtypes;
	CPerformanceTimer  *const m_pPerf;

	inline bool Allow90deg() const { return m_allow_90deg; }

	inline CFollowTrackRailBase(const Train *v, bool allow_90deg = true)
		: m_veh_owner(v->owner), m_allow_90deg(allow_90deg), m_railtypes(v->compatible_railtypes), m_pPerf(NULL)
	{
		assert(m_railtypes != INVALID_RAILTYPES);
	}

	inline CFollowTrackRailBase(const Train *v, bool allow_90deg, RailTypes railtype_override, CPerformanceTimer *pPerf = NULL)
		: m_veh_owner(v->owner), m_allow_90deg(allow_90deg), m_railtypes(railtype_override), m_pPerf(pPerf)
	{
		assert(railtype_override != INVALID_RAILTYPES);
	}

	inline CFollowTrackRailBase(const Train *v, bool allow_90deg, RailType railtype_override)
		: m_veh_owner(v->owner), m_allow_90deg(allow_90deg), m_railtypes(GetRailTypeInfo(railtype_override)->compatible_railtypes), m_pPerf(NULL)
	{
		assert(m_railtypes != INVALID_RAILTYPES);
	}

	inline CFollowTrackRailBase(Owner o, bool allow_90deg, RailTypes railtype_override)
		: m_veh_owner(o), m_allow_90deg(allow_90deg), m_railtypes(railtype_override), m_pPerf(NULL)
	{
		assert(railtype_override != INVALID_RAILTYPES);
	}

	inline TrackdirBits GetTrackStatusTrackdirBits(TileIndex tile) const
	{
		return TrackStatusToTrackdirBits(GetTileRailwayStatus(tile));
	}

	/** check old tile */
	inline TileResult CheckOldTile()
	{
		assert(!m_old.in_wormhole());
		assert((GetTrackStatusTrackdirBits(m_old.tile) & TrackdirToTrackdirBits(m_old.td)) != 0);

		switch (GetTileType(m_old.tile)) {
			case TT_RAILWAY:
				return IsTileSubtype(m_old.tile, TT_BRIDGE) && m_exitdir == GetTunnelBridgeDirection(m_old.tile) ?
						TR_BRIDGE : TR_NORMAL;

			case TT_MISC:
				switch (GetTileSubtype(m_old.tile)) {
					case TT_MISC_TUNNEL: {
						DiagDirection enterdir = GetTunnelBridgeDirection(m_old.tile);
						if (enterdir == m_exitdir) return TR_TUNNEL;
						assert(ReverseDiagDir(enterdir) == m_exitdir);
						return TR_NORMAL;
					}

					case TT_MISC_DEPOT: {
						/* depots cause reversing */
						assert(IsRailDepot(m_old.tile));
						DiagDirection exitdir = GetGroundDepotDirection(m_old.tile);
						if (exitdir != m_exitdir) {
							assert(exitdir == ReverseDiagDir(m_exitdir));
							return TR_REVERSE;
						}
						return TR_NORMAL;
					}

					default: return TR_NORMAL;
				}

			default: return TR_NORMAL;
		}
	}

	/** stores track status (available trackdirs) for the new tile into m_new.trackdirs */
	inline bool CheckNewTile()
	{
		CPerfStart perf(*m_pPerf);

		TrackdirBits trackdirs;
		if (IsNormalRailTile(m_new.tile)) {
			trackdirs = TrackBitsToTrackdirBits(GetTrackBits(m_new.tile));
		} else {
			trackdirs = GetTrackStatusTrackdirBits(m_new.tile);
		}

		trackdirs &= DiagdirReachesTrackdirs(m_exitdir);
		if (trackdirs == TRACKDIR_BIT_NONE) {
			m_err = EC_NO_WAY;
			return false;
		}

		m_new.set_trackdirs (trackdirs);

		perf.Stop();

		if (IsRailDepotTile(m_new.tile)) {
			DiagDirection exitdir = GetGroundDepotDirection(m_new.tile);
			if (ReverseDiagDir(exitdir) != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		/* rail transport is possible only on tiles with the same owner as vehicle */
		if (GetTileOwner(m_new.tile) != m_veh_owner) {
			/* different owner */
			m_err = EC_NO_WAY;
			return false;
		}

		/* rail transport is possible only on compatible rail types */
		RailType rail_type;
		if (IsRailwayTile(m_new.tile)) {
			rail_type = GetSideRailType(m_new.tile, ReverseDiagDir(m_exitdir));
			if (rail_type == INVALID_RAILTYPE) {
				m_err = EC_NO_WAY;
				return false;
			}
		} else {
			rail_type = GetRailType(m_new.tile);
		}

		if (!HasBit(m_railtypes, rail_type)) {
			/* incompatible rail type */
			m_err = EC_RAIL_TYPE;
			return false;
		}

		/* tunnel holes and bridge ramps can be entered only from proper direction */
		assert(m_flag != TF_BRIDGE);
		assert(m_flag != TF_TUNNEL);
		if (IsTunnelTile(m_new.tile)) {
			if (GetTunnelBridgeDirection(m_new.tile) != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		} else if (IsRailBridgeTile(m_new.tile)) {
			if (GetTunnelBridgeDirection(m_new.tile) == ReverseDiagDir(m_exitdir)) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		/* special handling for rail stations - get to the end of platform */
		if (m_flag == TF_STATION) {
			/* entered railway station
			 * get platform length */
			uint length = BaseStation::GetByTile(m_new.tile)->GetPlatformLength(m_new.tile, m_exitdir);
			/* how big step we must do to get to the last platform tile; */
			m_tiles_skipped = length - 1;
			/* move to the platform end */
			TileIndexDiff diff = TileOffsByDiagDir(m_exitdir);
			diff *= m_tiles_skipped;
			m_new.tile = TILE_ADD(m_new.tile, diff);
		}

		return true;
	}

	/** return true if we successfully reversed at end of road/track */
	inline bool CheckEndOfLine()
	{
		return false;
	}

	inline bool CheckStation()
	{
		return HasStationTileRail(m_new.tile);
	}

	/** Follow a track that heads into a wormhole */
	inline bool EnterWormhole (bool is_bridge, TileIndex other_end, uint length)
	{
		assert (length > 0);

		m_tiles_skipped = length - 1;
		m_new.set (TileAddByDiagDir (other_end, ReverseDiagDir(m_exitdir)),
				DiagDirToDiagTrackdir(m_exitdir), other_end);

		return true;
	}

	/** Follow m_old when in a wormhole */
	inline void FollowWormhole()
	{
		assert(m_old.in_wormhole());
		assert(IsRailBridgeTile(m_old.wormhole) || IsTunnelTile(m_old.wormhole));

		m_new.set_tile (m_old.wormhole);
		m_flag = IsTileSubtype(m_old.wormhole, TT_BRIDGE) ? TF_BRIDGE : TF_TUNNEL;
		m_tiles_skipped = GetTunnelBridgeLength(m_new.tile, m_old.tile);
	}

	/** Helper for pathfinders - get max speed on m_old */
	int GetSpeedLimit (void) const
	{
		/* Check for on-bridge and railtype speed limit */
		TileIndex bridge_tile;
		RailType rt;

		if (!m_old.in_wormhole()) {
			bridge_tile = IsRailBridgeTile(m_old.tile) ? m_old.tile : INVALID_TILE;
			rt = m_old.get_railtype();
		} else if (IsTileSubtype(m_old.wormhole, TT_BRIDGE)) {
			bridge_tile = m_old.wormhole;
			rt = GetBridgeRailType(bridge_tile);
		} else {
			bridge_tile = INVALID_TILE;
			rt = GetRailType(m_old.wormhole);
		}

		int max_speed;

		/* Check for on-bridge speed limit */
		if (bridge_tile != INVALID_TILE) {
			max_speed = GetBridgeSpec(GetRailBridgeType(bridge_tile))->speed;
		} else {
			max_speed = INT_MAX; // no limit
		}

		/* Check for speed limit imposed by railtype */
		uint16 rail_speed = GetRailTypeInfo(rt)->max_speed;
		if (rail_speed > 0) max_speed = min(max_speed, rail_speed);

		return max_speed;
	}

	inline bool MaskReservedTracks()
	{
		if (m_flag == TF_STATION) {
			/* Check skipped station tiles as well. */
			TileIndexDiff diff = TileOffsByDiagDir(m_exitdir);
			TileIndex tile = m_new.tile - diff * m_tiles_skipped;
			for (;;) {
				if (HasStationReservation(tile)) {
					m_new.clear_trackdirs();
					m_err = EC_RESERVED;
					return false;
				}
				if (tile == m_new.tile) return true;
				tile += diff;
			}
		}

		if (m_new.in_wormhole()) {
			assert(m_new.is_single());
			if (HasReservedPos(m_new)) {
				m_new.clear_trackdirs();
				m_err = EC_RESERVED;
				return false;
			} else {
				return true;
			}
		}

		TrackBits reserved = GetReservedTrackbits(m_new.tile);
		/* Mask already reserved trackdirs. */
		TrackdirBits trackdirs = m_new.trackdirs & ~TrackBitsToTrackdirBits(reserved);
		/* Mask out all trackdirs that conflict with the reservation. */
		Track t;
		FOR_EACH_SET_TRACK(t, TrackdirBitsToTrackBits(trackdirs)) {
			if (TracksOverlap(reserved | TrackToTrackBits(t))) trackdirs &= ~TrackToTrackdirBits(t);
		}
		if (trackdirs == TRACKDIR_BIT_NONE) {
			m_new.clear_trackdirs();
			m_err = EC_RESERVED;
			return false;
		}
		m_new.set_trackdirs (trackdirs);
		return true;
	}
};

typedef CFollowTrack<CFollowTrackRailBase> CFollowTrackRail;


/**
 * Track follower road base class
 */
struct CFollowTrackRoadBase : CFollowTrackBase<RoadPathPos>
{
	const RoadVehicle *const m_veh; ///< moving vehicle

	static inline bool Allow90deg() { return true; }

	inline CFollowTrackRoadBase(const RoadVehicle *v)
		: m_veh(v)
	{
		assert(v != NULL);
	}

	inline TrackdirBits GetTrackStatusTrackdirBits(TileIndex tile) const
	{
		return TrackStatusToTrackdirBits(GetTileRoadStatus(tile, m_veh->compatible_roadtypes));
	}

	inline bool IsTram() { return HasBit(m_veh->compatible_roadtypes, ROADTYPE_TRAM); }

	/** Tests if a tile is a road tile with a single tramtrack (tram can reverse) */
	inline DiagDirection GetSingleTramBit(TileIndex tile)
	{
		assert(IsTram()); // this function shouldn't be called in other cases

		if (IsRoadTile(tile)) {
			RoadBits rb = GetRoadBits(tile, ROADTYPE_TRAM);
			switch (rb) {
				case ROAD_NW: return DIAGDIR_NW;
				case ROAD_SW: return DIAGDIR_SW;
				case ROAD_SE: return DIAGDIR_SE;
				case ROAD_NE: return DIAGDIR_NE;
				default: break;
			}
		}
		return INVALID_DIAGDIR;
	}

	/** check old tile */
	inline TileResult CheckOldTile()
	{
		assert(!m_old.in_wormhole());
		assert(((GetTrackStatusTrackdirBits(m_old.tile) & TrackdirToTrackdirBits(m_old.td)) != 0) ||
		       (IsTram() && GetSingleTramBit(m_old.tile) != INVALID_DIAGDIR)); // Disable the assertion for single tram bits

		switch (GetTileType(m_old.tile)) {
			default: NOT_REACHED();

			case TT_ROAD:
				if (IsTram()) {
					DiagDirection single_tram = GetSingleTramBit(m_old.tile);
					/* single tram bits cause reversing */
					if (single_tram == ReverseDiagDir(m_exitdir)) {
						return TR_REVERSE;
					}
					/* single tram bits can only be left in one direction */
					if (single_tram != INVALID_DIAGDIR && single_tram != m_exitdir) {
						return TR_NO_WAY;
					}
				}
				return IsTileSubtype(m_old.tile, TT_BRIDGE) && m_exitdir == GetTunnelBridgeDirection(m_old.tile) ?
						TR_BRIDGE : TR_NORMAL;

			case TT_MISC:
				switch (GetTileSubtype(m_old.tile)) {
					case TT_MISC_TUNNEL: {
						DiagDirection enterdir = GetTunnelBridgeDirection(m_old.tile);
						if (enterdir == m_exitdir) return TR_TUNNEL;
						assert(ReverseDiagDir(enterdir) == m_exitdir);
						return TR_NORMAL;
					}

					case TT_MISC_DEPOT: {
						/* depots cause reversing */
						assert(IsRoadDepot(m_old.tile));
						DiagDirection exitdir = GetGroundDepotDirection(m_old.tile);
						if (exitdir != m_exitdir) {
							assert(exitdir == ReverseDiagDir(m_exitdir));
							return TR_REVERSE;
						}
						return TR_NORMAL;
					}

					default: return TR_NORMAL;
				}

			case TT_STATION:
				/* road stop can be left at one direction only unless it's a drive-through stop */
				if (IsStandardRoadStopTile(m_old.tile)) {
					DiagDirection exitdir = GetRoadStopDir(m_old.tile);
					if (exitdir != m_exitdir) {
						return TR_NO_WAY;
					}
				}
				return TR_NORMAL;
		}
	}

	/** stores track status (available trackdirs) for the new tile into m_new.trackdirs */
	inline bool CheckNewTile()
	{
		TrackdirBits trackdirs = GetTrackStatusTrackdirBits(m_new.tile);

		if (trackdirs == TRACKDIR_BIT_NONE) {
			/* GetTileRoadStatus() returns 0 for single tram bits.
			 * As we cannot change it there (easily) without breaking something, change it here */
			if (IsTram() && GetSingleTramBit(m_new.tile) == ReverseDiagDir(m_exitdir)) {
				m_new.set_trackdir (DiagDirToDiagTrackdir(m_exitdir));
				return true;
			} else {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		trackdirs &= DiagdirReachesTrackdirs(m_exitdir);
		if (trackdirs == TRACKDIR_BIT_NONE) {
			m_err = EC_NO_WAY;
			return false;
		}

		m_new.set_trackdirs (trackdirs);

		if (IsStandardRoadStopTile(m_new.tile)) {
			/* road stop can be entered from one direction only unless it's a drive-through stop */
			DiagDirection exitdir = GetRoadStopDir(m_new.tile);
			if (ReverseDiagDir(exitdir) != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		/* depots can also be entered from one direction only */
		if (IsRoadDepotTile(m_new.tile)) {
			DiagDirection exitdir = GetGroundDepotDirection(m_new.tile);
			if (ReverseDiagDir(exitdir) != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
			/* don't try to enter other company's depots */
			if (GetTileOwner(m_new.tile) != m_veh->owner) {
				m_err = EC_OWNER;
				return false;
			}
		}

		/* tunnel holes and bridge ramps can be entered only from proper direction */
		assert(m_flag != TF_BRIDGE);
		assert(m_flag != TF_TUNNEL);
		if (IsTunnelTile(m_new.tile)) {
			if (GetTunnelBridgeDirection(m_new.tile) != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		} else if (IsRoadBridgeTile(m_new.tile)) {
			if (GetTunnelBridgeDirection(m_new.tile) == ReverseDiagDir(m_exitdir)) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		return true;
	}

	/** return true if we successfully reversed at end of road/track */
	inline bool CheckEndOfLine()
	{
		/* In case we can't enter the next tile, but are
		 * a normal road vehicle, then we can actually
		 * try to reverse as this is the end of the road.
		 * Trams can only turn on the appropriate bits in
		 * which case reaching this would mean a dead end
		 * near a building and in that case there would
		 * a "false" QueryNewTileTrackStatus result and
		 * as such reversing is already tried. The fact
		 * that function failed can have to do with a
		 * missing road bit, or inability to connect the
		 * different bits due to slopes. */
		if (!IsTram()) {
			/* if we reached the end of road, we can reverse the RV and continue moving */
			m_exitdir = ReverseDiagDir(m_exitdir);
			/* new tile will be the same as old one */
			m_new.set (m_old.tile, GetTrackStatusTrackdirBits(m_old.tile) & DiagdirReachesTrackdirs(m_exitdir));
			/* we always have some trackdirs reachable after reversal */
			assert(!m_new.is_empty());
			return true;
		}
		return false;
	}

	inline bool CheckStation()
	{
		return IsRoadStopTile(m_new.tile);
	}

	/** Follow a track that heads into a wormhole */
	static inline bool EnterWormhole (bool is_bridge, TileIndex other_end, uint length)
	{
		return false; // skip the wormhole
	}

	/** Follow m_old when in a wormhole */
	static inline void FollowWormhole()
	{
		NOT_REACHED();
	}

	/** Helper for pathfinders - get max speed on m_old */
	int GetSpeedLimit (void) const
	{
		int max_speed;

		/* Check for on-bridge speed limit */
		if (IsRoadBridgeTile(m_old.tile)) {
			max_speed = 2 * GetBridgeSpec(GetRoadBridgeType(m_old.tile))->speed;
		} else {
			max_speed = INT_MAX; // no limit
		}

		return max_speed;
	}
};

typedef CFollowTrack<CFollowTrackRoadBase> CFollowTrackRoad;


/**
 * Track follower water base class
 */
struct CFollowTrackWaterBase : CFollowTrackBase<ShipPathPos>
{
	const bool m_allow_90deg;

	inline bool Allow90deg() const { return m_allow_90deg; }

	inline CFollowTrackWaterBase(bool allow_90deg = true)
		: m_allow_90deg(allow_90deg)
	{
	}

	inline TrackdirBits GetTrackStatusTrackdirBits(TileIndex tile) const
	{
		return GetTileWaterwayStatus(tile);
	}

	/** check old tile */
	inline TileResult CheckOldTile()
	{
		assert(!m_old.in_wormhole());
		assert((GetTrackStatusTrackdirBits(m_old.tile) & TrackdirToTrackdirBits(m_old.td)) != 0);

		return IsAqueductTile(m_old.tile) && m_exitdir == GetTunnelBridgeDirection(m_old.tile) ?
				TR_BRIDGE : TR_NORMAL;
	}

	/** stores track status (available trackdirs) for the new tile into m_new.trackdirs */
	inline bool CheckNewTile()
	{
		TrackdirBits trackdirs = GetTrackStatusTrackdirBits(m_new.tile) & DiagdirReachesTrackdirs(m_exitdir);
		if (trackdirs == TRACKDIR_BIT_NONE) {
			m_err = EC_NO_WAY;
			return false;
		}

		m_new.set_trackdirs (trackdirs);

		/* tunnel holes and bridge ramps can be entered only from proper direction */
		assert(m_flag == TF_NONE);
		if (IsAqueductTile(m_new.tile) &&
				GetTunnelBridgeDirection(m_new.tile) == ReverseDiagDir(m_exitdir)) {
			m_err = EC_NO_WAY;
			return false;
		}

		return true;
	}

	/** return true if we successfully reversed at end of road/track */
	inline bool CheckEndOfLine()
	{
		return false;
	}

	inline bool CheckStation()
	{
		return false;
	}

	/** Follow a track that heads into a wormhole */
	static inline bool EnterWormhole (bool is_bridge, TileIndex other_end, uint length)
	{
		return false; // skip the wormhole
	}

	/** Follow m_old when in a wormhole */
	static inline void FollowWormhole()
	{
		NOT_REACHED();
	}
};

typedef CFollowTrack<CFollowTrackWaterBase> CFollowTrackWater;

#endif /* FOLLOW_TRACK_HPP */
