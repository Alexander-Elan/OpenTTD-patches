/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_base.h Base classes/functions for stations. */

#ifndef STATION_BASE_H
#define STATION_BASE_H

#include "core/random_func.hpp"
#include "base_station_base.h"
#include "newgrf_airport.h"
#include "cargopacket.h"
#include "industry_type.h"
#include "linkgraph/linkgraph_type.h"
#include "newgrf_storage.h"
#include "map/tilearea.h"
#include <map>

static const byte INITIAL_STATION_RATING = 175;

/**
 * Flow statistics telling how much flow should be sent along a link. This is
 * done by creating "flow shares" and using std::map's upper_bound() method to
 * look them up with a random number. A flow share is the difference between a
 * key in a map and the previous key. So one key in the map doesn't actually
 * mean anything by itself.
 */
class FlowStat {
public:
	typedef std::map<uint32, StationID> SharesMap;

	static const SharesMap empty_sharesmap;

	/**
	 * Invalid constructor. This can't be called as a FlowStat must not be
	 * empty. However, the constructor must be defined and reachable for
	 * FlwoStat to be used in a std::map.
	 */
	inline FlowStat() {NOT_REACHED();}

	/**
	 * Create a FlowStat with an initial entry.
	 * @param st Station the initial entry refers to.
	 * @param flow Amount of flow for the initial entry.
	 * @param restricted If the flow to be added is restricted.
	 */
	inline FlowStat(StationID st, uint flow, bool restricted = false)
	{
		assert(flow > 0);
		this->shares[flow] = st;
		this->unrestricted = restricted ? 0 : flow;
	}

	/**
	 * Add some flow to the end of the shares map. Only do that if you know
	 * that the station isn't in the map yet. Anything else may lead to
	 * inconsistencies.
	 * @param st Remote station.
	 * @param flow Amount of flow to be added.
	 * @param restricted If the flow to be added is restricted.
	 */
	inline void AppendShare(StationID st, uint flow, bool restricted = false)
	{
		assert(flow > 0);
		this->shares[(--this->shares.end())->first + flow] = st;
		if (!restricted) this->unrestricted += flow;
	}

	uint GetShare(StationID st) const;

	void ChangeShare(StationID st, int flow);

	void RestrictShare(StationID st);

	void ReleaseShare(StationID st);

	void ScaleToMonthly(uint runtime);

	/**
	 * Get the actual shares as a const pointer so that they can be iterated
	 * over.
	 * @return Actual shares.
	 */
	inline const SharesMap *GetShares() const { return &this->shares; }

	/**
	 * Return total amount of unrestricted shares.
	 * @return Amount of unrestricted shares.
	 */
	inline uint GetUnrestricted() const { return this->unrestricted; }

	/**
	 * Swap the shares maps, and thus the content of this FlowStat with the
	 * other one.
	 * @param other FlowStat to swap with.
	 */
	inline void SwapShares(FlowStat &other)
	{
		this->shares.swap(other.shares);
		Swap(this->unrestricted, other.unrestricted);
	}

	/**
	 * Get a station a package can be routed to. This done by drawing a
	 * random number between 0 and sum_shares and then looking that up in
	 * the map with lower_bound. So each share gets selected with a
	 * probability dependent on its flow. Do include restricted flows here.
	 * @param is_restricted Output if a restricted flow was chosen.
	 * @return A station ID from the shares map.
	 */
	inline StationID GetViaWithRestricted(bool &is_restricted) const
	{
		assert(!this->shares.empty());
		uint rand = RandomRange((--this->shares.end())->first);
		is_restricted = rand >= this->unrestricted;
		return this->shares.upper_bound(rand)->second;
	}

	/**
	 * Get a station a package can be routed to. This done by drawing a
	 * random number between 0 and sum_shares and then looking that up in
	 * the map with lower_bound. So each share gets selected with a
	 * probability dependent on its flow. Don't include restricted flows.
	 * @return A station ID from the shares map.
	 */
	inline StationID GetVia() const
	{
		assert(!this->shares.empty());
		return this->unrestricted > 0 ?
				this->shares.upper_bound(RandomRange(this->unrestricted))->second :
				INVALID_STATION;
	}

	StationID GetVia(StationID excluded, StationID excluded2 = INVALID_STATION) const;

	void Invalidate();

private:
	SharesMap shares;  ///< Shares of flow to be sent via specified station (or consumed locally).
	uint unrestricted; ///< Limit for unrestricted shares.
};

