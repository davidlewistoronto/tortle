#ifndef __MAKE_BLOCKS_H
#define __MAKE_BLOCKS_H

#include "tortle_types.h"
#include "nethelp.h"

bptr make_block (t_src_context *sc, char *name, block_sim_fn fn, block_codegen_fn cfn, int nstate, symptr *nodes, int nnodes, int noutputs,
	unsigned *driven, unsigned *driven_3s, int nnums, const char *blocktypename);

bptr make_general (t_src_context *sc, const char *name, block_sim_fn fn, block_codegen_fn cfn, int nstate, symptr *nodes, int nnodes, int needinnodes, int needoutnodes,
	int nnums, int neednums, const char *blocktypename);

bptr make_fn (t_src_context *sc, makeblockrec *maketype, const char *name, symptr *nodes, int nnodes, unsigned *nums,
	int nnums);

bptr make_ram_fn(t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums);
bptr make_rom_fn(t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums);
bptr make_reg_fn(t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums);
bptr make_rsreg_fn(t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums);
bptr make_buf3s_fn(t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums);
//bptr make_const_fn(char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums, int del);
bptr make_concat_fn(t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums);
bptr make_mux_fn(t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums);
bptr make_demux_fn(t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums);
bptr make_buf3s_fn (t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums);
bptr make_rsreg_fn (t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums);

/* const is the only block that can have numbers > 1 word wide so it is special */

bptr make_const_fn (t_src_context *sc, const char *name, symptr *nodes, int nnodes, t_const_val *v);
#endif
