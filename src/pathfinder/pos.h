/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pathfinder/pos.h Path position types. */

#ifndef PATHFINDER_POS_H
#define PATHFINDER_POS_H

#include "../map/coord.h"
#include "../track_type.h"
#include "../track_func.h"

/**
 * Path tile (real map tile or virtual tile in wormhole)
 */
struct PathTile {
	TileIndex tile;
	TileIndex wormhole;

	/** Create a PathTile */
	PathTile(TileIndex t = INVALID_TILE, TileIndex w = INVALID_TILE)
		: tile(t), wormhole(w) { }

	/** Set this tile to another given tile */
	void set (const PathTile &tile) { *this = tile; }

	/** Set this tile to a given tile */
	void set (TileIndex t, TileIndex w = INVALID_TILE)
	{
		tile = t;
		wormhole = w;
	}

	/** Check if this tile is in a wormhole */
	bool in_wormhole() const { return wormhole != INVALID_TILE; }

	/** Compare with another tile */
	bool operator == (const PathTile &other) const
	{
		return (tile == other.tile) && (wormhole == other.wormhole);
	}

	/** Compare with another tile */
	bool operator != (const PathTile &other) const
	{
		return (tile != other.tile) || (wormhole != other.wormhole);
	}
};

/**
 * Path position (tile and trackdir)
 */
struct PathPos : PathTile {
	Trackdir td;

	/** Create an empty PathPos */
	PathPos() : PathTile(), td(INVALID_TRACKDIR) { }

	/** Create a PathPos for a given tile and trackdir */
	PathPos(TileIndex t, Trackdir d) : PathTile(t), td(d) { }

	/** Create a PathPos in a wormhole */
	PathPos(TileIndex t, Trackdir d, TileIndex w) : PathTile(t, w), td(d) { }

	/** Set this position to another given position */
	void set (const PathPos &pos)
	{
		PathTile::set(pos);
		td = pos.td;
	}

	/** Set this position to a given tile and trackdir */
	void set (TileIndex t, Trackdir d)
	{
		PathTile::set(t);
		td = d;
	}

	/** Set this position to a given wormhole position */
	void set (TileIndex t, Trackdir d, TileIndex w)
	{
		PathTile::set (t, w);
		td = d;
	}

	/** Set the tile of this position to a given tile */
	void set_tile (TileIndex t)
	{
		PathTile::set(t);
		td = INVALID_TRACKDIR; // trash previous trackdir
	}

	/** Set the tile of this position to a given tile */
	void set_tile (TileIndex t, TileIndex w)
	{
		PathTile::set (t, w);
		td = INVALID_TRACKDIR; // trash previous trackdir
	}

	/** Compare with another PathPos */
	bool operator == (const PathPos &other) const
	{
		return PathTile::operator==(other) && (td == other.td);
	}

	/** Compare with another PathPos */
	bool operator != (const PathPos &other) const
	{
		return PathTile::operator!=(other) || (td != other.td);
	}
};

/**
 * Pathfinder new position; td will be INVALID_TRACKDIR unless trackdirs has exactly one trackdir set
 */
struct PathMPos : PathPos {
	TrackdirBits trackdirs;

	/** Set trackdirs to a given set */
	void set_trackdirs (TrackdirBits s)
	{
		assert (tile != INVALID_TILE); // tile should be already set
		trackdirs = s;
		td = HasExactlyOneBit(s) ? FindFirstTrackdir(s) : INVALID_TRACKDIR;
	}

	/** Set trackdirs to a single trackdir */
	void set_trackdir (Trackdir d)
	{
		assert (tile != INVALID_TILE); // tile should be already set
		td = d;
		trackdirs = TrackdirToTrackdirBits(d);
	}

	/** Clear trackdirs */
	void clear_trackdirs()
	{
		assert (tile != INVALID_TILE); // tile should be already set
		trackdirs = TRACKDIR_BIT_NONE;
		td = INVALID_TRACKDIR;
	}

	/** Check whether the position has no trackdirs */
	bool is_empty() const
	{
		return trackdirs == TRACKDIR_BIT_NONE;
	}

	/** Check whether the position has exactly one trackdir */
	bool is_single() const
	{
		assert (HasExactlyOneBit(trackdirs) == (td != INVALID_TRACKDIR));
		return td != INVALID_TRACKDIR;
	}
};

#endif /* PATHFINDER_POS_H */