/** Flow descriptions by origin stations. */
class FlowStatMap : public std::map<StationID, FlowStat> {
public:
	uint GetFlow() const;
	uint GetFlowVia(StationID via) const;
	uint GetFlowFrom(StationID from) const;
	uint GetFlowFromVia(StationID from, StationID via) const;

	void AddFlow(StationID origin, StationID via, uint amount);
	void PassOnFlow(StationID origin, StationID via, uint amount);
	void DeleteFlows (StationID via, StationIDStack *erased = NULL);
	void RestrictFlows(StationID via);
	void ReleaseFlows(StationID via);
	void FinalizeLocalConsumption(StationID self);
};

/**
 * Stores station stats for a single cargo.
 */
struct GoodsEntry {
	/** Status of this cargo for the station. */
	enum GoodsEntryStatus {
		/**
		 * Set when the station accepts the cargo currently for final deliveries.
		 * It is updated every STATION_ACCEPTANCE_TICKS ticks by checking surrounding tiles for acceptance >= 8/8.
		 */
		GES_ACCEPTANCE,

		/**
		 * This indicates whether a cargo has a rating at the station.
		 * Set when cargo was ever waiting at the station.
		 * It is set when cargo supplied by surrounding tiles is moved to the station, or when
		 * arriving vehicles unload/transfer cargo without it being a final delivery.
		 *
		 * This flag is cleared after 255 * STATION_RATING_TICKS of not having seen a pickup.
		 */
		GES_RATING,

		/**
		 * Set when a vehicle ever delivered cargo to the station for final delivery.
		 * This flag is never cleared.
		 */
		GES_EVER_ACCEPTED,

		/**
		 * Set when cargo was delivered for final delivery last month.
		 * This flag is set to the value of GES_CURRENT_MONTH at the start of each month.
		 */
		GES_LAST_MONTH,

		/**
		 * Set when cargo was delivered for final delivery this month.
		 * This flag is reset on the beginning of every month.
		 */
		GES_CURRENT_MONTH,

		/**
		 * Set when cargo was delivered for final delivery during the current STATION_ACCEPTANCE_TICKS interval.
		 * This flag is reset every STATION_ACCEPTANCE_TICKS ticks.
		 */
		GES_ACCEPTED_BIGTICK,
	};

	GoodsEntry() :
		status(0),
		time_since_pickup(255),
		rating(INITIAL_STATION_RATING),
		last_speed(0),
		last_age(255),
		amount_fract(0),
		link_graph(INVALID_LINK_GRAPH),
		node(INVALID_NODE),
		max_waiting_cargo(0)
	{}

	byte status; ///< Status of this cargo, see #GoodsEntryStatus.

	/**
	 * Number of rating-intervals (up to 255) since the last vehicle tried to load this cargo.
	 * The unit used is STATION_RATING_TICKS.
	 * This does not imply there was any cargo to load.
	 */
	byte time_since_pickup;

	byte rating;            ///< %Station rating for this cargo.

	/**
	 * Maximum speed (up to 255) of the last vehicle that tried to load this cargo.
	 * This does not imply there was any cargo to load.
	 * The unit used is a special vehicle-specific speed unit for station ratings.
	 *  - Trains: km-ish/h
	 *  - RV: km-ish/h
	 *  - Ships: 0.5 * km-ish/h
	 *  - Aircraft: 8 * mph
	 */
	byte last_speed;

	/**
	 * Age in years (up to 255) of the last vehicle that tried to load this cargo.
	 * This does not imply there was any cargo to load.
	 */
	byte last_age;

	byte amount_fract;      ///< Fractional part of the amount in the cargo list
	StationCargoList cargo; ///< The cargo packets of cargo waiting in this station

	LinkGraphID link_graph; ///< Link graph this station belongs to.
	NodeID node;            ///< ID of node in link graph referring to this goods entry.
	FlowStatMap flows;      ///< Planned flows through this station.
	uint max_waiting_cargo; ///< Max cargo from this station waiting at any station.

	/**
	 * Reports whether a vehicle has ever tried to load the cargo at this station.
	 * This does not imply that there was cargo available for loading. Refer to GES_RATING for that.
	 * @return true if vehicle tried to load.
	 */
	bool HasVehicleEverTriedLoading() const { return this->last_speed != 0; }

	/**
	 * Does this cargo have a rating at this station?
	 * @return true if the cargo has a rating, i.e. cargo has been moved to the station.
	 */
	inline bool HasRating() const
	{
		return HasBit(this->status, GES_RATING);
	}

