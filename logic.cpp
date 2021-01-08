/* copyright (c) David M. Lewis 1987 */

#include <stdio.h>

#include "config.h"
#include "parser.h"
#include "tortle_types.h"
#include "logic.h"
#include "utils.h"

#include "sim.h"

#include "debug.h"


#define abs(x) ((x) < 0 ? -(x) : (x))

/* for future optimization */

#ifdef compiled_runtime
#define check_output_next_2s(blk, iout)

#else

void check_output_next_2s (bptr blk, int iout)
{   check_output_next (blk, iout, 0);
}
#endif

t_fast_logic_desc fast_block_list [] =
{
	{nandfn, fastnand2fn, 2},
	{nandfn, fastnand1fn, 1},
	{nandfn, fastnandfn, -1},
	{andfn, fastand2fn, 2},
	{andfn, fastandfn, -1},
	{xorfn, fastxor2fn, 2},
	{xorfn, fastxorfn, -1},
	{orfn, fastorfn, -1},
	{NULL, NULL, 0}
};


void debug_block_fn (bptr b)
{	int i;

	if (debug_logiceval || b->debugblock)
	{	fprintf (debugfile, "t = %d eval block %s: ", current_tick, b->name);
		for (i = 0; i < b->ninputs; i++)
		{	fprintf (debugfile, "%s = %s  ", b->inputs[i]->name,
				print_logic_value_base (b->inputs[i]->node_value, b->inputs[i]->node_size.bit_high + 1));
		}
		fprintf (debugfile, "\n");
		for (i = 0; i < b->noutputs; i++)
		{	fprintf (debugfile, "out %d is %s node %s was %s, now %s\n", i, b->outputactive [i] ?
				"active" : "inactive", b->outputs [i]->name,
				print_logic_value_base (b->outputs [i]->node_value, b->outputs [i]->node_size.bit_high + 1),
				print_logic_value_base (b->output_values_next [i], b->outputs [i]->node_size.bit_high + 1));
		}
	}
}

#ifdef detail_debugging
#define debug_block(b) debug_block_fn (b)
#else
#define debug_block(b)
#endif


#define check_output_2s(blk, iout, nv, delta)				check_output(blk, iout, nv, delta)


void constfn (bptr b)
{	int iw;

	for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
	{   b->output_values_next [0] [iw] = b->state [iw];
	}
	check_output_next_2s (b, 0);
	debug_block (b);
}


void nandfn (bptr b)
{	logic_value_word result;
	nptr *nn;
	int i;
	int iw;

	for (iw = 0; iw < b->output_widths_words [0]; iw++)
	{   result = ~0;
		for (nn = b->inputs, i = 0; i < b->ninputs; i++)
		{	if (iw < (*nn)->node_size_words)
			{   result &= (*nn)->node_value [iw];
			} else
			{   result = 0;
			}
			nn++;
		}
		result = (~result) & node_bitmask (b->outputs [0], iw);
		b->output_values_next [0] [iw] = result;
	}
	check_output_next_2s (b, 0);
	debug_block (b);
}

#define updatenode(b,v) {	if (v != b->outputval [0])																		\
{	nptr nd; b->outputval [0] = v; nd = b->outputs [0]; nd->valuenext = v;													\
	if (!nd->active) {nd->active = true; activenodes [nactivenodes++] = nd;}												\
	if (nd->traced)																											\
	{	printf ("%s <- %s ", nd->name, print_logic_value_base (nd->node_value_next, nd->node_size.bit_high + 1));			\
		printf (" @ %d\n", current_tick);																					\
	}																														\
}}

void fastnandfn (bptr b)
{	int i;
	logic_value_word result;

	result = ~0;
	for (i = 0; i < b->ninputs; i++)
	{	result &= b->inputs [i]->value;
	}
	result = (~result & b->outputs [0]->bitmask);
	updatenode (b, result);

}

