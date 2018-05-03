/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file base_station_base.h Base classes/functions for base stations. */

#ifndef BASE_STATION_BASE_H
#define BASE_STATION_BASE_H

#include "core/pool_type.hpp"
#include "command_type.h"
#include "viewport_type.h"
#include "station_type.h"
#include "map/station.h"
#include "map/tilearea.h"

struct StationSpecList {
	const StationSpec *spec;
	uint32 grfid;      ///< GRF ID of this custom station
	uint8  localidx;   ///< Station ID within GRF of station
};


/** Base class for all station-ish types */
struct BaseStation : PooledItem <BaseStation, StationID, 32, 64000> {
	TileIndex xy;                   ///< Base tile of the station
	ViewportSign sign;              ///< NOSAVE: Dimensions of sign
	byte delete_ctr;                ///< Delete counter. If greater than 0 then it is decremented until it reaches 0; the waypoint is then is deleted.

	char *name;                     ///< Custom name
	StringID string_id;             ///< Default name (town area) of station

	Town *town;                     ///< The town this station is associated with
	OwnerByte owner;                ///< The owner of this station
	StationFacilityByte facilities; ///< The facilities that this station has

	uint8 num_specs;                ///< Number of specs in the speclist
	StationSpecList *speclist;      ///< List of station specs of this station

	Date build_date;                ///< Date of construction

	uint16 random_bits;             ///< Random bits assigned to this station
	byte waiting_triggers;          ///< Waiting triggers (NewGRF) for this station
	uint8 cached_anim_triggers;     ///< NOSAVE: Combined animation trigger bitmask, used to determine if trigger processing should happen.
	uint32 cached_cargo_triggers;   ///< NOSAVE: Combined cargo trigger bitmask

	TileArea train_station;         ///< Tile area the train 'station' part covers
	TileArea rect;                  ///< NOSAVE: Station spread out rectangle

	/**
	 * Initialize the base station.
	 * @param tile The location of the station sign
	 */
	BaseStation(TileIndex tile) :
		xy(tile),
		train_station(INVALID_TILE, 0, 0)
	{
	}

	virtual ~BaseStation();

	/**
	 * Check whether a specific tile belongs to this station.
	 * @param tile the tile to check
	 * @return true if the tile belongs to this station
	 */
	bool TileBelongsToStation (TileIndex tile) const
	{
		return IsStationTile(tile) && GetStationIndex(tile) == this->index;
	}

	/**
	 * Check whether a specific tile belongs to this rail station.
	 * @param tile the tile to check
	 * @return true if the tile belongs to this station
	 */
	bool TileBelongsToRailStation(TileIndex tile) const
	{
		return HasStationTileRail(tile) && GetStationIndex(tile) == this->index;
	}

	/**
	 * Helper function to get a NewGRF variable that isn't implemented by the base class.
	 * @param grffile GRF file related to this query
	 * @param variable that is queried
	 * @param parameter parameter for that variable
	 * @param available will return false if ever the variable asked for does not exist
	 * @return the value stored in the corresponding variable
	 */
	virtual uint32 GetNewGRFVariable (const struct GRFFile *grffile, byte variable, byte parameter, bool *available) const = 0;

	/**
	 * Update the coordinated of the sign (as shown in the viewport).
	 */
	virtual void UpdateVirtCoord() = 0;

	/* Test if adding an area would exceed the maximum station spread. */
	bool TestAddRect (const TileArea &ta);

	/* Update station area after removing a rectangle. */
	void AfterRemoveRect (const TileArea &ta);

	/** Update station area after removing a tile. */
	void AfterRemoveTile (TileIndex tile)
	{
		this->AfterRemoveRect (TileArea(tile));
	}

	/**
	 * Calculates the tile of the given area that is closest to a given tile.
	 * @param tile The tile from where to calculate the distance
	 * @param ta the tile area to get the closest tile of
	 * @return The tile in the area that is closest to the given tile.
	 */
	TileIndex GetClosestTile (TileIndex tile, const TileArea &ta) const
	{
		/* If the area does not have any tiles, use the station sign */
		return ta.empty() ? this->xy : ta.get_closest_tile(tile);
	}


	/**
	 * Get the base station belonging to a specific tile.
	 * @param tile The tile to get the base station from.
	 * @return the station associated with that tile.
	 */
	static inline BaseStation *GetByTile(TileIndex tile)
	{
		return BaseStation::Get(GetStationIndex(tile));
	}

	/** Check if this station is a waypoint. */
	inline bool IsWaypoint (void) const
	{
		return (this->facilities & FACIL_WAYPOINT) != 0;
	}

	/**
	 * Check whether the base station currently is in use; in use means
	 * that it is not scheduled for deletion and that it still has some
	 * facilities left.
	 * @return true if still in use
	 */
	inline bool IsInUse() const
	{
		return (this->facilities & ~FACIL_WAYPOINT) != 0;
	}

	static void PostDestructor(size_t index);
};

#define FOR_ALL_BASE_STATIONS(var) FOR_ALL_ITEMS_FROM(BaseStation, station_index, var, 0)

/**
 * Class defining several overloaded accessors so we don't
 * have to cast base stations that often
 */
template <class T, bool Tis_waypoint>
struct SpecializedStation : public BaseStation {
	static const bool IS_WAYPOINT = Tis_waypoint;

	/**
	 * Set station type correctly
	 * @param tile The base tile of the station.
	 */
	inline SpecializedStation<T, Tis_waypoint>(TileIndex tile) :
			BaseStation(tile)
	{
		this->facilities = Tis_waypoint ? FACIL_WAYPOINT : FACIL_NONE;
	}

	/**
	 * Tests whether given index is a valid index for station of this type
	 * @param index tested index
	 * @return is this index valid index of T?
	 */
	static inline bool IsValidID(size_t index)
	{
		return BaseStation::IsValidID(index) && BaseStation::Get(index)->IsWaypoint() == IS_WAYPOINT;
	}

	/**
	 * Gets station with given index
	 * @return pointer to station with given index casted to T *
	 */
	static inline T *Get(size_t index)
	{
		return (T *)BaseStation::Get(index);
	}

	/**
	 * Returns station if the index is a valid index for this station type
	 * @return pointer to station with given index if it's a station of this type
	 */
	static inline T *GetIfValid(size_t index)
	{
		return IsValidID(index) ? Get(index) : NULL;
	}

	/**
	 * Get the station belonging to a specific tile.
	 * @param tile The tile to get the station from.
	 * @return the station associated with that tile.
	 */
	static inline T *GetByTile(TileIndex tile)
	{
		return GetIfValid(GetStationIndex(tile));
	}

	/**
	 * Converts a BaseStation to SpecializedStation with type checking.
	 * @param st BaseStation pointer
	 * @return pointer to SpecializedStation
	 */
	static inline T *From(BaseStation *st)
	{
		assert(st->IsWaypoint() == IS_WAYPOINT);
		return (T *)st;
	}

	/**
	 * Converts a const BaseStation to const SpecializedStation with type checking.
	 * @param st BaseStation pointer
	 * @return pointer to SpecializedStation
	 */
	static inline const T *From(const BaseStation *st)
	{
		assert(st->IsWaypoint() == IS_WAYPOINT);
		return (const T *)st;
	}
};

#define FOR_ALL_BASE_STATIONS_OF_TYPE(name, var) FOR_ALL_ITEMS_FROM(name, station_index, var, 0) if (var->IsWaypoint() == name::IS_WAYPOINT)

#endif /* BASE_STATION_BASE_H */
