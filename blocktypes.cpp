/* copyright (c) David M. Lewis 1987 */

#include <stdio.h>

#include "parser.h"
#include "tortle_types.h"

#include "blocktypes.h"
#include "make_blocks.h"
#include "logic.h"
#include "codegen_logic.h"


makeblockrec init_block_types [] =
{
	{"and",				(make_block_fn)NULL,	andfn,				codegen_andfn,		-1,		1},
	{"nand",			(make_block_fn)NULL,	nandfn,				codegen_nandfn,		-1,		1},
	{"or",				(make_block_fn)NULL,	orfn,				codegen_orfn,		-1,		1},
	{"nor",				(make_block_fn)NULL,	norfn,				codegen_norfn,		-1,		1},
	{"xor",				(make_block_fn)NULL,	xorfn,				codegen_xorfn,		-1,		1},
	{"eqeq",			(make_block_fn)NULL,	eqeqfn,				codegen_eqeqfn,		-1,		1},
	{"greater",			(make_block_fn)NULL,	greaterfn,			codegen_greaterfn,		2,		1},
	{"less",			(make_block_fn)NULL,	lessfn,				codegen_lessfn,		2,		1},
	{"leq",				(make_block_fn)NULL,	leqfn,				codegen_leqfn,		2,		1},
	{"geq",				(make_block_fn)NULL,	geqfn,				codegen_geqfn,		2,		1},
	{"eq",				(make_block_fn)NULL,	eqfn,				codegen_eqfn,		2,		1},
	{"neq",				(make_block_fn)NULL,	neqfn,				codegen_neqfn,		2,		1},
	{"inv",				(make_block_fn)NULL,	nandfn,				codegen_nandfn,		1,		1},
	{"priority",		(make_block_fn)NULL,	priorityfn,			codegen_priorityfn,		1,		2},
	{"ram",				make_ram_fn,			memfn,				codegen_memfn},
	{"rom",				make_rom_fn,			memfn,				codegen_memfn},
	{"reg",				make_reg_fn,			regfn,				codegen_rsregfn}, 				/* rsreg will work for reg if checks number of inputs */
	{"rsreg",			make_rsreg_fn,			rsregfn,			codegen_rsregfn},
	{"latch",			(make_block_fn)NULL,	latchfn,			NULL,				2,		1},
	{"buf3s",			make_buf3s_fn,			buf3sfn,			codegen_buf3sfn},
	/* should really check adder has at least one output */
	{"adder",			(make_block_fn)NULL,	adderfn,			codegen_adderfn,	3,		-1},
	{"sub",				(make_block_fn)NULL,	subfn,				codegen_subfn,		3,		-1},
	{"lsh",				(make_block_fn)NULL,	lshfn,				codegen_lshfn,		2,		1},
	{"rsh",				(make_block_fn)NULL,	rshfn,				codegen_rshfn,		2,		1},
	{"concat",			make_concat_fn,			concatfn,			codegen_concatfn},
	{"mux",				make_mux_fn,			muxfn,				codegen_muxfn},
	{"demux",			make_demux_fn,			demuxfn,			codegen_demuxfn},
	{"expand",			(make_block_fn)NULL,	expandfn,			codegen_expandfn,	1,		1},
//	{"const",			make_const_fn,			constfn,			codegen_expandfn,	1,		1},
	{"**undefined**",	(make_block_fn)NULL,	undefinedfn,		codegen_expandfn,	-1,		0},
	{"****null block",	(make_block_fn)NULL,	(block_sim_fn)NULL,	codegen_undefinedfn}
};