void fastnand2fn (bptr b)
{	logic_value_word result;

	result = ~(b->inputs [0]->value & b->inputs [1]->value) & b->outputs [0]->bitmask;
	updatenode (b, result);
#ifdef foooooo
	if (result != b->outputval [0])
	{	b->outputval [0] = result;
		updatenode(b->outputs [0], result);
		b->outputs [0]->valuenext = result;
		if (!b->outputs [0]->active)
		{	b->outputs [0]->active = true;
			activenodes [nactivenodes++] = b->outputs [0];
		}
	}
#endif
}

void fastnand1fn (bptr b)
{	logic_value_word result;

	result = ~b->inputs [0]->value & b->outputs [0]->bitmask;
	updatenode (b, result);
}

void fastandfn (bptr b)
{	int i;
	logic_value_word result;

	result = ~0;
	for (i = 0; i < b->ninputs; i++)
	{	result &= b->inputs [i]->value;
	}
	result = (result & b->outputs [0]->bitmask);
	updatenode (b, result);

}

void fastand2fn (bptr b)
{	int i;
	logic_value_word result;

	result = (b->inputs [0]->value & b->inputs [1]->value) & b->outputs [0]->bitmask;
	updatenode (b, result);
}


void fastxorfn (bptr b)
{	int i;
	logic_value_word result;

	result = 0;
	for (i = 0; i < b->ninputs; i++)
	{	result ^= b->inputs [i]->value;
	}
	result = result & b->outputs [0]->bitmask;
	updatenode (b, result);

}

void fastxor2fn (bptr b)
{	int i;
	logic_value_word result;

	result = (b->inputs [0]->value ^ b->inputs [1]->value) & b->outputs [0]->bitmask;
	updatenode (b, result);

}

void fastorfn (bptr b)
{	int i;
	logic_value_word result;

	result = 0;
	for (i = 0; i < b->ninputs; i++)
	{	result |= b->inputs [i]->value;
	}
	result = result & b->outputs [0]->bitmask;
	updatenode (b, result);

}

void andfn (bptr b)
{	logic_value_word result;
	nptr *nn;
	int i;
	int iw;

	for (iw = 0; iw < b->output_widths_words [0]; iw++)
	{   result = ~0;
		for (nn = b->inputs, i = 0; i < b->ninputs; i++)
		{	if (iw < (*nn)->node_size_words)
			{	result &= (*nn)->node_value [iw];
			} else
			{	result = 0;
			}
			nn++;
		}
		result = result & node_bitmask (b->outputs [0], iw);
		b->output_values_next [0] [iw] = result;
	}
	check_output_next_2s (b, 0);
	debug_block (b);
}


void orfn (bptr b)
{	logic_value_word result;
	nptr *nn;
	int i;
	int iw;

	for (iw = 0; iw < b->output_widths_words [0]; iw++)
	{   result = 0;
		for (nn = b->inputs, i = 0; i < b->ninputs; i++)
		{	if (iw < (*nn)->node_size_words)
			{	result |= (*nn)->node_value [iw];
			}
			nn++;
		}
		result = result & node_bitmask (b->outputs [0], iw);
		b->output_values_next [0] [iw] = result;
	}
	check_output_next_2s (b, 0);
	debug_block (b);
}


void norfn (bptr b)
{	logic_value_word result;
	nptr *nn;
	int i;
	int iw;

	for (iw = 0; iw < b->output_widths_words [0]; iw++)
	{   result = 0;
		for (nn = b->inputs, i = 0; i < b->ninputs; i++)
		{	if (iw < (*nn)->node_size_words)
			{	result |= (*nn)->node_value [iw];
			}
			nn++;
		}
		result = (~result) & node_bitmask (b->outputs [0], iw);
		b->output_values_next [0] [iw] = result;
	}
	check_output_next_2s (b, 0);
	debug_block (b);
}

void xorfn (bptr b)
{	logic_value_word result;
	nptr *nn;
	int i;
	int iw;

	for (iw = 0; iw < b->output_widths_words [0]; iw++)
	{   result = 0;
		for (nn = b->inputs, i = 0; i < b->ninputs; i++)
		{	if (iw < (*nn)->node_size_words)
			{	result ^= (*nn)->node_value [iw];
			}
			nn++;
		}
		result = result & node_bitmask (b->outputs [0], iw);
		b->output_values_next [0] [iw] = result;
	}
	check_output_next_2s (b, 0);
	debug_block (b);
}


