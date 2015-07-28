/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tracerestrict.h Header file for Trace Restriction. */

#include "stdafx.h"
#include "tracerestrict.h"
#include "train.h"
#include "core/bitmath_func.hpp"
#include "core/pool_func.hpp"
#include "command_func.h"
#include "company_func.h"
#include "viewport_func.h"
#include "window_func.h"
#include "order_base.h"
#include "pathfinder/yapf/yapf_cache.h"
#include "table/strings.h"
#include <vector>

/** Trace Restrict Data Storage Model Notes:
 *
 * Signals may have 0, 1 or 2 trace restrict programs attached to them,
 * up to one for each track. Two-way signals share the same program.
 *
 * The mapping between signals and programs is defined in terms of
 * TraceRestrictRefId to TraceRestrictProgramID,
 * where TraceRestrictRefId is formed of the tile index and track,
 * and TraceRestrictProgramID is an index into the program pool.
 *
 * If one or more mappings exist for a given signal tile, bit 12 of M3 will be set to 1.
 * This is updated whenever mappings are added/removed for that tile. This is to avoid
 * needing to do a mapping lookup for the common case where there is no trace restrict
 * program mapping for the given tile.
 *
 * Programs in the program pool are refcounted based on the number of mappings which exist.
 * When this falls to 0, the program is deleted from the pool.
 * If a program has a refcount greater than 1, it is a shared program.
 *
 * In all cases, an empty program is evaluated the same as the absence of a program.
 * Therefore it is not necessary to store mappings to empty unshared programs.
 * Any editing action which would otherwise result in a mapping to an empty program
 * which has no other references, instead removes the mapping.
 * This is not done for shared programs as this would delete the shared aspect whenever
 * the program became empty.
 *
 * Empty programs with a refcount of 1 may still exist due to the edge case where:
 * 1: There is an empty program with refcount 2
 * 2: One of the two mappings is deleted
 * Finding the other mapping would entail a linear search of the mappings, and there is little
 * to be gained by doing so.
 */

/** Initialize the program pool */
TraceRestrictProgramPool _tracerestrictprogram_pool("TraceRestrictProgram");
INSTANTIATE_POOL_METHODS(TraceRestrictProgram)

/**
 * TraceRestrictRefId --> TraceRestrictProgramID (Pool ID) mapping
 * The indirection is mainly to enable shared programs
 * TODO: use a more efficient container/indirection mechanism
 */
TraceRestrictMapping _tracerestrictprogram_mapping;

/// This should be used when all pools have been or are immediately about to be also cleared
/// Calling this at other times will leave dangling refcounts
void ClearTraceRestrictMapping() {
	_tracerestrictprogram_mapping.clear();
}

enum TraceRestrictCondStackFlags {
	TRCSF_DONE_IF         = 1<<0,       ///< The if/elif/else is "done", future elif/else branches will not be executed
	TRCSF_SEEN_ELSE       = 1<<1,       ///< An else branch has been seen already, error if another is seen afterwards
	TRCSF_ACTIVE          = 1<<2,       ///< The condition is currently active
	TRCSF_PARENT_INACTIVE = 1<<3,       ///< The parent condition is not active, thus this condition is also not active
};
DECLARE_ENUM_AS_BIT_SET(TraceRestrictCondStackFlags)

static void HandleCondition(std::vector<TraceRestrictCondStackFlags> &condstack, TraceRestrictCondFlags condflags, bool value)
{
	if (condflags & TRCF_OR) {
		assert(!condstack.empty());
		if (condstack.back() & TRCSF_ACTIVE) {
			// leave TRCSF_ACTIVE set
			return;
		}
	}

	if (condflags & (TRCF_OR | TRCF_ELSE)) {
		assert(!condstack.empty());
		if (condstack.back() & (TRCSF_DONE_IF | TRCSF_PARENT_INACTIVE)) {
			condstack.back() &= ~TRCSF_ACTIVE;
			return;
		}
	} else {
		if (!condstack.empty() && !(condstack.back() & TRCSF_ACTIVE)) {
			//this is a 'nested if', the 'parent if' is not active
			condstack.push_back(TRCSF_PARENT_INACTIVE);
			return;
		}
		condstack.push_back(static_cast<TraceRestrictCondStackFlags>(0));
	}

	if (value) {
		condstack.back() |= TRCSF_DONE_IF | TRCSF_ACTIVE;
	} else {
		condstack.back() &= ~TRCSF_ACTIVE;
	}
}

