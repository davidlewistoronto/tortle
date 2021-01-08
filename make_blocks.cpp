/* copyright (c) David M. Lewis 1987 */

#include <stdio.h>

#include "config.h"
#include "parser.h"
#include "tortle_types.h"
#include "logic.h"
#include "codegen_logic.h"
#include "make_blocks.h"
#include "sim.h"
#include "utils.h"
#include "blocktypes.h"

#include "debug.h"


bptr make_block (t_src_context *sc, const char *name, block_sim_fn fn, block_codegen_fn cfn, int nstate, symptr *nodes, int nnodes, int noutputs,
	bool *init_outputactive, int nnums, const char *blocktypename)
{	nptr tempnodes [maxblocknodes];
	bptr b;
	int i;
	char msg [maxnamelen + 100];

	if (debug_makeblocks)
	{	printf ("make_block \"%s\", %d nodes: ", name, nnodes);
		for (i = 0; i < nnodes; i++)
		printf ("\"%s\"%c", nodes [i]->nodedef->name, i == nnodes - 1 ? '\n' : ',');
	}
	if (nnums != 0)
	{	sprintf (msg, "block %s: cannot have number parameters", name);
		print_error_context (sc, msg);
	}
	if (nnodes < noutputs)
	{	sprintf (msg, "not enough inputs to block %s", name);
		print_error_context (sc, msg);
	}
	for (i = 0; i < nnodes; i++)
		tempnodes [i] = nodes [i]->nodedef;
	b = new_block (name, fn, cfn, nstate, nnodes - noutputs, tempnodes, noutputs, tempnodes + nnodes - noutputs,
		init_outputactive, blocktypename);
	b->src_context_stack.sc = sc;
	b->src_context_stack.next = src_context_stack;
	b->src_context_stack.scs_id = -1;

	return (b);
}

/* make a block with a given number of total nodes, and a constraint either on the number of inputs or outputs
 * so we can disambiguate.
 * only works for 2state, always driven outputs
 */

bptr make_general (t_src_context *sc, const char *name, block_sim_fn fn, block_codegen_fn cfn, int nstate, symptr *nodes, int nnodes, int needinnodes, int needoutnodes,
	int nnums, int neednums, const char *blocktypename)
{	int ninputs, noutputs;
	bool init_outputactive [maxblocknodes];
	int i;
	char msg [maxnamelen + 100];

	if (needinnodes == -1 && needoutnodes == -1)
	{	sprintf (msg, "internal tortle error: impossible to resolve nnodes in %s", name);
		print_error_context (sc, msg);
		ninputs = nnodes;
		noutputs = 0;
	}
	else
	{	if (needinnodes == -1)
			ninputs = nnodes - needoutnodes;
		else
			ninputs = needinnodes;
		if (needoutnodes == -1)
			noutputs = nnodes - needinnodes;
		else
			noutputs = needoutnodes;
	}
	if (ninputs + noutputs != nnodes)
	{	sprintf (msg, "wrong number of nodes to block %s", name);
		print_error_context (sc, msg);
		return NULL;
	}
	for (i = 0; i < noutputs; i++) {
		init_outputactive [i] = true;
	}
	if (nnums != neednums)
	{	sprintf (msg, "wrong number of numbers to block %s", name);
		print_error_context (sc, msg);
	}
	return (make_block (sc, name, fn, cfn, nstate, nodes, nnodes, noutputs, init_outputactive, 0, blocktypename));
}

bptr make_fn (t_src_context *sc, makeblockrec *maketype, const char *name, symptr *nodes, int nnodes, unsigned *nums,
	int nnums)
{
	if (maketype->f)
		return ((maketype->f) (sc, name, nodes, nnodes, nums, nnums));
	else
		return (make_general (sc, name, maketype->logicfn, maketype->codegenfn, 0, nodes, nnodes, maketype->need_in_nodes,
			maketype->need_out_nodes, nnums, 0, maketype->name));
}

bptr make_buf3s_fn (t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums)
{	nptr tempnodes [maxblocknodes];
	bptr b;
	int i;
	bool init_outputactive [1];
	t_bitrange driven_3s [1];
	char msg [maxnamelen + 100];


	if (debug_makeblocks)
	{	printf ("make_buf3s \"%s\", %d nodes: ", name, nnodes);
		for (i = 0; i < nnodes; i++)
		printf ("\"%s\"%c", nodes [i]->nodedef->name, i == nnodes - 1 ? '\n' : ',');
	}
	if (nnodes != 3)
	{	sprintf (msg, "block %s: must have 3 connections", name);
		print_error_context (sc, msg);
	}
	if (nnums != 0)
	{	sprintf (msg, "block %s: cannot have number parameters", name);
		print_error_context (sc, msg);
	}
	tempnodes [0] = nodes [0]->nodedef;
	tempnodes [1] = nodes [1]->nodedef;
	tempnodes [2] = nodes [2]->nodedef;		/* output node */
	init_outputactive [0] = false;
	b = new_block (name, buf3sfn, NULL, 1, nnodes - 1, tempnodes, 1, tempnodes + nnodes - 1, init_outputactive, "buf3s");
	b->src_context_stack.sc = sc;
	b->src_context_stack.next = src_context_stack;
	b->state [0] = 0;		/* output_active for previous tick */
	return (b);
}

/* note make_const is the only one that actually gets passed nodes vs.
 * symptrs.
 * WTF does that mean? I think this is wrong.
 */

