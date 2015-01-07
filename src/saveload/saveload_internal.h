/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file saveload_internal.h Declaration of functions used in more save/load files */

#ifndef SAVELOAD_INTERNAL_H
#define SAVELOAD_INTERNAL_H

#include "../company_manager_face.h"
#include "../order_base.h"
#include "../engine_type.h"
#include "saveload.h"

void InitializeOldNames();
StringID RemapOldStringID(StringID s);
char *CopyFromOldName(const SavegameTypeVersion *stv, StringID id);
void ResetOldNames();

void MoveBuoysToWaypoints();
void MoveWaypointsToBaseStations(const SavegameTypeVersion *stv);

void AfterLoadMap(const SavegameTypeVersion *stv);
void AfterLoadVehicles(const SavegameTypeVersion *stv);
void FixupTrainLengths();
void UpdateStationSpeclists();
void AfterLoadStations();
void AfterLoadLabelMaps();
void AfterLoadStoryBook(const SavegameTypeVersion *stv);
void AfterLoadLinkGraphs(const SavegameTypeVersion *stv);
void AfterLoadCompanyStats();
void UpdateHousesAndTowns();

void UpdateOldAircraft();

void SaveViewportBeforeSaveGame();
void ResetViewportAfterLoadGame();

void ConvertOldMultiheadToNew();
void ConnectMultiheadedTrains();

EngineState *AppendTempDataEngine (void);
EngineState *GetTempDataEngine (EngineID index);
void CopyTempEngineData();

extern int32 _saved_scrollpos_x;
extern int32 _saved_scrollpos_y;
extern ZoomLevelByte _saved_scrollpos_zoom;

CompanyManagerFace ConvertFromOldCompanyManagerFace(uint32 face);

BaseOrder UnpackOldOrder (uint16 packed);

#endif /* SAVELOAD_INTERNAL_H */
