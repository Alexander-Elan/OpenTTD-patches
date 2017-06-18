/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry_land.h Information about the behaviour of the default industry tiles. */

assert_compile (PALETTE_MODIFIER_COLOUR == 30);

#define PMC (1 << PALETTE_MODIFIER_COLOUR)
#define MS(v,n) (((uint64)(v)) << n)
#define M(ground, building, x, y, dx, dy, dz, proc) \
	(ground | MS (building, 32) | MS (x, 16) | MS (y, 19) \
		| MS (dz, 22) | MS (dx, 48) | MS (dy, 53) | MS (proc, 58))

/** Industry graphics data. */
static const uint64 industry_draw_tile_data [NEW_INDUSTRYTILEOFFSET][4] = {
	{
		M(     0xf54,      0x7db,  7, 0,  9,  9, 10, 0 ),
		M(     0xf54,      0x7dc,  7, 0,  9,  9, 30, 0 ),
		M(     0xf54,      0x7dd,  7, 0,  9,  9, 30, 0 ),
		M(     0x7e6,      0x7dd,  7, 0,  9,  9, 30, 0 ),
	}, {
		M(     0x7e6,      0x7dd,  7, 0,  9,  9, 30, 0 ),
		M(     0x7e6,      0x7de,  7, 0,  9,  9, 30, 0 ),
		M(     0x7e6,      0x7df,  7, 0,  9,  9, 30, 0 ),
		M(     0x7e6,      0x7df,  7, 0,  9,  9, 30, 0 ),
	}, {
		M(     0xf54,      0x7e0,  1, 2, 15,  9, 30, 0 ),
		M(     0xf54,      0x7e1,  1, 2, 15,  9, 30, 0 ),
		M(     0xf54,      0x7e2,  1, 2, 15,  9, 30, 0 ),
		M(     0x7e6,      0x7e2,  1, 2, 15,  9, 30, 0 ),
	}, {
		M(     0xf54,      0x7e3,  4, 4,  9,  9, 30, 0 ),
		M(     0xf54,      0x7e4,  4, 4,  9,  9, 30, 0 ),
		M(     0xf54,      0x7e5,  4, 4,  9,  9, 30, 0 ),
		M(     0x7e6,      0x7e5,  4, 4,  9,  9, 30, 0 ),
	}, {
		0xf54, 0xf54, 0x7e6, 0x7e9,
	}, {
		0xf54, 0xf54, 0x7e6, 0x7e7,
	}, {
		0xf54, 0xf54, 0x7e6, 0x7e8,
	}, {
		M(     0xf54,      0x7fd,  1, 1, 14, 14,  5, 0 ),
		M(     0xf54,      0x7fe,  1, 1, 14, 14, 44, 0 ),
		M(     0xf54,      0x7ff,  1, 1, 14, 14, 44, 0 ),
		M(     0xf54,      0x7ff,  1, 1, 14, 14, 44, 0 ),
	}, {
		M(     0xf54,      0x800,  0, 2, 16, 12,  6, 0 ),
		M(     0xf54,      0x801,  0, 2, 16, 12, 47, 0 ),
		M(     0xf54,      0x802,  0, 2, 16, 12, 50, 0 ),
		M(     0xf54,      0x802,  0, 2, 16, 12, 50, 0 ),
	}, {
		M(     0xf54,      0x803,  1, 0, 14, 15,  5, 0 ),
		M(     0xf54,      0x804,  1, 0, 14, 15, 19, 0 ),
		M(     0xf54,      0x805,  1, 0, 14, 15, 21, 0 ),
		M(     0xf54,      0x805,  1, 0, 14, 15, 21, 0 ),
	}, {
		       0xf54,
		       0xf54,
		       0xf54,
		M(     0xf54,      0x806,  1, 2, 14, 11, 32, 5 ),
	}, {
		M(     0xf54,      0x80d,  1, 0, 13, 16,  8, 0 ),
		M(     0xf54,      0x80e,  1, 0, 13, 16, 20, 0 ),
		M(     0xf54,      0x80f,  1, 0, 13, 16, 20, 0 ),
		M(     0xf54,      0x80f,  1, 0, 13, 16, 20, 0 ),
	}, {
		M(     0xf54,      0x810,  0, 1, 16, 14,  8, 0 ),
		M(     0xf54,      0x811,  0, 1, 16, 14, 21, 0 ),
		M(     0xf54,      0x812,  0, 1, 16, 14, 21, 0 ),
		M(     0xf54,      0x812,  0, 1, 16, 14, 21, 0 ),
	}, {
		M(     0xf54,      0x813,  1, 1, 14, 14, 12, 0 ),
		M(     0xf54,      0x814,  1, 1, 14, 14, 15, 0 ),
		M(     0xf54,      0x815,  1, 1, 14, 14, 22, 0 ),
		M(     0xf54,      0x815,  1, 1, 14, 14, 22, 0 ),
	}, {
		       0xf54,
		       0xf54,
		       0xf54,
		M(     0xf54,      0x816,  0, 0, 16, 15, 20, 0 ),
	}, {
		       0xf54,
		       0xf54,
		       0xf54,
		M(     0xf54,      0x817,  0, 1, 16, 13, 19, 0 ),
	}, {
		M(     0x81d,      0x818,  0, 0, 16, 16, 10, 0 ),
		M(     0x81d,      0x819,  0, 0, 16, 16, 15, 0 ),
		M(     0x81d,      0x81a,  0, 0, 16, 16, 31, 0 ),
		M(     0x81d,      0x81b,  0, 0, 16, 16, 39, 0 ),
	}, {
		M(     0x81d,      0x81c,  0, 0, 16, 16,  7, 0 ),
		M(     0x81d,      0x81c,  0, 0, 16, 16,  7, 0 ),
		M(     0x81d,      0x81c,  0, 0, 16, 16,  7, 0 ),
		M(     0x81d,      0x81c,  0, 0, 16, 16,  7, 0 ),
	}, {
		M(     0xf54,  PMC|0x81e,  1, 1, 14, 14,  4, 0 ),
		M(     0xf54,  PMC|0x81f,  1, 1, 14, 14, 24, 0 ),
		M(     0xf54,  PMC|0x820,  1, 1, 14, 14, 27, 0 ),
		M(     0x58c,  PMC|0x820,  1, 1, 14, 14, 27, 0 ),
	}, {
		M(     0xf54,  PMC|0x821,  3, 3, 10,  9,  3, 0 ),
		M(     0xf54,  PMC|0x822,  3, 3, 10,  9, 63, 0 ),
		M(     0xf54,  PMC|0x823,  3, 3, 10,  9, 62, 0 ),
		M(     0x58c,  PMC|0x823,  3, 3, 10,  9, 62, 0 ),
	}, {
		M(     0xf54,  PMC|0x824,  4, 4,  7,  7,  3, 0 ),
		M(     0xf54,  PMC|0x825,  4, 4,  7,  7, 72, 0 ),
		M(     0xf54,  PMC|0x825,  4, 4,  7,  7, 72, 0 ),
		M(     0x58c,  PMC|0x826,  4, 4,  7,  7, 80, 0 ),
	}, {
		M(     0xf54,  PMC|0x827,  2, 0, 12, 16, 51, 0 ),
		M(     0xf54,  PMC|0x828,  2, 0, 12, 16, 51, 0 ),
		M(     0xf54,  PMC|0x829,  2, 0, 12, 16, 51, 0 ),
		M(     0x58c,  PMC|0x829,  2, 0, 12, 16, 51, 0 ),
	}, {
		M(     0xf54,  PMC|0x82a,  0, 0, 16, 16, 26, 0 ),
		M(     0xf54,  PMC|0x82b,  0, 0, 16, 16, 44, 0 ),
		M(     0xf54,  PMC|0x82c,  0, 0, 16, 16, 46, 0 ),
		M(     0x58c,  PMC|0x82c,  0, 0, 16, 16, 46, 0 ),
	}, {
		M(     0xf54,      0x82d,  3, 1, 10, 13,  2, 0 ),
		M(     0xf54,      0x82e,  3, 1, 10, 13, 11, 0 ),
		M(     0xf54,      0x82f,  3, 1, 10, 13, 11, 0 ),
		M(     0x58c,      0x82f,  3, 1, 10, 13, 11, 0 ),
	}, {
		       0xfdd,
		       0xfdd,
		       0xfdd,
		       0xfdd,
	}, {
		       0xfdd,
		       0xfdd,
		       0xfdd,
		M(     0xfdd,      0x833,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0xfdd,      0x837,  0, 0, 16, 16, 20, 0 ),
		M(     0xfdd,      0x834,  0, 0, 16, 16, 20, 0 ),
		M(     0xfdd,      0x834,  0, 0, 16, 16, 20, 0 ),
		M(     0xfdd,      0x830,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0xfdd,      0x838,  0, 0, 16, 16, 20, 0 ),
		M(     0xfdd,      0x835,  0, 0, 16, 16, 20, 0 ),
		M(     0xfdd,      0x835,  0, 0, 16, 16, 20, 0 ),
		M(     0xfdd,      0x831,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0xfdd,      0x839,  0, 0, 16, 16, 20, 0 ),
		M(     0xfdd,      0x836,  0, 0, 16, 16, 20, 0 ),
		M(     0xfdd,      0x836,  0, 0, 16, 16, 20, 0 ),
		M(     0xfdd,      0x832,  0, 0, 16, 16, 20, 0 ),
	}, {
		       0x7e6,
		M(     0x87d,      0x87e,  0, 0, 16, 16, 20, 0 ),
		M(     0x87d,      0x87e,  0, 0, 16, 16, 20, 0 ),
		M(     0x87d,      0x87e,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0x87d,      0x87e,  0, 0, 16, 16, 20, 0 ),
		M(     0x87d,      0x87f,  0, 0, 16, 16, 20, 0 ),
		M(     0x87d,      0x880,  0, 0, 16, 16, 20, 0 ),
		M(     0x87d,      0x881,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0x87d,      0x882,  0, 0, 16, 16, 20, 0 ),
		M(     0x87d,      0x883,  0, 0, 16, 16, 20, 0 ),
		M(     0x87d,      0x883,  0, 0, 16, 16, 20, 0 ),
		M(     0x87d,      0x882,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0x87d,      0x881,  0, 0, 16, 16, 20, 0 ),
		M(     0x87d,      0x880,  0, 0, 16, 16, 20, 0 ),
		M(     0x87d,      0x87f,  0, 0, 16, 16, 20, 0 ),
		M(     0x87d,      0x87e,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0x83a,  PMC|0x83c,  0, 0, 16, 16, 18, 0 ),
		M(     0x83a,  PMC|0x83c,  0, 0, 16, 16, 18, 0 ),
		M(     0x83a,  PMC|0x83c,  0, 0, 16, 16, 18, 0 ),
		M(     0x83a,  PMC|0x83c,  0, 0, 16, 16, 18, 0 ),
	}, {
		M(     0x83b,  PMC|0x83d,  0, 0, 16, 16, 18, 0 ),
		M(     0x83b,  PMC|0x83d,  0, 0, 16, 16, 18, 0 ),
		M(     0x83b,  PMC|0x83d,  0, 0, 16, 16, 18, 0 ),
		M(     0x83b,  PMC|0x83d,  0, 0, 16, 16, 18, 0 ),
	}, {
		       0x7e6,
		M(     0x83e,  PMC|0x83f,  0, 0, 16, 16, 18, 0 ),
		M(     0x83e,  PMC|0x83f,  0, 0, 16, 16, 18, 0 ),
		M(     0x83e,  PMC|0x83f,  0, 0, 16, 16, 18, 0 ),
	}, {
		       0x7e6,
		M(     0x840,      0x841,  0, 0, 16, 16, 18, 0 ),
		M(     0x840,      0x841,  0, 0, 16, 16, 18, 0 ),
		M(     0x840,      0x841,  0, 0, 16, 16, 18, 0 ),
	}, {
		       0x7e6,
		M(     0x842,      0x843,  0, 0, 16, 16, 30, 0 ),
		M(     0x842,      0x843,  0, 0, 16, 16, 30, 0 ),
		M(     0x842,      0x843,  0, 0, 16, 16, 30, 0 ),
	}, {
		       0x7e6,
		M(     0x844,      0x845,  0, 0, 16, 16, 16, 0 ),
		M(     0x844,      0x845,  0, 0, 16, 16, 16, 0 ),
		M(     0x844,      0x845,  0, 0, 16, 16, 16, 0 ),
	}, {
		M(     0x7e6,      0x869,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86d,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86d,  0, 0, 16, 16, 50, 0 ),
		M( PMC|0x862,  PMC|0x866,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6,      0x86a,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86e,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86e,  0, 0, 16, 16, 50, 0 ),
		M( PMC|0x863,  PMC|0x867,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6,      0x86b,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86f,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86f,  0, 0, 16, 16, 50, 0 ),
		M( PMC|0x864,  PMC|0x868,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6,      0x86c,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x870,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x870,  0, 0, 16, 16, 50, 0 ),
		   PMC|0x865,
	}, {
		M(     0xf54,  PMC|0x871,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x875,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x875,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x879,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0xf54,  PMC|0x872,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x876,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x876,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x87a,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0xf54,  PMC|0x873,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x877,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x877,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x87b,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0xf54,  PMC|0x874,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x878,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x878,  0, 0, 16, 16, 50, 0 ),
		M(     0xf54,  PMC|0x87c,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0xf54,      0x7ea,  3, 2,  8,  8, 18, 0 ),
		M(     0xf54,      0x7eb,  3, 2,  8,  8, 37, 0 ),
		M(     0xf54,      0x7ec,  3, 2,  8,  8, 49, 0 ),
		M(     0x7e6,      0x7ec,  3, 2,  8,  8, 49, 0 ),
	}, {
		M(     0x7e6,      0x7ec,  3, 2,  8,  8, 49, 0 ),
		M(     0x7e6,      0x7ed,  3, 2,  8,  8, 49, 0 ),
		M(     0x7e6,      0x7ee,  3, 2,  8,  8, 49, 0 ),
		M(     0x7e6,      0x7ee,  3, 2,  8,  8, 49, 0 ),
	}, {
		M(     0xf54,      0x7ef,  3, 2, 10,  7, 20, 0 ),
		M(     0xf54,      0x7f0,  3, 2, 10,  7, 40, 0 ),
		M(     0xf54,      0x7f1,  3, 2, 10,  7, 40, 0 ),
		M(     0x7e6,      0x7f1,  3, 2, 10,  7, 40, 0 ),
	}, {
		M(     0xf54,      0x7f2,  4, 4,  7,  8, 22, 0 ),
		M(     0xf54,      0x7f3,  4, 4,  7,  8, 22, 0 ),
		M(     0xf54,      0x7f4,  4, 4,  7,  8, 22, 0 ),
		M(     0x7e6,      0x7f4,  4, 4,  7,  8, 22, 0 ),
	}, {
		M(     0xf54,      0x7f5,  2, 1, 11, 13, 12, 0 ),
		M(     0xf54,      0x7f6,  2, 1, 11, 13, 12, 0 ),
		M(     0xf54,      0x7f7,  2, 1, 11, 13, 12, 0 ),
		M(     0x7e6,      0x7f7,  2, 1, 11, 13, 12, 0 ),
	}, {
		M(     0x7e6,      0x85c,  0, 0,  1,  1,  1, 0 ),
		M(     0x851,      0x852,  0, 0, 16, 16, 20, 0 ),
		M(     0x851,      0x852,  0, 0, 16, 16, 20, 0 ),
		M( PMC|0x846,  PMC|0x847,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0x7e6,      0x85d,  0, 0,  1,  1,  1, 0 ),
		M(     0x853,      0x854,  0, 0, 16, 16, 20, 0 ),
		M(     0x853,      0x854,  0, 0, 16, 16, 20, 0 ),
		M( PMC|0x848,  PMC|0x849,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0x7e6,      0x85e,  0, 0,  1,  1,  1, 0 ),
		M(     0x855,      0x856,  0, 0, 16, 16, 20, 0 ),
		M(     0x855,      0x856,  0, 0, 16, 16, 20, 0 ),
		M( PMC|0x84a,  PMC|0x84b,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0x7e6,      0x85f,  0, 0,  1,  1,  1, 0 ),
		       0x857,
		       0x857,
		   PMC|0x84c,
	}, {
		M(     0x7e6,      0x860,  0, 0,  1,  1,  1, 0 ),
		M(     0x858,      0x859,  0, 0, 16, 16, 20, 0 ),
		M(     0x858,      0x859,  0, 0, 16, 16, 20, 0 ),
		M( PMC|0x84d,  PMC|0x84e,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0x7e6,      0x861,  0, 0,  1,  1,  1, 0 ),
		M(     0x85a,      0x85b,  0, 0, 16, 16, 20, 0 ),
		M(     0x85a,      0x85b,  0, 0, 16, 16, 20, 0 ),
		M( PMC|0x84f,  PMC|0x850,  0, 0, 16, 16, 20, 0 ),
	}, {
		M(     0x7e6,      0x884,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x884,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x884,  0, 0, 16, 16, 25, 0 ),
		M(     0x886,      0x884,  0, 0, 16, 16, 25, 0 ),
	}, {
		M(     0x7e6,      0x885,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x885,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x885,  0, 0, 16, 16, 25, 0 ),
		M(     0x887,      0x885,  0, 0, 16, 16, 25, 0 ),
	}, {
		M(     0x7e6,  PMC|0x88c,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x88d,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x88d,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x88e,  0, 0, 16, 16, 25, 0 ),
	}, {
		M(     0x7e6,  PMC|0x88f,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x890,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x890,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x891,  0, 0, 16, 16, 25, 0 ),
	}, {
		M(     0x7e6,  PMC|0x892,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x893,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x893,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x894,  0, 0, 16, 16, 25, 0 ),
	}, {
		M(     0x7e6,  PMC|0x895,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x896,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x896,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,  PMC|0x897,  0, 0, 16, 16, 25, 0 ),
	}, {
		M(     0x7e6,      0x898,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x899,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x899,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x89a,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x7e6,
		       0x7e6,
		       0x7e6,
		M(     0x7e6,      0x8a6,  0, 0, 16, 16, 25, 0 ),
	}, {
		M(     0x7e6,      0x89b,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x89c,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x89c,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x89d,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x7e6,
		       0x7e6,
		       0x7e6,
		M(     0x7e6,      0x89e,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x7e6,
		M(     0x7e6,      0x89f,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8a0,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8a0,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x7e6,
		M(     0x7e6,      0x89f,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8a0,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8a1,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x7e6,
		M(     0x7e6,      0x8a2,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8a3,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8a4,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x7e6,
		M(     0x7e6,      0x8a2,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8a3,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8a5,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x8a7,
		M(     0x7e6,      0x8b7,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8b7,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8c7,  0, 0, 16, 16, 25, 0 ),
	}, {
		0x8a8, 0x8b8, 0x8b8, 0x8c8,
	}, {
		       0x8a9,
		M(     0x7e6,      0x8b9,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8b9,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8c9,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x8aa,
		M(     0x7e6,      0x8ba,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8ba,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8ca,  0, 0, 16, 16, 25, 0 ),
	}, {
		0x8ab, 0x8bb, 0x8bb, 0x8cb,
	}, {
		0x8ac, 0x8bc, 0x8bc, 0x8cc,
	}, {
		0x8ad, 0x8bd, 0x8bd, 0x8cd,
	}, {
		       0x8ae,
		       0x8be,
		       0x8be,
		M(     0x8ce,      0x8d7,  0, 0, 16, 16, 35, 0 ),
	}, {
		0x8af, 0x8bf, 0x8bf, 0x8cf,
	}, {
		0x8b0, 0x8c0, 0x8c0, 0x8d0,
	}, {
		0x8b1, 0x8c1, 0x8c1, 0x8d1,
	}, {
		0x8b2, 0x8c2, 0x8c2, 0x8d2,
	}, {
		0x8b3, 0x8c3, 0x8c3, 0x8d3,
	}, {
		0x8b4, 0x8c4, 0x8c4, 0x8d4,
	}, {
		0x8b5, 0x8c5, 0x8c5, 0x8d5,
	}, {
		0x8b6, 0x8c6, 0x8c6, 0x8d6,
	}, {
		M(     0x8ce,      0x8d7,  0, 0, 16, 16, 35, 0 ),
		M(     0x8ce,      0x8d8,  0, 0, 16, 16, 35, 0 ),
		M(     0x8ce,      0x8d9,  0, 0, 16, 16, 35, 0 ),
		M(     0x8ce,      0x8d9,  0, 0, 16, 16, 35, 0 ),
	}, {
		M(     0x7e6,      0x88a,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x88a,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x88a,  0, 0, 16, 16, 25, 0 ),
		M(     0x888,      0x88a,  0, 0, 16, 16, 25, 0 ),
	}, {
		M(     0x7e6,      0x88b,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x88b,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x88b,  0, 0, 16, 16, 25, 0 ),
		M(     0x889,      0x88b,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x8da,
		M(     0x7e6,      0x8e3,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8e3,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8ec,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x8db,
		M(     0x7e6,      0x8e4,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8e4,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8ed,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x8dc,
		M(     0x7e6,      0x8e5,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8e5,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8ee,  0, 0, 16, 16, 25, 0 ),
	}, {
		       0x8dd,
		M(     0x7e6,      0x8e6,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8e6,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8ef,  0, 0, 16, 16, 25, 0 ),
	}, {
		0x8de, 0x8e7, 0x8e7, 0x8f0,
	}, {
		0x8df, 0x8e8, 0x8e8, 0x8f1,
	}, {
		       0x8e0,
		M(     0x7e6,      0x8e9,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8e9,  0, 0, 16, 16, 25, 0 ),
		M(     0x7e6,      0x8f2,  0, 0, 16, 16, 25, 0 ),
	}, {
		0x8e1, 0x8ea, 0x8ea, 0x8f3,
	}, {
		0x8e2, 0x8eb, 0x8eb, 0x8f4,
	}, {
		0x8f5, 0x905, 0x905, 0x915,
	}, {
		0x8f6, 0x906, 0x906, 0x916,
	}, {
		0x8f7, 0x907, 0x907, 0x917,
	}, {
		0x8f8, 0x908, 0x908, 0x918,
	}, {
		0x8f9, 0x909, 0x909, 0x919,
	}, {
		0x8fa, 0x90a, 0x90a, 0x91a,
	}, {
		0x8fb, 0x90b, 0x90b, 0x91b,
	}, {
		0x8fc, 0x90c, 0x90c, 0x91c,
	}, {
		0x8fd, 0x90d, 0x90d, 0x91d,
	}, {
		0x8fe, 0x90e, 0x90e, 0x91e,
	}, {
		0x8ff, 0x90f, 0x90f, 0x91f,
	}, {
		0x900, 0x910, 0x910, 0x920,
	}, {
		0x901, 0x911, 0x911, 0x921,
	}, {
		0x902, 0x912, 0x912, 0x922,
	}, {
		0x903, 0x913, 0x913, 0x923,
	}, {
		0x904, 0x914, 0x914, 0x924,
	}, {
		       0x925,
		       0x925,
		M(     0x925,      0x926,  0, 0, 16, 16, 30, 0 ),
		M(     0x925,      0x926,  0, 0, 16, 16, 30, 0 ),
	}, {
		       0x925,
		       0x925,
		M(     0x925,      0x927,  0, 0, 16, 16, 30, 0 ),
		M(     0x925,      0x927,  0, 0, 16, 16, 30, 0 ),
	}, {
		M(    0x11c6,  PMC|0x92b,  0, 0, 16, 16, 25, 0 ),
		M(    0x11c6,  PMC|0x92c,  0, 0, 16, 16, 25, 0 ),
		M(    0x11c6,  PMC|0x92c,  0, 0, 16, 16, 25, 0 ),
		M(    0x11c6,  PMC|0x92d,  0, 0, 16, 16, 25, 0 ),
	}, {
		M(    0x11c6,  PMC|0x92e,  0, 0, 16, 16, 25, 0 ),
		M(    0x11c6,  PMC|0x92f,  0, 0, 16, 16, 25, 0 ),
		M(    0x11c6,  PMC|0x92f,  0, 0, 16, 16, 25, 0 ),
		M(    0x11c6,  PMC|0x930,  0, 0, 16, 16, 25, 0 ),
	}, {
		M(    0x11c6,  PMC|0x928,  0, 0, 16, 16, 25, 0 ),
		M(    0x11c6,  PMC|0x929,  0, 0, 16, 16, 25, 0 ),
		M(    0x11c6,  PMC|0x929,  0, 0, 16, 16, 25, 0 ),
		M(    0x11c6,  PMC|0x92a,  0, 0, 16, 16, 25, 0 ),
	}, {
		M(     0x7e6,      0x869,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86d,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86d,  0, 0, 16, 16, 50, 0 ),
		M( PMC|0x862,  PMC|0x866,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6,      0x86a,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86e,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86e,  0, 0, 16, 16, 50, 0 ),
		M( PMC|0x863,  PMC|0x867,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6,      0x86b,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86f,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x86f,  0, 0, 16, 16, 50, 0 ),
		M( PMC|0x864,  PMC|0x868,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6,      0x86c,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x870,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x870,  0, 0, 16, 16, 50, 0 ),
		   PMC|0x865,
	}, {
		M(     0x7e6,      0x931,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x935,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x935,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x939,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6,      0x932,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x936,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x936,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x93a,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6,      0x933,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x937,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x937,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x93b,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6,      0x934,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x938,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x938,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,      0x93c,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x81d,      0x818,  0, 0, 16, 16, 10, 0 ),
		M(     0x81d,      0x819,  0, 0, 16, 16, 15, 0 ),
		M(     0x81d,      0x81a,  0, 0, 16, 16, 31, 0 ),
		M(     0x81d,      0x81b,  0, 0, 16, 16, 39, 0 ),
	}, {
		M(     0x81d,      0x81c,  0, 0, 16, 16,  7, 0 ),
		M(     0x81d,      0x81c,  0, 0, 16, 16,  7, 0 ),
		M(     0x81d,      0x81c,  0, 0, 16, 16,  7, 0 ),
		M(     0x81d,      0x81c,  0, 0, 16, 16,  7, 0 ),
	}, {
		0x7e6, 0x7e6, 0x7e6, 0x7e6,
	}, {
		M(     0x7e6, PMC|0x1245,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6, PMC|0x1248,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6, PMC|0x1248,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6, PMC|0x124b,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6, PMC|0x1247,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6, PMC|0x124a,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6, PMC|0x124a,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6, PMC|0x124d,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6, PMC|0x1246,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6, PMC|0x1249,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6, PMC|0x1249,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6, PMC|0x124c,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x81d,     0x124e,  0, 0, 16, 16, 10, 0 ),
		M(     0x81d,     0x124f,  0, 0, 16, 16, 10, 0 ),
		M(     0x81d,     0x1250,  0, 0, 16, 16, 10, 0 ),
		M(     0x81d,     0x1251,  0, 0, 16, 16, 10, 0 ),
	}, {
		M(     0x81d,     0x1252,  0, 0, 16, 16, 10, 0 ),
		M(     0x81d,     0x1252,  0, 0, 16, 16, 10, 0 ),
		M(     0x81d,     0x1252,  0, 0, 16, 16, 10, 0 ),
		M(     0x81d,     0x1252,  0, 0, 16, 16, 10, 0 ),
	}, {
		M(     0x81d,     0x1253,  0, 0, 16, 16, 10, 0 ),
		M(     0x81d,     0x1254,  0, 0, 16, 16, 10, 0 ),
		M(     0x81d,     0x1254,  0, 0, 16, 16, 10, 0 ),
		M(     0x81d,     0x1255,  0, 0, 16, 16, 10, 0 ),
	}, {
		0x7e6, 0x7e6, 0x7e6, 0x7e6,
	}, {
		M(     0x7e6,     0x125b,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,     0x125e,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,     0x125e,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,     0x1261,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6,     0x125c,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,     0x125f,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,     0x125f,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,     0x1262,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0x7e6,     0x125d,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,     0x1260,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,     0x1260,  0, 0, 16, 16, 50, 0 ),
		M(     0x7e6,     0x1263,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1243,
		M(    0x1243,     0x1264,  0, 0, 16, 16, 50, 0 ),
		M(    0x1243,     0x1264,  0, 0, 16, 16, 50, 0 ),
		M(    0x1243,     0x1268,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1243,
		M(    0x1243,     0x1265,  0, 0, 16, 16, 50, 0 ),
		M(    0x1243,     0x1265,  0, 0, 16, 16, 50, 0 ),
		M(    0x1243,     0x1269,  0, 0, 16, 16, 50, 4 ),
	}, {
		      0x1243,
		M(    0x1243,     0x1266,  0, 0, 16, 16, 50, 0 ),
		M(    0x1243,     0x1266,  0, 0, 16, 16, 50, 0 ),
		M(    0x1243,     0x126a,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1243,
		M(    0x1243,     0x1267,  0, 0, 16, 16, 50, 0 ),
		M(    0x1243,     0x1267,  0, 0, 16, 16, 50, 0 ),
		M(    0x1243,     0x126b,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1243,
		      0x1243,
		      0x1243,
		M(    0x1243,     0x126c,  0, 0, 16, 16, 50, 0 ),
	}, {
		0x1243, 0x1243, 0x1243, 0x1243,
	}, {
		      0x1271,
		      0x1271,
		      0x1271,
		M(    0x1271,     0x1279,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1272,
		      0x1272,
		      0x1272,
		M(    0x1272,     0x127a,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1273,
		      0x1273,
		      0x1273,
		M(    0x1273,     0x127b,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1274,
		      0x1274,
		      0x1274,
		M(    0x1274,     0x127c,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1275,
		      0x1275,
		      0x1275,
		M(    0x1275,     0x127d,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1276,
		      0x1276,
		      0x1276,
		M(    0x1276,     0x127e,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1277,
		      0x1277,
		      0x1277,
		M(    0x1277,     0x127f,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1278,
		      0x1278,
		      0x1278,
		M(    0x1278,     0x1280,  0, 0, 16, 16, 50, 0 ),
	}, {
		0x1244, 0x1244, 0x1244, 0x1244,
	}, {
		      0x1244,
		      0x1244,
		      0x1244,
		M(    0x1244, PMC|0x1284,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1244,
		M(    0x1244, PMC|0x1283,  0, 0, 16, 16, 50, 0 ),
		M(    0x1244, PMC|0x1283,  0, 0, 16, 16, 50, 0 ),
		M(    0x1244, PMC|0x1286,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(    0x1244, PMC|0x1281,  0, 0, 16, 16, 50, 0 ),
		M(    0x1244, PMC|0x1282,  0, 0, 16, 16, 50, 0 ),
		M(    0x1244, PMC|0x1282,  0, 0, 16, 16, 50, 0 ),
		M(    0x1244, PMC|0x1285,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1243,
		M(    0x1243, PMC|0x1287,  0, 0, 16, 16, 50, 0 ),
		M(    0x1243, PMC|0x1287,  0, 0, 16, 16, 50, 0 ),
		M(    0x1243, PMC|0x1287,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1244,
		M(    0x1244, PMC|0x1288,  0, 0, 16, 16, 50, 0 ),
		M(    0x1244, PMC|0x1288,  0, 0, 16, 16, 50, 0 ),
		M(    0x1244, PMC|0x1288,  0, 0, 16, 16, 50, 0 ),
	}, {
		      0x1243,
		M(    0x1243, PMC|0x1289,  0, 0, 16, 16, 50, 3 ),
		M(    0x1243, PMC|0x1289,  0, 0, 16, 16, 50, 3 ),
		M(    0x1243, PMC|0x1289,  0, 0, 16, 16, 50, 3 ),
	}, {
		0x1244, 0x1244, 0x1244, 0x1244,
	}, {
		M(     0xf8d,     0x129b,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d,     0x129b,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d,     0x129b,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d,     0x129b,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0xf8d,     0x129c,  0, 0, 16, 16, 50, 2 ),
		M(     0xf8d,     0x129c,  0, 0, 16, 16, 50, 2 ),
		M(     0xf8d,     0x129c,  0, 0, 16, 16, 50, 2 ),
		M(     0xf8d,     0x129c,  0, 0, 16, 16, 50, 2 ),
	}, {
		M(     0xf8d,     0x129d,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d,     0x129d,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d,     0x129d,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d,     0x129d,  0, 0, 16, 16, 50, 0 ),
	}, {
		PMC|0x12a0, PMC|0x12a0, PMC|0x12a0, PMC|0x12a0,
	}, {
		PMC|0x12a1, PMC|0x12a1, PMC|0x12a1, PMC|0x12a1,
	}, {
		PMC|0x12a2, PMC|0x12a2, PMC|0x12a2, PMC|0x12a2,
	}, {
		PMC|0x12a3, PMC|0x12a3, PMC|0x12a3, PMC|0x12a3,
	}, {
		0xf8d, 0xf8d, 0xf8d, 0xf8d,
	}, {
		M(     0xf8d, PMC|0x12a4,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d, PMC|0x12a4,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d, PMC|0x12a4,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d, PMC|0x12a4,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0xf8d, PMC|0x12a6,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d, PMC|0x12a6,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d, PMC|0x12a6,  0, 0, 16, 16, 50, 0 ),
		M(     0xf8d, PMC|0x12a6,  0, 0, 16, 16, 50, 0 ),
	}, {
		M(     0xf8d, PMC|0x12a5,  0, 0, 16, 16, 50, 1 ),
		M(     0xf8d, PMC|0x12a5,  0, 0, 16, 16, 50, 1 ),
		M(     0xf8d, PMC|0x12a5,  0, 0, 16, 16, 50, 1 ),
		M(     0xf8d, PMC|0x12a5,  0, 0, 16, 16, 50, 1 ),
	}
};

#undef M
#undef MS
#undef PMC