/// Test value op condvalue
static bool TestCondition(uint16 value, TraceRestrictCondOp condop, uint16 condvalue)
{
	switch (condop) {
		case TRCO_IS:
			return value == condvalue;
		case TRCO_ISNOT:
			return value != condvalue;
		case TRCO_LT:
			return value < condvalue;
		case TRCO_LTE:
			return value <= condvalue;
		case TRCO_GT:
			return value > condvalue;
		case TRCO_GTE:
			return value >= condvalue;
		default:
			NOT_REACHED();
			return false;
	}
}

/// Test order condition
/// order may be NULL
static bool TestOrderCondition(const Order *order, TraceRestrictItem item)
{
	bool result = false;

	if (order) {
		DestinationID condvalue = GetTraceRestrictValue(item);
		switch (static_cast<TraceRestrictOrderCondAuxField>(GetTraceRestrictAuxField(item))) {
			case TROCAF_STATION:
				result = order->IsType(OT_GOTO_STATION) && order->GetDestination() == condvalue;
				break;

			case TROCAF_WAYPOINT:
				result = order->IsType(OT_GOTO_WAYPOINT) && order->GetDestination() == condvalue;
				break;

			case OT_GOTO_DEPOT:
				result = order->IsType(OT_GOTO_DEPOT) && order->GetDestination() == condvalue;
				break;

			default:
				NOT_REACHED();
		}
	}

	switch (GetTraceRestrictCondOp(item)) {
		case TRCO_IS:
			return result;

		case TRCO_ISNOT:
			return !result;

		default:
			NOT_REACHED();
			return false;
	}
}

/// Execute program on train and store results in out
void TraceRestrictProgram::Execute(const Train* v, TraceRestrictProgramResult& out) const
{
	// static to avoid needing to re-alloc/resize on each execution
	static std::vector<TraceRestrictCondStackFlags> condstack;
	condstack.clear();

	size_t size = this->items.size();
	for (size_t i = 0; i < size; i++) {
		TraceRestrictItem item = this->items[i];
		TraceRestrictItemType type = GetTraceRestrictType(item);

		if (IsTraceRestrictConditional(item)) {
			TraceRestrictCondFlags condflags = GetTraceRestrictCondFlags(item);
			TraceRestrictCondOp condop = GetTraceRestrictCondOp(item);

			if (type == TRIT_COND_ENDIF) {
				assert(!condstack.empty());
				if (condflags & TRCF_ELSE) {
					// else
					assert(!(condstack.back() & TRCSF_SEEN_ELSE));
					HandleCondition(condstack, condflags, true);
					condstack.back() |= TRCSF_SEEN_ELSE;
				} else {
					// end if
					condstack.pop_back();
				}
			} else {
				uint16 condvalue = GetTraceRestrictValue(item);
				bool result = false;
				switch(type) {
					case TRIT_COND_UNDEFINED:
						result = false;
						break;

					case TRIT_COND_TRAIN_LENGTH:
						result = TestCondition(CeilDiv(v->gcache.cached_total_length, TILE_SIZE), condop, condvalue);
						break;

					case TRIT_COND_MAX_SPEED:
						result = TestCondition(v->GetDisplayMaxSpeed(), condop, condvalue);
						break;

					case TRIT_COND_CURRENT_ORDER:
						result = TestOrderCondition(&(v->current_order), item);
						break;

					case TRIT_COND_NEXT_ORDER: {
						if (v->orders.list == NULL) break;
						if (v->orders.list->GetNumOrders() == 0) break;

						const Order *current_order = v->GetOrder(v->cur_real_order_index);
						for (const Order *order = v->orders.list->GetNext(current_order); order != current_order; order = v->orders.list->GetNext(order)) {
							if (order->IsGotoOrder()) {
								result = TestOrderCondition(order, item);
								break;
							}
						}
						break;
					}

					default:
						NOT_REACHED();
				}
				HandleCondition(condstack, condflags, result);
			}
		} else {
			if (condstack.empty() || condstack.back() & TRCSF_ACTIVE) {
				switch(type) {
					case TRIT_PF_DENY:
						if (GetTraceRestrictValue(item)) {
							out.flags &= ~TRPRF_DENY;
						} else {
							out.flags |= TRPRF_DENY;
						}
						break;
					case TRIT_PF_PENALTY:
						out.penalty += GetTraceRestrictValue(item);
						break;
					default:
						NOT_REACHED();
				}
			}
		}
	}
	assert(condstack.empty());
}