void eqeqfn (bptr b)
{	logic_value_word result;
	nptr *nn;
	int i;
	int iw;

	for (iw = 0; iw < b->output_widths_words [0]; iw++)
	{   if (iw < b->inputs [0]->node_size_words)
			result = b->inputs [0]->node_value [iw];
		else
			result = 0;
		for (nn = &(b->inputs [1]), i = 1; i < b->ninputs; i++)
		{	if (iw < (*nn)->node_size_words)
			{	result ^= (*nn)->node_value [iw];
			}
			result = (~result) & node_bitmask (b->outputs [0], iw);
			nn++;
		}
		b->output_values_next [0] [iw] = result;
	}
	check_output_next_2s (b, 0);
	debug_block (b);
}

void regfn (bptr b)
{	 unsigned result;
	 int iw;

	if ((b->inputs [0]->node_value [0] & 1) && b->state [0] == 0)
	{   for (iw = 0; iw < b->output_widths_words [0]; iw++)
		{   result = 0;
			if (iw < b->inputs [1]->node_size_words)
			{	result = b->inputs [1]->node_value [iw];
			}
			result &= node_bitmask (b->outputs [0], iw);
			b->output_values_next [0] [iw] = result;
		}
	}
	check_output_next_2s (b, 0);
	b->state [0] = b->inputs [0]->node_value [0] & 1;
	debug_block (b);
}


void rsregfn (bptr b)
{	 unsigned result;
	 int iw;

	if ((b->inputs [3]->node_value [0] & 1) ||
		(b->inputs [2]->node_value [0] & 1) ||
		((b->inputs [0]->node_value [0] & 1) && b->state [0] == 0))
	{   for (iw = 0; iw < b->output_widths_words [0]; iw++)
		{   result = 0;
			if (iw < b->inputs [1]->node_size_words)
			{	result = b->inputs [1]->node_value [iw];
			}
			if (b->inputs [3]->node_value [0] & 1)
			{  result = word_all_bit_mask;
			}
			if (b->inputs [2]->node_value [0] & 1)
			{  result = 0;
			}
			result &= node_bitmask (b->outputs [0], iw);
			b->output_values_next [0] [iw] = result;
			if (b->noutputs > 1)
			{  result = ~result & node_bitmask (b->outputs [0], iw);
			   b->output_values_next [1] [iw] = result;
			}
		}
	}
	check_output_next_2s (b, 0);
	if (b->noutputs > 1)
	{  check_output_next_2s (b, 1);
	}
	b->state [0] = b->inputs [0]->node_value [0] & 1;
	debug_block (b);
}


void latchfn (bptr b)
{	unsigned result;
	int iw;

	if (b->inputs [0]->node_value [0] & 1)
	{   for (iw = 0; iw < b->output_widths_words [0]; iw++)
		{   result = 0;
			if (iw < b->inputs [1]->node_size_words)
			{	result = b->inputs [1]->node_value [iw];
			}
			result &= node_bitmask (b->outputs [0], iw);
			b->output_values_next [0] [iw] = result;
		}
	}
	check_output_next_2s (b, 0);
	debug_block (b);
}


void buf3sfn (bptr b)
{	unsigned wasactive;
	unsigned isactive;
	int iw;

	wasactive = b->state [0];
	isactive = b->inputs [1]->node_value [0] & 1;
	if (isactive)
		b->outputactive [0] = true;
	else
		b->outputactive [0] = false;
	b->state [0] = isactive;

//	 sched_block_output_next (bptr blk, int iout, int delta)
	for (iw = 0; iw < b->output_widths_words [0]; iw++)
	{	if (iw < b->inputs [0]->node_size_words)
		{	b->output_values_next [0] [iw] = b->inputs [0]->node_value [iw] & node_bitmask (b->outputs [0], iw);
		} else
		{	b->output_values_next [0] [iw] = 0;
		}
	}
	if (b->inputs [1]->node_value [0] && !wasactive)
	{	sched_block_output_next (b, 0, 1);
	}
	else if (!b->inputs [1]->node_value [0] && wasactive)
	{	sched_block_output_next (b, 0, -1);
	}
	else if (b->inputs [1]->node_value [0])
	{	sched_block_output_next (b, 0, 0);
	}
	debug_block (b);
}