	/**
	 * Get the best next hop for a cargo packet from station source.
	 * @param source Source of the packet.
	 * @return The chosen next hop or INVALID_STATION if none was found.
	 */
	inline StationID GetVia(StationID source) const
	{
		FlowStatMap::const_iterator flow_it(this->flows.find(source));
		return flow_it != this->flows.end() ? flow_it->second.GetVia() : INVALID_STATION;
	}

	/**
	 * Get the best next hop for a cargo packet from station source, optionally
	 * excluding one or two stations.
	 * @param source Source of the packet.
	 * @param excluded If this station would be chosen choose the second best one instead.
	 * @param excluded2 Second station to be excluded, if != INVALID_STATION.
	 * @return The chosen next hop or INVALID_STATION if none was found.
	 */
	inline StationID GetVia(StationID source, StationID excluded, StationID excluded2 = INVALID_STATION) const
	{
		FlowStatMap::const_iterator flow_it(this->flows.find(source));
		return flow_it != this->flows.end() ? flow_it->second.GetVia(excluded, excluded2) : INVALID_STATION;
	}
};

/** A Dock */
struct Dock : PooledItem <Dock, DockID, 32, 64000> {
	TileIndex    xy;    ///< Position on the map
	struct Dock *next;  ///< Next dock at this station

	/** Initialises a Dock */
	inline Dock (TileIndex tile = INVALID_TILE) : xy(tile) { }

	/** Check if a tile is the docking tile for this dock. */
	bool is_docking_tile (TileIndex tile)
	{
		return tile == GetDockingTile (this->xy);
	}
};

#define FOR_ALL_DOCKS_FROM(var, start) FOR_ALL_ITEMS_FROM(Dock, dock_index, var, start)
#define FOR_ALL_DOCKS(var) FOR_ALL_DOCKS_FROM(var, 0)

/** All airport-related information. Only valid if tile != INVALID_TILE. */
struct Airport : public TileArea {
	Airport() : TileArea(INVALID_TILE, 0, 0) {}

	uint64 flags;       ///< stores which blocks on the airport are taken. was 16 bit earlier on, then 32
	byte type;          ///< Type of this airport, @see AirportTypes
	byte layout;        ///< Airport layout number.
	DirectionByte rotation; ///< How this airport is rotated.

	PersistentStorage *psa; ///< Persistent storage for NewGRF airports.

	/**
	 * Get the AirportSpec that from the airport type of this airport.
	 * @return The AirportSpec for this airport.
	 */
	const AirportSpec *GetSpec() const
	{
		assert (this->tile != INVALID_TILE);
		return AirportSpec::Get(this->type);
	}

	/**
	 * Get the finite-state machine for this airport or the finite-state machine
	 * for the dummy airport in case this isn't an airport.
	 * @pre this->type < NEW_AIRPORT_OFFSET.
	 * @return The state machine for this airport.
	 */
	const AirportFTA *GetFTA() const
	{
		if (this->tile == INVALID_TILE) return &AirportFTA::dummy;
		return this->GetSpec()->fsm;
	}

	/** Check if this airport has at least one hangar. */
	inline bool HasHangar() const
	{
		return this->GetFTA()->num_hangars > 0;
	}

	/**
	 * Get the tile offset to add to the base tile of this airport for the
	 * given hangar taking rotation into account. The base tile is the
	 * northernmost tile of this airport. This function helps to make sure
	 * that getting the tile of a hangar works even for rotated airport
	 * layouts without requiring a rotated array of hangar tiles.
	 * @param h The hangar whose tile to get.
	 * @return The rotated offset.
	 */
	TileIndexDiff GetRotatedHangarDiff (const AirportFTA::Hangar *h) const
	{
		const AirportSpec *as = this->GetSpec();
		int x, y;
		switch (this->rotation) {
			case DIR_N:
				x = h->x;
				y = h->y;
				break;
			case DIR_E:
				x = h->y;
				y = as->size_x - 1 - h->x;
				break;
			case DIR_S:
				x = as->size_x - 1 - h->x;
				y = as->size_y - 1 - h->y;
				break;
			case DIR_W:
				x = as->size_y - 1 - h->y;
				y = h->x;
				break;
			default: NOT_REACHED();
		}
		return TileDiffXY (x, y);
	}