void TraceRestrictProgram::DecrementRefCount() {
	assert(this->refcount > 0);
	this->refcount--;
	if (this->refcount == 0) {
		delete this;
	}
}

/// returns successful result if program seems OK
/// This only validates that conditional nesting is correct, at present
CommandCost TraceRestrictProgram::Validate(const std::vector<TraceRestrictItem> &items) {
		// static to avoid needing to re-alloc/resize on each execution
	static std::vector<TraceRestrictCondStackFlags> condstack;
	condstack.clear();

	size_t size = items.size();
	for (size_t i = 0; i < size; i++) {
		TraceRestrictItem item = items[i];
		TraceRestrictItemType type = GetTraceRestrictType(item);

		if (IsTraceRestrictConditional(item)) {
			TraceRestrictCondFlags condflags = GetTraceRestrictCondFlags(item);

			if (type == TRIT_COND_ENDIF) {
				if (condstack.empty()) {
					return_cmd_error(STR_TRACE_RESTRICT_ERROR_VALIDATE_NO_IF); // else/endif with no starting if
				}
				if (condflags & TRCF_ELSE) {
					// else
					if (condstack.back() & TRCSF_SEEN_ELSE) {
						return_cmd_error(STR_TRACE_RESTRICT_ERROR_VALIDATE_DUP_ELSE); // Two else clauses
					}
					HandleCondition(condstack, condflags, true);
					condstack.back() |= TRCSF_SEEN_ELSE;
				} else {
					// end if
					condstack.pop_back();
				}
			} else {
				if (condflags & (TRCF_OR | TRCF_ELSE)) { // elif/orif
					if (condstack.empty()) {
						return_cmd_error(STR_TRACE_RESTRICT_ERROR_VALIDATE_ELIF_NO_IF); // Pre-empt assertions in HandleCondition
					}
					if (condstack.back() & TRCSF_SEEN_ELSE) {
						return_cmd_error(STR_TRACE_RESTRICT_ERROR_VALIDATE_DUP_ELSE); // else clause followed by elif/orif
					}
				}
				HandleCondition(condstack, condflags, true);
			}
		}
	}
	if(!condstack.empty()) {
		return_cmd_error(STR_TRACE_RESTRICT_ERROR_VALIDATE_END_CONDSTACK);
	}
	return CommandCost();
}

void SetTraceRestrictValueDefault(TraceRestrictItem &item, TraceRestrictValueType value_type)
{
	switch (value_type) {
		case TRVT_NONE:
		case TRVT_INT:
		case TRVT_DENY:
		case TRVT_SPEED:
			SetTraceRestrictValue(item, 0);
			SetTraceRestrictAuxField(item, 0);
			break;

		case TRVT_ORDER:
			SetTraceRestrictValue(item, INVALID_STATION);
			SetTraceRestrictAuxField(item, TROCAF_STATION);
			break;

		default:
			NOT_REACHED();
			break;
	}
}