void addsubfn (bptr b, bool subflag)
{	logic_value_word result;
	logic_value_word cout;
	logic_value_word cin;
	logic_value_word op0;
	logic_value_word op1;
	logic_value_word result_sum0;
	logic_value_word result_carry0;
	logic_value_word result_highbits;
	int iw;

	cin = b->inputs [2]->node_value [0] & 1;
	for (iw = 0; iw < b->output_widths_words [0]; iw++)
	{	if (iw < b->inputs [0]->node_size_words)
		{	op0 = b->inputs [0]->node_value [iw];
		} else
		{	op0 = 0;
		}
		if (iw < b->inputs [1]->node_size_words)
		{	op1 = b->inputs [1]->node_value [iw];
		} else
		{	op1 = 0;
		}
		if (subflag)
		{	op1 = ~op1 & node_bitmask (b->inputs [1], iw);
		}
		result_sum0 = (op0 ^ op1 ^ cin) & 1;
		result_carry0 = ((op0 & 1) + (op1 & 1) + cin) >> 1;
		result_highbits = (op0 >> 1) + (op1 >> 1) + result_carry0;
		result = (result_highbits << 1) | result_sum0;
		b->output_values_next [0] [iw] = result & node_bitmask (b->outputs [0], iw);
		cin = (result_highbits >> (bits_per_word - 1)) & 1;
	}
	check_output_next_2s (b, 0);

	if (b->noutputs > 1)
	{	if ((b->outputs [0]->node_size.bit_high + 1) % bits_per_word == 0)
		{	cout = cin;
		} else
		{	cout = (result >> ((b->outputs [0]->node_size.bit_high + 1) % bits_per_word)) & 1;
		}
		b->output_values_next [1] [0] = cout;
		check_output_next_2s (b, 1);
	}
}

void adderfn (bptr b)
{	addsubfn (b, 0);
}


void subfn (bptr b)
{	addsubfn (b, 1);
}


void compare_block_inputs (bptr b, bool *result_gt, bool *result_eq, bool *result_lt)
{	int iw;
	unsigned in0, in1;
	bool gt, eq, lt;

	gt = false;
	eq = true;
	lt = false;

	for (iw = 0; iw < b->inputs [0]->node_size_words || iw < b->inputs [1]->node_size_words; iw++)
	{	if (iw >= b->inputs [0]->node_size_words)
		{	in0 = 0;
		} else {
			in0 = b->inputs [0]->node_value [iw];
		}
		if (iw >= b->inputs [1]->node_size_words)
		{	in1 = 0;
		} else {
			in1 = b->inputs [1]->node_value [iw];
		}
		if (in0 > in1)
		{	gt = true;
			eq = false;
			lt = false;
		}
		if (in0 < in1)
		{	gt = false;
			eq = false;
			lt = true;
		}
	}
	*result_gt = gt;
	*result_eq = eq;
	*result_lt = lt;
}



void greaterfn (bptr b)
{	logic_value_word result;
	bool gt, eq, lt;

	compare_block_inputs (b, &gt, &eq, &lt);
	if (gt)
		result = 1;
	else
		result = 0;
	b->output_values_next [0] [0] = result;
	check_output_next_2s (b, 0);
	debug_block (b);
}

void lessfn (bptr b)
{	logic_value_word result;
	bool gt, eq, lt;

	compare_block_inputs (b, &gt, &eq, &lt);
	if (lt)
		result = 1;
	else
		result = 0;
	b->output_values_next [0] [0] = result;
	check_output_next_2s (b, 0);
	debug_block (b);
}

void geqfn (bptr b)
{	logic_value_word result;
	bool gt, eq, lt;

	compare_block_inputs (b, &gt, &eq, &lt);
	if (gt || eq)
		result = 1;
	else
		result = 0;
	b->output_values_next [0] [0] = result;
	check_output_next_2s (b, 0);
	debug_block (b);
}