	/**
	 * Get the first tile of the given hangar.
	 * @param hangar_num The hangar to get the location of.
	 * @pre hangar_num < GetNumHangars().
	 * @return A tile with the given hangar.
	 */
	inline TileIndex GetHangarTile(uint hangar_num) const
	{
		const AirportFTA *fta = this->GetFTA();
		assert (hangar_num < fta->num_hangars);
		return this->tile + this->GetRotatedHangarDiff (&fta->hangars[hangar_num]);
	}

	const AirportFTA::Hangar *GetHangarDataByTile (TileIndex tile) const;

	/**
	 * Get the exit direction of the hangar at a specific tile.
	 * @param tile The tile to query.
	 * @pre IsHangarTile(tile).
	 * @return The exit direction of the hangar, taking airport rotation into account.
	 */
	inline Direction GetHangarExitDirection(TileIndex tile) const
	{
		const AirportFTA::Hangar *h = this->GetHangarDataByTile(tile);
		assert (h != NULL);
		const AirportSpec *as = this->GetSpec();
		return ChangeDir ((Direction)h->dir,
				DirDifference (this->rotation,
					(Direction)as->table[0]->rotation));
	}

	/** Get the number of hangars on this airport. */
	inline uint GetNumHangars() const
	{
		return this->GetFTA()->num_hangars;
	}
};

typedef SmallVector<Industry *, 2> IndustryVector;

/** Station data structure */
struct Station FINAL : SpecializedStation<Station, false> {
public:
	RoadStop *GetPrimaryRoadStop(RoadStopType type) const
	{
		return type == ROADSTOP_BUS ? bus_stops : truck_stops;
	}

	RoadStop *GetPrimaryRoadStop(const struct RoadVehicle *v) const;

	RoadStop *bus_stops;    ///< All the road stops
	TileArea bus_station;   ///< Tile area the bus 'station' part covers
	RoadStop *truck_stops;  ///< All the truck stops
	TileArea truck_station; ///< Tile area the truck 'station' part covers

	Dock *docks;            ///< All the docks
	TileArea dock_area;     ///< Tile area the docks cover

	Airport airport;        ///< Tile area the airport covers

	IndustryType indtype;   ///< Industry type to get the name from

	StationHadVehicleOfTypeByte had_vehicle_of_type;

	byte time_since_load;
	byte time_since_unload;

	byte last_vehicle_type;
	std::list<Vehicle *> loading_vehicles;
	GoodsEntry goods[NUM_CARGO];  ///< Goods at this station
	uint32 always_accepted;       ///< Bitmask of always accepted cargo types (by houses, HQs, industry tiles when industry doesn't accept cargo)

	IndustryVector industries_near; ///< Cached list of industries near the station that can accept cargo, @see DeliverGoodsToIndustry()

	Station(TileIndex tile = INVALID_TILE);
	~Station();

	void AddFacility(StationFacility new_facility_bit, TileIndex facil_xy);

	void MarkTilesDirty(bool cargo_change) const;

	void UpdateVirtCoord();

	bool CanHandleCargo (CargoID cargo) const
	{
		StationFacility f = IsCargoInClass (cargo, CC_PASSENGERS) ? ~FACIL_TRUCK_STOP : ~FACIL_BUS_STOP;
		return (this->facilities & f) != 0;
	}

	/* virtual */ uint GetPlatformLength(TileIndex tile, DiagDirection dir) const;
	/* virtual */ uint GetPlatformLength(TileIndex tile) const;
	void RecomputeIndustriesNear();
	static void RecomputeIndustriesNearForAll();

	uint GetCatchmentRadius() const;
	TileArea GetCatchmentArea() const;

	inline bool TileBelongsToRailStation(TileIndex tile) const
	{
		return IsRailStationTile(tile) && GetStationIndex(tile) == this->index;
	}

	inline bool IsDockingTile(TileIndex tile) const
	{
		Dock *d;
		for (d = this->docks; d != NULL; d = d->next) {
			if (d->is_docking_tile (tile)) return true;
		}
		return false;
	}

	inline bool TileBelongsToAirport(TileIndex tile) const
	{
		return IsAirportTile(tile) && GetStationIndex(tile) == this->index;
	}

	uint32 GetNewGRFVariable (const struct GRFFile *grffile, byte variable, byte parameter, bool *available) const OVERRIDE;
};

#define FOR_ALL_STATIONS(var) FOR_ALL_BASE_STATIONS_OF_TYPE(Station, var)

#endif /* STATION_BASE_H */