/// Set the type field of a TraceRestrictItem, and
/// reset any other fields which are no longer valid/meaningful
/// to sensible defaults
void SetTraceRestrictTypeAndNormalise(TraceRestrictItem &item, TraceRestrictItemType type)
{
	if (item != 0) {
		assert(GetTraceRestrictType(item) != TRIT_NULL);
		assert(IsTraceRestrictConditional(item) == IsTraceRestrictTypeConditional(type));
	}
	assert(type != TRIT_NULL);

	TraceRestrictTypePropertySet old_properties = GetTraceRestrictTypeProperties(item);
	SetTraceRestrictType(item, type);
	TraceRestrictTypePropertySet new_properties = GetTraceRestrictTypeProperties(item);

	if (old_properties.cond_type != new_properties.cond_type ||
			old_properties.value_type != new_properties.value_type) {
		SetTraceRestrictCondOp(item, TRCO_IS);
		SetTraceRestrictValueDefault(item, new_properties.value_type);
	}
}

void SetIsSignalRestrictedBit(TileIndex t)
{
	// First mapping for this tile, or later
	TraceRestrictMapping::iterator lower_bound = _tracerestrictprogram_mapping.lower_bound(MakeTraceRestrictRefId(t, static_cast<Track>(0)));

	// First mapping for next tile, or later
	TraceRestrictMapping::iterator upper_bound = _tracerestrictprogram_mapping.lower_bound(MakeTraceRestrictRefId(t + 1, static_cast<Track>(0)));

	// If iterators are the same, there are no mappings for this tile
	SetRestrictedSignal(t, lower_bound != upper_bound);
}

void TraceRestrictCreateProgramMapping(TraceRestrictRefId ref, TraceRestrictProgram *prog)
{
	std::pair<TraceRestrictMapping::iterator, bool> insert_result =
			_tracerestrictprogram_mapping.insert(std::make_pair(ref, TraceRestrictMappingItem(prog->index)));

	if (!insert_result.second) {
		// value was not inserted, there is an existing mapping
		// unref the existing mapping before updating it
		_tracerestrictprogram_pool.Get(insert_result.first->second.program_id)->DecrementRefCount();
		insert_result.first->second = prog->index;
	}
	prog->IncrementRefCount();

	TileIndex tile = GetTraceRestrictRefIdTileIndex(ref);
	Track track = GetTraceRestrictRefIdTrack(ref);
	SetIsSignalRestrictedBit(tile);
	MarkTileDirtyByTile(tile, ZOOM_LVL_DRAW_MAP);
	YapfNotifyTrackLayoutChange(tile, track);
}

void TraceRestrictRemoveProgramMapping(TraceRestrictRefId ref)
{
	TraceRestrictMapping::iterator iter = _tracerestrictprogram_mapping.find(ref);
	if (iter != _tracerestrictprogram_mapping.end()) {
		// Found
		_tracerestrictprogram_pool.Get(iter->second.program_id)->DecrementRefCount();
		_tracerestrictprogram_mapping.erase(iter);

		TileIndex tile = GetTraceRestrictRefIdTileIndex(ref);
		Track track = GetTraceRestrictRefIdTrack(ref);
		SetIsSignalRestrictedBit(tile);
		MarkTileDirtyByTile(tile, ZOOM_LVL_DRAW_MAP);
		YapfNotifyTrackLayoutChange(tile, track);
	}
}

/// Gets the trace restrict program for the tile/track ref ID identified by @p ref.
/// An empty program will be constructed if none exists, and @p create_new is true
/// unless the pool is full
TraceRestrictProgram *GetTraceRestrictProgram(TraceRestrictRefId ref, bool create_new)
{
	// Optimise for lookup, creating doesn't have to be that fast

	TraceRestrictMapping::iterator iter = _tracerestrictprogram_mapping.find(ref);
	if (iter != _tracerestrictprogram_mapping.end()) {
		// Found
		return _tracerestrictprogram_pool.Get(iter->second.program_id);
	} else if (create_new) {
		// Not found

		// Create new pool item
		if (!TraceRestrictProgram::CanAllocateItem()) {
			return NULL;
		}
		TraceRestrictProgram *prog = new TraceRestrictProgram();

		// Create new mapping to pool item
		TraceRestrictCreateProgramMapping(ref, prog);
		return prog;
	} else {
		return NULL;
	}
}

