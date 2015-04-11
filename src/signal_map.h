/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file signal_map.h Slightly cooked access to signals on the map */

#ifndef SIGNAL_MAP_H
#define SIGNAL_MAP_H

#include "stdafx.h"
#include "signal_type.h"
#include "map/rail.h"
#include "pathfinder/railpos.h"

/**
 * Sets the state of the signal along the given trackdir.
 */
static inline void SetSignalState(TileIndex tile, Trackdir trackdir, SignalState state)
{
	if (IsRailwayTile(tile)) {
		SetSignalStateByTrackdir(tile, trackdir, state);
	} else {
		maptile_set_tunnel_signal_state(tile, TrackdirToExitdir(trackdir) == GetTunnelBridgeDirection(tile), state);
	}
}


/**
 * Is a pbs signal present along the trackdir?
 * @param tile the tile to check
 * @param td the trackdir to check
 */
static inline bool HasPbsSignalOnTrackdir(TileIndex tile, Trackdir td)
{
	return IsRailwayTile(tile) ?
			HasSignalOnTrackdir(tile, td) && IsPbsSignal(GetSignalType(tile, TrackdirToTrack(td))) :
			maptile_is_rail_tunnel(tile) && maptile_has_tunnel_signal(tile, TrackdirToExitdir(td) == GetTunnelBridgeDirection(tile)) && IsPbsSignal(maptile_get_tunnel_signal_type(tile));
}


/**
 * Is a one-way signal blocking the trackdir? A one-way signal on the
 * trackdir against will block, but signals on both trackdirs won't.
 * @param pos the position to check
 */
static inline bool HasOnewaySignalBlockingPos(const RailPathPos &pos)
{
	if (pos.in_wormhole()) return false;

	if (IsRailwayTile (pos.tile)) {
		return HasSignalOnTrackdir (pos.tile, ReverseTrackdir (pos.td)) &&
			!HasSignalOnTrackdir (pos.tile, pos.td) && IsOnewaySignal (GetSignalType (pos.tile, TrackdirToTrack (pos.td)));
	} else if (maptile_is_rail_tunnel (pos.tile)) {
		return maptile_has_tunnel_signal (pos.tile, TrackdirToExitdir (pos.td) != GetTunnelBridgeDirection (pos.tile));
	} else {
		return false;
	}
}

#endif /* SIGNAL_MAP_H */
