/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf.hpp Base includes/functions for YAPF. */

#ifndef YAPF_HPP
#define YAPF_HPP

#include "../../landscape.h"
#include "../pf_performance_timer.hpp"
#include "yapf.h"

//#undef FORCEINLINE
//#define inline inline

#include "../../debug.h"
#include "../../misc/dbg_helpers.h"
#include "../../settings_type.h"
#include "astar.hpp"
#include "../pos.h"
#include "../follow_track.hpp"

extern int _total_pf_time_us;

/** Yapf Node Key base class. */
template <class PPos>
struct CYapfNodeKey : PPos {
	typedef PPos Pos;

	inline void Set(const Pos &pos)
	{
		Pos::set(pos);
	}

	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteTile("m_tile", Pos::tile);
		dmp.WriteEnumT("m_td", Pos::td);
	}
};

/** Yapf Node Key that evaluates hash from (and compares) tile & exit dir. */
template <class PPos>
struct CYapfNodeKeyExitDir : public CYapfNodeKey<PPos> {
	DiagDirection  exitdir;

	inline void Set(const PPos &pos)
	{
		CYapfNodeKey<PPos>::Set(pos);
		exitdir = (pos.td == INVALID_TRACKDIR) ? INVALID_DIAGDIR : TrackdirToExitdir(pos.td);
	}

	inline int CalcHash() const
	{
		return exitdir | (PPos::tile << 2);
	}

	inline bool operator == (const CYapfNodeKeyExitDir &other) const
	{
		return PPos::PathTile::operator==(other) && (exitdir == other.exitdir);
	}

	void Dump(DumpTarget &dmp) const
	{
		CYapfNodeKey<PPos>::Dump(dmp);
		dmp.WriteEnumT("m_exitdir", exitdir);
	}
};

/** Yapf Node Key that evaluates hash from (and compares) tile & track dir. */
template <class PPos>
struct CYapfNodeKeyTrackDir : public CYapfNodeKey<PPos> {
	inline int CalcHash() const
	{
		return (PPos::in_wormhole() ? (PPos::td + 6) : PPos::td) | (PPos::tile << 4);
	}

	inline bool operator == (const CYapfNodeKeyTrackDir &other) const
	{
		return PPos::PathTile::operator==(other) && (PPos::td == other.td);
	}
};

/** Yapf Node base */
template <class Tkey_, class Tnode>
struct CYapfNodeT : AstarNodeBase<Tnode> {
	typedef AstarNodeBase<Tnode> ABase;
	typedef Tkey_ Key;
	typedef typename Key::Pos Pos;
	typedef Tnode Node;

	Tkey_       m_key;

	inline void Set(Node *parent, const Pos &pos)
	{
		ABase::Set (parent);
		m_key.Set(pos);
	}

	inline const Pos& GetPos() const
	{
		return m_key;
	}

	inline const Tkey_& GetKey() const
	{
		return m_key;
	}

	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteStructT("m_parent", ABase::m_parent);
		dmp.WriteLine("m_cost = %d", ABase::m_cost);
		dmp.WriteLine("m_estimate = %d", ABase::m_estimate);
		dmp.WriteStructT("m_key", &m_key);
	}
};

/** Cost estimation helper. */
static inline int YapfCalcEstimate (TileIndex src, DiagDirection dir, TileIndex dst)
{
	static const int dg_dir_to_x_offs[] = {-1, 0, 1, 0};
	static const int dg_dir_to_y_offs[] = {0, 1, 0, -1};

	int x1 = 2 * TileX(src) + dg_dir_to_x_offs[(int)dir];
	int y1 = 2 * TileY(src) + dg_dir_to_y_offs[(int)dir];
	int x2 = 2 * TileX(dst);
	int y2 = 2 * TileY(dst);
	int dx = abs(x1 - x2);
	int dy = abs(y1 - y2);
	int dmin = min(dx, dy);
	int dxy = abs(dx - dy);
	return dmin * YAPF_TILE_CORNER_LENGTH + (dxy - 1) * (YAPF_TILE_LENGTH / 2);
}

/** Cost estimation helper. */
template <class Pos>
static inline int YapfCalcEstimate (const Pos &pos, TileIndex dst)
{
	return YapfCalcEstimate (pos.tile, TrackdirToExitdir(pos.td), dst);
}

#endif /* YAPF_HPP */