/// Notify that a signal is being removed
/// Remove any trace restrict items associated with it
void TraceRestrictNotifySignalRemoval(TileIndex tile, Track track)
{
	TraceRestrictRefId ref = MakeTraceRestrictRefId(tile, track);
	TraceRestrictRemoveProgramMapping(ref);
	DeleteWindowById(WC_TRACE_RESTRICT, ref);
}

void TraceRestrictDoCommandP(TileIndex tile, Track track, TraceRestrictDoCommandType type, uint32 offset, uint32 value, StringID error_msg)
{
	uint32 p1 = 0;
	SB(p1, 0, 3, track);
	SB(p1, 3, 5, type);
	assert(offset < (1 << 16));
	SB(p1, 8, 16, offset);
	DoCommandP(tile, p1, value, CMD_PROGRAM_TRACERESTRICT_SIGNAL | CMD_MSG(error_msg));
}

static CommandCost TraceRestrictCheckTileIsUsable(TileIndex tile, Track track)
{
	// Check that there actually is a signal here
	if (!IsPlainRailTile(tile) || !HasTrack(tile, track)) {
		return_cmd_error(STR_ERROR_THERE_IS_NO_RAILROAD_TRACK);
	}
	if (!HasSignalOnTrack(tile, track)) {
		return_cmd_error(STR_ERROR_THERE_ARE_NO_SIGNALS);
	}

	// Check tile ownership, do this afterwards to avoid tripping up on house/industry tiles
	CommandCost ret = CheckTileOwnership(tile);
	if (ret.Failed()) {
		return ret;
	}

	return CommandCost();
}

/**
 * The main command for editing a signal tracerestrict program.
 * @param tile The tile which contains the signal.
 * @param flags Internal command handler stuff.
 * Below apply for instruction modification actions only
 * @param p1 Bitstuffed items
 * @param p2 Item, for insert and modify operations
 * @return the cost of this operation (which is free), or an error
 */