void leqfn (bptr b)
{	logic_value_word result;
	bool gt, eq, lt;

	compare_block_inputs (b, &gt, &eq, &lt);
	if (lt || eq)
		result = 1;
	else
		result = 0;
	b->output_values_next [0] [0] = result;
	check_output_next_2s (b, 0);
	debug_block (b);
}

void eqfn (bptr b)
{	logic_value_word result;
	bool gt, eq, lt;

	compare_block_inputs (b, &gt, &eq, &lt);
	if (eq)
		result = 1;
	else
		result = 0;
	b->output_values_next [0] [0] = result;
	check_output_next_2s (b, 0);
	debug_block (b);
}

void neqfn (bptr b)
{	logic_value_word result;
	bool gt, eq, lt;

	compare_block_inputs (b, &gt, &eq, &lt);
	if (!eq)
		result = 1;
	else
		result = 0;
	b->output_values_next [0] [0] = result;
	check_output_next_2s (b, 0);
	debug_block (b);
}

// insert_shifted_bits (logic_value dst, int n_logic_value_words, logic_value v, int lsh, t_bitrange range)
void lshfn (bptr b)
{	unsigned shift;
	t_bitrange r;
	int iw;

	for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
	{	b->output_values_next [0] [iw] = 0;
	}

	shift = b->inputs [1]->node_value [0];
	r.bit_low = shift;
	r.bit_high = my_min (b->outputs [0]->node_size.bit_high, b->inputs [0]->node_size.bit_high + (int) shift);
	insert_shifted_bits (b->output_values_next [0], b->outputs [0]->node_size_words, b->inputs [0]->node_value, shift, r);

	check_output_next_2s (b, 0);
	debug_block (b);
}


void rshfn (bptr b)
{	unsigned shift;
	t_bitrange r;
	int iw;

	for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
	{	b->output_values_next [0] [iw] = 0;
	}

	shift = b->inputs [1]->node_value [0];
	r.bit_low = 0;
	r.bit_high = my_min (b->outputs [0]->node_size.bit_high, b->inputs [0]->node_size.bit_high - (int) shift);
	insert_shifted_bits (b->output_values_next [0], b->outputs [0]->node_size_words, b->inputs [0]->node_value, - (int) shift, r);

	check_output_next_2s (b, 0);
	debug_block (b);
}

void concatfn (bptr b)
{	t_bitrange dst_range;
	int i;

	for (i = 0; i < b->ninputs; i++)
	{	dst_range.bit_low = b->state [i];
		dst_range.bit_high = dst_range.bit_low + b->inputs [i]->node_size.bit_high;
		if (dst_range.bit_high > b->outputs [0]->node_size.bit_high)
		{	dst_range.bit_high = b->outputs [0]->node_size.bit_high;
		}
		insert_shifted_bits (b->output_values_next [0], b->outputs [0]->node_size_words, b->inputs [i]->node_value,
					dst_range.bit_low, dst_range);
	}
	check_output_next_2s (b, 0);
}

void muxfn (bptr b)
{	int iw;
	int sel;
	logic_value_word result;
	logic_value sel_val;
	int sel_words;

	sel = (b->inputs [0]->node_value [0]) & b->state [0];
	sel_val = b->inputs [sel + 1]->node_value;
	sel_words = b->inputs [sel + 1]->node_size_words;
	for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
	{	if (iw < sel_words)
		{	result = sel_val [iw];
		} else
		{	result = 0;
		}
		b->output_values_next [0] [iw] = result & node_bitmask (b->outputs [0], iw);
	}
	check_output_next_2s (b, 0);
}



/* stupid implementation of demux, since wiggles all outputs */

void demuxfn (bptr b)
{	logicval i;
	int iw;

	for (i = 0; i <= b->state [0]; i++)
	{	for (iw = 0; iw < b->outputs [i]->node_size_words; iw++)
		{	b->output_values_next [i] [iw] = 0;
		}
		if (i == b->inputs [0]->node_value [0])
		{	for (iw = 0; iw < b->outputs [i]->node_size_words && iw < b->inputs [1]->node_size_words; iw++)
			{	b->output_values_next [i] [iw] = b->inputs [1]->node_value [iw] & node_bitmask (b->outputs [i], iw);
			}
		}
		check_output_next_2s (b, i);
	}
	debug_block (b);
}

