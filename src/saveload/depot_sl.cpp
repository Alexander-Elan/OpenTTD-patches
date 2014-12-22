/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot_sl.cpp Code handling saving and loading of depots */

#include "../stdafx.h"
#include "../depot_base.h"
#include "../town.h"

#include "saveload_buffer.h"

static TownID _town_index;

static const SaveLoad _depot_desc[] = {
	 SLE_VAR(Depot, xy,         SLE_FILE_U16 | SLE_VAR_U32, , ,   0,   5),
	 SLE_VAR(Depot, xy,         SLE_UINT32,                0, ,   6,    ),
	SLEG_VAR(_town_index,       SLE_UINT16,                 , ,   0, 140),
	 SLE_REF(Depot, town,       REF_TOWN,                  0, , 141,    ),
	 SLE_VAR(Depot, town_cn,    SLE_UINT16,                0, , 141,    ),
	 SLE_STR(Depot, name,       SLS_NONE,                  0, , 141,    ),
	 SLE_VAR(Depot, build_date, SLE_INT32,                 0, , 142,    ),
	 SLE_END()
};

static void Save_DEPT(SaveDumper *dumper)
{
	Depot *depot;

	FOR_ALL_DEPOTS(depot) {
		dumper->WriteElement(depot->index, depot, _depot_desc);
	}
}

static void Load_DEPT(LoadBuffer *reader)
{
	int index;

	while ((index = reader->IterateChunk()) != -1) {
		Depot *depot = new (index) Depot();
		reader->ReadObject(depot, _depot_desc);

		/* Set the town 'pointer' so we can restore it later. */
		if (reader->IsOTTDVersionBefore(141)) depot->town = (Town *)(size_t)_town_index;
	}
}

static void Ptrs_DEPT(const SavegameTypeVersion *stv)
{
	Depot *depot;

	FOR_ALL_DEPOTS(depot) {
		SlObjectPtrs(depot, _depot_desc, stv);
		if ((stv != NULL) && stv->is_ottd_before (141)) depot->town = Town::Get((size_t)depot->town);
	}
}

extern const ChunkHandler _depot_chunk_handlers[] = {
	{ 'DEPT', Save_DEPT, Load_DEPT, Ptrs_DEPT, NULL, CH_ARRAY | CH_LAST},
};