CommandCost CmdProgramSignalTraceRestrict(TileIndex tile, DoCommandFlag flags, uint64 p1, uint64 p2, const char *text)
{
	TraceRestrictDoCommandType type = static_cast<TraceRestrictDoCommandType>(GB(p1, 3, 5));

	if (type >= TRDCT_PROG_COPY) {
		return CmdProgramSignalTraceRestrictProgMgmt(tile, flags, p1, p2, text);
	}

	Track track = static_cast<Track>(GB(p1, 0, 3));
	uint32 offset = GB(p1, 8, 16);
	TraceRestrictItem item = static_cast<TraceRestrictItem>(p2);

	CommandCost ret = TraceRestrictCheckTileIsUsable(tile, track);
	if (ret.Failed()) {
		return ret;
	}

	bool can_make_new = (type == TRDCT_INSERT_ITEM) && (flags & DC_EXEC);
	bool need_existing = (type != TRDCT_INSERT_ITEM);
	TraceRestrictProgram *prog = GetTraceRestrictProgram(MakeTraceRestrictRefId(tile, track), can_make_new);
	if (need_existing && !prog) {
		return_cmd_error(STR_TRACE_RESTRICT_ERROR_NO_PROGRAM);
	}

	uint32 offset_limit_exclusive = ((type == TRDCT_INSERT_ITEM) ? 1 : 0);
	if (prog) offset_limit_exclusive += prog->items.size();

	if (offset >= offset_limit_exclusive) {
		return_cmd_error(STR_TRACE_RESTRICT_ERROR_OFFSET_TOO_LARGE);
	}

	// copy program
	std::vector<TraceRestrictItem> items;
	if (prog) items = prog->items;

	switch (type) {
		case TRDCT_INSERT_ITEM:
			items.insert(items.begin() + offset, item);
			if (IsTraceRestrictConditional(item) &&
					GetTraceRestrictCondFlags(item) == 0 &&
					GetTraceRestrictType(item) != TRIT_COND_ENDIF) {
				// this is an opening if block, insert a corresponding end if
				TraceRestrictItem endif_item = 0;
				SetTraceRestrictType(endif_item, TRIT_COND_ENDIF);
				items.insert(items.begin() + offset + 1, endif_item);
			}
			break;

		case TRDCT_MODIFY_ITEM: {
			TraceRestrictItem old_item = items[offset];
			if (IsTraceRestrictConditional(old_item) != IsTraceRestrictConditional(item)) {
				return_cmd_error(STR_TRACE_RESTRICT_ERROR_CAN_T_CHANGE_CONDITIONALITY);
			}
			items[offset] = item;
			break;
		}

		case TRDCT_REMOVE_ITEM: {
			TraceRestrictItem old_item = items[offset];
			if (IsTraceRestrictConditional(old_item)) {
				bool remove_whole_block = false;
				if (GetTraceRestrictCondFlags(old_item) == 0) {
					if (GetTraceRestrictType(old_item) == TRIT_COND_ENDIF) {
						// this is an end if, can't remove these
						return_cmd_error(STR_TRACE_RESTRICT_ERROR_CAN_T_REMOVE_ENDIF);
					} else {
						// this is an opening if
						remove_whole_block = true;
					}
				}

				uint32 recursion_depth = 1;
				std::vector<TraceRestrictItem>::iterator remove_start = items.begin() + offset;
				std::vector<TraceRestrictItem>::iterator remove_end = remove_start + 1;

				// iterate until matching end block found
				for (; remove_end != items.end(); ++remove_end) {
					TraceRestrictItem current_item = *remove_end;
					if (IsTraceRestrictConditional(current_item)) {
						if (GetTraceRestrictCondFlags(current_item) == 0) {
							if (GetTraceRestrictType(current_item) == TRIT_COND_ENDIF) {
								// this is an end if
								recursion_depth--;
								if (recursion_depth == 0) {
									if (remove_whole_block) {
										// inclusively remove up to here
										++remove_end;
										break;
									} else {
										// exclusively remove up to here
										break;
									}
								}
							} else {
								// this is an opening if
								recursion_depth++;
							}
						} else {
							// this is an else/or type block
							if (recursion_depth == 1 && !remove_whole_block) {
								// exclusively remove up to here
								recursion_depth = 0;
								break;
							}
						}
					}
				}
				if (recursion_depth != 0) return CMD_ERROR; // ran off the end
				items.erase(remove_start, remove_end);
			} else {
				items.erase(items.begin() + offset);
			}
			break;
		}

		default:
			NOT_REACHED();
			break;
	}

	CommandCost validation_result = TraceRestrictProgram::Validate(items);
	if (validation_result.Failed()) {
		return validation_result;
	}

	if (flags & DC_EXEC) {
		assert(prog);

		// move in modified program
		prog->items.swap(items);

		if (prog->items.size() == 0 && prog->refcount == 1) {
			// program is empty, and this tile is the only reference to it
			// so delete it, as it's redundant
			TraceRestrictRemoveProgramMapping(MakeTraceRestrictRefId(tile, track));
		}

		// update windows
		InvalidateWindowClassesData(WC_TRACE_RESTRICT);
	}

	return CommandCost();
}

void TraceRestrictProgMgmtWithSourceDoCommandP(TileIndex tile, Track track, TraceRestrictDoCommandType type,
		TileIndex source_tile, Track source_track, StringID error_msg)
{
	uint32 p1 = 0;
	SB(p1, 0, 3, track);
	SB(p1, 3, 5, type);
	SB(p1, 8, 3, source_track);
	DoCommandP(tile, p1, source_tile, CMD_PROGRAM_TRACERESTRICT_SIGNAL | CMD_MSG(error_msg));
}

/**
 * Sub command for copy/share/unshare operations on signal tracerestrict programs.
 * @param tile The tile which contains the signal.
 * @param flags Internal command handler stuff.
 * @param p1 Bitstuffed items
 * @param p2 Source tile, for share/copy operations
 * @return the cost of this operation (which is free), or an error
 */