void memfn (bptr b)
{	unsigned result;
	unsigned address;
	unsigned datain;
	int state_wordnum;
	int state_subword;
	int state_words_per_word;
	int words_per_state_word;
	int nports;
	int iport;
	unsigned depth;
	int width;
	unsigned mask;
	int iw;

	depth = b->state [state_word_ram_depth];
	width = b->state [state_word_ram_width];
	nports = b->state [state_word_ram_nports];
	state_words_per_word = b->state [state_word_ram_state_words_per_word];
	words_per_state_word = b->state [state_word_ram_words_per_state_word];

	/* check all write enables and write data if set */

	for (iport = 0; iport < nports; iport++)
	{	if (b->inputs [iport * 3 + 1]->node_value [0] & 1)
		{	address = b->inputs [iport * 3]->node_value [0];
			if (address < depth)
			{	state_wordnum = address * state_words_per_word / words_per_state_word + state_word_ram_state_data;
				if (words_per_state_word > 1)
				{	state_subword = address % words_per_state_word;
					mask = bitwidmask (width) << width * state_subword;
					b->state [state_wordnum] = (b->state [state_wordnum] & ~mask) | ((b->inputs [iport * 3 + 2]->node_value [0] << width * state_subword) & mask);
				} else
				{	for (iw = 0; iw < state_words_per_word; iw++)
					{	if (iw < b->inputs [iport * 3 + 2]->node_size_words)
						{	datain = b->inputs [iport * 3 + 2]->node_value [iw] & bitwidmask (width - iw * bits_per_word);
						} else
						{	datain = 0;
						}
						b->state [state_wordnum + iw] = datain;
					}
				}
			}
		}
	}

	/* now do reads */

	for (iport = 0; iport < nports; iport++)
	{	address = b->inputs [iport * 3]->node_value [0];
		for (iw = 0; iw < b->outputs [iport]->node_size_words; iw++)
		{	b->output_values_next [iport] [iw] = 0;
		}
		if (address < depth)
		{	state_wordnum = address * state_words_per_word / words_per_state_word + state_word_ram_state_data;
			if (words_per_state_word > 1)
			{	state_subword = address % words_per_state_word;
				mask = bitwidmask (width);
				result = (b->state [state_wordnum] >> (state_subword * width)) & mask;
				b->output_values_next [iport] [0] = result & node_bitmask (b->outputs [iport], 0);
			} else {
				for (iw = 0; iw < b->outputs [iport]->node_size_words && iw * bits_per_word < width; iw++)
				{	b->output_values_next [iport] [iw] = b->state [state_wordnum + iw] & node_bitmask (b->outputs [iport], iw);
				}
			}
			check_output_next_2s (b, iport);
		}
	}
}



void expandfn (bptr b)
{	unsigned result;
	int iw;

	if (b->inputs [0]->node_value [0])
	{	for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
		{	b->output_values_next [0] [iw] = (~0) & node_bitmask (b->outputs [0], iw);
		}
	} else
	{	for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
		{	b->output_values_next [0] [iw] = 0;
		}
	}
	check_output_next_2s (b, 0);
	debug_block (b);
}


void priorityfn (bptr b)
{	unsigned v;
	int resultpos;
	bool resultfound;

	resultfound = false;
	resultpos = b->inputs [0]->node_size.bit_high;
	while (resultpos >= 0 && !resultfound)
	{	if (b->inputs [0]->node_value [bit_div_word (resultpos)] & (1 << bit_mod_word (resultpos)))
		{	resultfound = true;
		} else
		{	resultpos--;
		}
	}
	b->output_values_next [0] [0] = ((unsigned) resultpos) & node_bitmask (b->outputs [0], 0);
	b->output_values_next [1] [0] = (unsigned) resultfound;

	check_output_next_2s (b, 0);
	check_output_next_2s (b, 1);
}



void undefinedfn (bptr b)
{

}