bptr make_const_fn (t_src_context *sc, const char *name, symptr *nodes, int nnodes, t_const_val *cv)
{	bptr b;
	int iw;

	b = make_general (sc, name, constfn, codegen_constfn, nodes [1]->nodedef->node_size_words, nodes, nnodes, 1, 1, 1, 1, "const");
	if (b != NULL)
	{	for (iw = 0; iw < nodes [1]->nodedef->node_size_words; iw++)
		{   b->state [iw] = get_const_word (cv, iw);
		}
	}
	return (b);
}

bptr make_concat_fn (t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums)
{	int i;
	int totalwid;
	bptr b;


	b = make_general (sc, name, concatfn, codegen_concatfn, nnodes - 1, nodes, nnodes, nnodes - 1, 1, nnums, 0, "concat");
	if (b != NULL)
	{	for (totalwid = 0, i = nnodes - 2; i >= 0; i--)
		{	b->state [i] = totalwid;
			totalwid += bitrange_width(b->inputs [i]->node_size);
		}
	}
	return (b);
}

bptr make_mux_fn (t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums)
{	int nin;
	bptr b;
	char msg [maxnamelen + 100];

	nin = nnodes - 2;
	if (((nin - 1) | nin) != nin + nin - 1)
	{	sprintf (msg, "block %s: must have power of two inputs", name);
		print_error_context (sc, msg);
	}
	b = make_general (sc, name, muxfn, codegen_muxfn, 1, nodes, nnodes, nnodes - 1, 1, nnums, 0, "mux");
	if (b != NULL)
	{	b->state [0] = nin - 1;
	}
	return (b);
}

bptr make_demux_fn (t_src_context *sc, const char *name, symptr *nodes,  int nnodes, unsigned *nums, int nnums)
{	int nout;
	bptr b;
	char msg [maxnamelen + 100];


	nout = nnodes - 2;
	if (((nout - 1) | nout) != nout + nout - 1)
	{	sprintf (msg, "block %s: must have power of two outputs", name);
		print_error_context (sc, msg);
	}
	b = make_general (sc, name, demuxfn, codegen_demuxfn, 1, nodes, nnodes, 2, nout, nnums, 0, "demux");
	if (b != NULL)
	{	b->state [0] = nout - 1;	/* use as mask */
	}
	return (b);
}

bptr make_ram_fn (t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums)
{	int depth;
	int width;
	int words_per_state_word;
	int state_words_per_word;
	int n_ram_state_words;
	bptr b;
	symptr tempnodes [maxblocknodes];
	int nports;
	int iport;
	int iw;
	char msg [maxnamelen + 100];

	if (nnums != 2)
	{	sprintf (msg, "block %s: bad depth / width", name);
		print_error_context (sc, msg);
		return (bptr) NULL;
	}
	depth = nums [0];
	width = nums [1];

	words_per_state_word = bits_per_word / width;
	if (words_per_state_word == 0)
	{	words_per_state_word = 1;
	}
	state_words_per_word = (width + bits_per_word - 1) / bits_per_word;
	n_ram_state_words = (depth * state_words_per_word + words_per_state_word - 1) / words_per_state_word;
	if (nnodes % 4 != 0)
	{	sprintf (msg, "block %s: memory must have multiple of 4 nodes for ports addr,we,din,dout", name);
		print_error_context (sc, msg);
		return NULL;	/* should have a cleaner handle than this */
	}
	nports = nnodes / 4;

	/* reorder ports inputs first then outputs as expected by make_general */

	for (iport = 0; iport < nports; iport++)
	{	tempnodes [iport * 3] = nodes [iport * 4];
		tempnodes [iport * 3 + 1] = nodes [iport * 4 + 1];
		tempnodes [iport * 3 + 2] = nodes [iport * 4 + 2];
		/* data out ports */
		tempnodes [nports * 3 + iport] = nodes [iport * 4 + 3];
	}
	b = make_general (sc, name, memfn, codegen_memfn, n_ram_state_words + state_word_ram_state_data,
			tempnodes, nnodes, 3 * nports, nports, 0, 0, "ram");
	b->state [state_word_ram_depth] = depth;
	b->state [state_word_ram_width] = width;
	b->state [state_word_ram_words_per_state_word] = words_per_state_word;
	b->state [state_word_ram_state_words_per_word] = state_words_per_word;
	b->state [state_word_ram_nports] = nports;
	for (iw = 0; iw < n_ram_state_words; iw++)
	{	b->state [state_word_ram_state_data + iw] = 0;
	}
	return b;
}

bptr make_rom_fn (t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums)
{	int nstate;
	int wordsperstateword;
	bptr b;
	symptr tmp_nodes [4];
	char msg [maxnamelen + 100];

	if (nnodes != 2)
	{	sprintf (msg, "block %s: wrong number of nodes for rom", name);
		print_error_context (sc, msg);
		return NULL;
	}
	tmp_nodes [0] = nodes [0];
	tmp_nodes [1] = nodes [1];
	tmp_nodes [2] = block_false;
	tmp_nodes [3] = block_false;
	return (make_ram_fn (sc, name, tmp_nodes, 4, nums, nnums));
}


bptr make_reg_fn (t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums)
{   bptr b;

	b = make_general (sc, name, regfn, codegen_rsregfn, 1, nodes, nnodes, 2, -1, nnums, 0, "reg");
	b->is_clocked = true;
	return (b);
}

bptr make_rsreg_fn (t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums)
{	bptr b;
	char msg [maxnamelen + 100];

	b = make_general (sc, name, rsregfn, codegen_rsregfn, 1, nodes, nnodes, 4, -1, nnums, 0, "rsreg");
	b->is_clocked = true;
	if (b->noutputs == 0)
	{	sprintf (msg, "not enough connections to rsreg %s", b->name);
		print_error_context (sc, msg);
	}
	return (b);
}