CommandCost CmdProgramSignalTraceRestrictProgMgmt(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	TraceRestrictDoCommandType type = static_cast<TraceRestrictDoCommandType>(GB(p1, 3, 5));
	Track track = static_cast<Track>(GB(p1, 0, 3));
	Track source_track = static_cast<Track>(GB(p1, 8, 3));
	TileIndex source_tile = static_cast<TileIndex>(p2);

	TraceRestrictRefId self = MakeTraceRestrictRefId(tile, track);
	TraceRestrictRefId source = MakeTraceRestrictRefId(source_tile, source_track);

	assert(type >= TRDCT_PROG_COPY);

	CommandCost ret = TraceRestrictCheckTileIsUsable(tile, track);
	if (ret.Failed()) {
		return ret;
	}

	if (type == TRDCT_PROG_SHARE || type == TRDCT_PROG_COPY) {
		if (self == source) {
			return_cmd_error(STR_TRACE_RESTRICT_ERROR_SOURCE_SAME_AS_TARGET);
		}

		ret = TraceRestrictCheckTileIsUsable(source_tile, source_track);
		if (ret.Failed()) {
			return ret;
		}
	}

	if (!(flags & DC_EXEC)) {
		return CommandCost();
	}

	switch (type) {
		case TRDCT_PROG_COPY: {
			TraceRestrictRemoveProgramMapping(self);
			TraceRestrictProgram *prog = GetTraceRestrictProgram(self, true);
			if (!prog) {
				// allocation failed
				return CMD_ERROR;
			}

			TraceRestrictProgram *source_prog = GetTraceRestrictProgram(source, false);
			if (source_prog) {
				prog->items = source_prog->items; // copy
			}
			break;
		}

		case TRDCT_PROG_SHARE: {
			TraceRestrictRemoveProgramMapping(self);
			TraceRestrictProgram *source_prog = GetTraceRestrictProgram(source, true);
			if (!source_prog) {
				// allocation failed
				return CMD_ERROR;
			}

			TraceRestrictCreateProgramMapping(self, source_prog);
			break;
		}

		case TRDCT_PROG_UNSHARE: {
			std::vector<TraceRestrictItem> items;
			TraceRestrictProgram *prog = GetTraceRestrictProgram(self, false);
			if (prog) {
				// copy program into temporary
				items = prog->items;
			}
			// remove old program
			TraceRestrictRemoveProgramMapping(self);

			if (items.size()) {
				// if prog is non-empty, create new program and move temporary in
				TraceRestrictProgram *new_prog = GetTraceRestrictProgram(self, true);
				if (!new_prog) {
					// allocation failed
					return CMD_ERROR;
				}

				new_prog->items.swap(items);
			}
			break;
		}

		case TRDCT_PROG_RESET: {
			TraceRestrictRemoveProgramMapping(self);
			break;
		}

		default:
			NOT_REACHED();
			break;
	}

	// update windows
	InvalidateWindowClassesData(WC_TRACE_RESTRICT);

	return CommandCost();
}

void TraceRestrictRemoveDestinationID(TraceRestrictOrderCondAuxField type, uint16 index)
{
	TraceRestrictProgram *prog;

	FOR_ALL_TRACE_RESTRICT_PROGRAMS(prog) {
		for (size_t i = 0; i < prog->items.size(); i++) {
			TraceRestrictItem &item = prog->items[i]; // note this is a reference,
			if (GetTraceRestrictType(item) == TRIT_COND_CURRENT_ORDER ||
					GetTraceRestrictType(item) == TRIT_COND_NEXT_ORDER) {
				if (GetTraceRestrictAuxField(item) == type && GetTraceRestrictValue(item) == index) {
					SetTraceRestrictValueDefault(item, TRVT_ORDER); // this updates the instruction in-place
				}
			}
		}
	}

	// update windows
	InvalidateWindowClassesData(WC_TRACE_RESTRICT);
}
