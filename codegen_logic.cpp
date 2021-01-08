#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>

#pragma hdrstop

#include "tortle_types.h"
#include "codegen_logic.h"
#include "utils.h"
#include "debug.h"


void codegen_copy_shifted_bits (FILE *cf, int dst_loc, int src_loc, int src_n_words, int lsh, t_bitrange dst_bitrange)
{	int wb_low, wb_high;
	t_bitrange write_br;
	logic_value_word write_mask;
	int lsh_mod;
	int rsh_mod;
	int insert_w;
	int iw;

	lsh_mod = bit_mod_word (lsh);
	rsh_mod = bits_per_word - lsh_mod;

	for (iw = 0; iw < src_n_words; iw++)
	{	wb_low = bits_per_word * iw + lsh;

		/* which word in dst to write the low bits */

		insert_w = bit_div_word (wb_low);

		/* this word will be written into lsh_mod of word insert_w to lsh_mod - 1 of insert_w + 1
		 * masked by the range
		 *
		 * so write lsh_mod upwards into insert_w and 0 .. lsh_mod - 1 into insert_w + 1
		 */

		write_br.bit_low = my_max (dst_bitrange.bit_low - insert_w * bits_per_word, lsh_mod);
		write_br.bit_high = dst_bitrange.bit_high - insert_w * bits_per_word;
		write_mask = bitrange_mask (write_br);
		if (write_mask != 0) {
			my_assert (insert_w >= 0, "insert into negative word position");
//			dst [insert_w] = dst [insert_w] & ~ write_mask | (w << lsh_mod) & write_mask;
			fprintf (cf, "\tsp [%d] =", dst_loc + insert_w);
			if (~ write_mask != 0)
			{	fprintf (cf, " (sp [%d] & 0x%x) |", dst_loc + insert_w, ~write_mask);
			}
			fprintf (cf, " ((sp [%d] << %d) & 0x%x);\n", src_loc + iw, lsh_mod, write_mask);
		}

		/* if bits are unaligned across words then need to stuff the high bits into next word */

		if (lsh_mod != 0) {
			insert_w++;
			write_br.bit_low = dst_bitrange.bit_low - insert_w * bits_per_word;
			write_br.bit_high = my_min (dst_bitrange.bit_high - insert_w * bits_per_word, lsh_mod - 1);
			write_mask = bitrange_mask (write_br);
			if (write_mask != 0) {
				my_assert (insert_w >= 0, "insert into negative word position");
//				dst [insert_w] = dst [insert_w] & ~ write_mask | (w >> rsh_mod) & write_mask;
				fprintf (cf, "\tsp [%d] =", dst_loc + insert_w);
				if (~ write_mask != 0)
				{	fprintf (cf, " (sp [%d] & 0x%x) |", dst_loc + insert_w, ~write_mask);
				}
				fprintf (cf, " ((sp [%d] >> %d) & 0x%x);\n", src_loc + iw, rsh_mod, write_mask);
			}
		}

	}

}

void codegen_drive_outcons (FILE *cf, bptr b, int iout)
{	int iconn;

	for (iconn = 0; iconn < b->noutcons [iout]; iconn++)
	{	if (b->outcons [iout] [iconn].node != b->outputs [iout])
		{   if (debug_compiled_comments)
			{	fprintf (cf, "\t/* outcon %s [%d] [%d] %s %d %d %d */\n", b->name, iout, iconn, b->outcons [iout] [iconn].node->name,
					b->outcons [iout] [iconn].conn_sh, b->outcons [iout] [iconn].conn_bitrange.bit_low,
					b->outcons [iout] [iconn].conn_bitrange.bit_high);
			}
			codegen_copy_shifted_bits (cf, b->outcons [iout] [iconn].node->node_address,
				b->outputs [iout]->node_address, b->outputs [iout]->node_size_words,
				b->outcons [iout] [iconn].conn_sh, b->outcons [iout] [iconn].conn_bitrange);
		}
	}
}

void codegen_gate (FILE *cf, const char *op_string, bool comp_result, bptr b)
{	int iin;
	int iw;
	logic_value_word outmask;
	logic_value_word inmask;

	for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
	{	fprintf (cf, "\tsp [%d] = (", b->outputs [0]->node_address + iw);
		if (comp_result)
			fprintf (cf, "~(");
		inmask = 0;
		for (iin = 0; iin < b->ninputs; iin++)
		{	inmask |= node_bitmask (b->inputs [iin], iw);
			if (iin != 0)
				fprintf (cf, " %s", op_string);
			if (iw >= b->inputs [iin]->node_size_words)
				fprintf (cf, " 0");
			else
				fprintf (cf, " sp [%d]", b->inputs [iin]->node_address + iw);
		}
		if (comp_result)
			fprintf (cf, ")");
		fprintf (cf, ")");
		outmask = node_bitmask (b->outputs [0], iw);
		if ((comp_result || ((inmask & ~outmask) != 0)) && (~outmask != 0))
			fprintf (cf, "&0x%x", outmask);
		fprintf (cf, ";\n");
	}
	codegen_drive_outcons (cf, b, 0);
}

void codegen_constfn (FILE *cf, bptr b)
{	int iw;

	for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
	{	fprintf (cf, "\tsp [%d] = 0x%x;\n", b->outputs [0]->node_address + iw, b->state [iw] & node_bitmask (b->outputs [0], iw));
	}
	codegen_drive_outcons (cf, b, 0);
}

void codegen_nandfn (FILE *cf, bptr b)
{	codegen_gate (cf, "&", true, b);
}

void codegen_andfn (FILE *cf, bptr b)
{	codegen_gate (cf, "&", false, b);
}

void codegen_orfn (FILE *cf, bptr b)
{	codegen_gate (cf, "|", false, b);
}

void codegen_norfn (FILE *cf, bptr b)
{	codegen_gate (cf, "|", true, b);
}

void codegen_xorfn (FILE *cf, bptr b)
{	codegen_gate (cf, "^", false, b);
}

void codegen_eqeqfn (FILE *cf, bptr b)
{	codegen_gate (cf, "^", (b->ninputs & 1) == 0, b);
}

void codegen_concatfn (FILE *cf, bptr b)
{	int iin;
	t_bitrange range;

	for (iin = 0; iin < b->ninputs; iin++)
	{	range.bit_low = b->state [iin];
		range.bit_high = my_min (b->outputs [0]->node_size.bit_high, range.bit_low + b->inputs [iin]->node_size.bit_high);
		if (range.bit_high >= range.bit_low)
		{	codegen_copy_shifted_bits (cf, b->outputs [0]->node_address, b->inputs [iin]->node_address,
					b->inputs [0]->node_size_words, range.bit_low, range);
		}
	}
	codegen_drive_outcons (cf, b, 0);
}


/* refactor to include codegen of outcons */

void codegen_interp_block (FILE *cf, bptr b, const char *fun_name)
{	int iout;

	fprintf (cf, "\t%s (compiled_code_data.ordered_blocks [%d]);\n", fun_name, b->block_id);
	for (iout = 0; iout < b->noutputs; iout++)
	{	codegen_drive_outcons (cf, b, iout);
	}
}

void codegen_regfn (FILE *cf, bptr b) {}
void codegen_rsregfn (FILE *cf, bptr b)
{	int iw;


	if (b->ninputs > 2)			/* is rsreg */
	{	/* set */
		fprintf (cf, "\tif (sp[%d] & 1)\n", b->inputs [3]->node_address);
		fprintf (cf, "\t{\n");
		for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
		{	fprintf (cf, "\t\tsp [%d] = 0x%x;\n", b->input_node_saved_loc + iw, node_bitmask (b->inputs [1], iw));
		}
		if (b->noutputs > 1)
		{	for (iw = 0; iw < b->outputs [1]->node_size_words; iw++)
			{	fprintf (cf, "\t\tsp [%d] = 0;\n", b->input_node_saved_loc + iw);
			}
		}
		fprintf (cf, "\t}\n");
		/* reset */
		fprintf (cf, "\tif (sp [%d] & 1)\n", b->inputs [2]->node_address);
		fprintf (cf, "\t{\n");
		for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
		{	fprintf (cf, "\t\tsp [%d] = 0;\n", b->input_node_saved_loc + iw);
		}
		if (b->noutputs > 1)
		{	for (iw = 0; iw < b->outputs [1]->node_size_words; iw++)
			{	fprintf (cf, "\t\tsp [%d] = 0x%x;\n", b->input_node_saved_loc + iw, node_bitmask (b->inputs [1], iw));
			}
		}
		fprintf (cf, "\t}\n");
	}
	/* copy d to q */
	for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
	{	fprintf (cf, "\tsp [%d] = sp [%d]", b->outputs [0]->node_address + iw, b->input_node_saved_loc + iw);
		if (b->outputs [0]->node_size.bit_high < b->inputs [0]->node_size.bit_high)
		{	fprintf (cf, " & 0x%x;\n", node_bitmask (b->outputs [0], iw));
		}
		else
		{	fprintf (cf, ";\n");
		}
	}
	/* do we have !q */

	if (b->noutputs > 1)
	{	for (iw = 0; iw < b->outputs [1]->node_size_words; iw++)
		{	fprintf (cf, "\tsp [%d] = (~sp [%d]) & 0x%x;\n", b->outputs [1]->node_address + iw, b->input_node_saved_loc + iw,
				node_bitmask (b->outputs [1], iw));
		}
	}


	codegen_drive_outcons (cf, b, 0);
	if (b->noutputs > 1)
	{	codegen_drive_outcons (cf, b, 0);
	}

}

void codegen_buf3sfn (FILE *cf, bptr b) {}

void codegen_adderfn (FILE *cf, bptr b)
{	codegen_interp_block (cf, b, "adderfn");
}

void codegen_subfn (FILE *cf, bptr b)
{	codegen_interp_block (cf, b, "subfn");
}

void codegen_compare_neq (FILE *cf, bptr b, bool ne_op)
{	t_node *n0;
	t_node *n1;
	int iw;

	n0 = b->inputs [1];
	n1 = b->inputs [0];

	fprintf (cf, "\tsp [%d] = ", b->outputs [0]->node_address);
	for (iw = 0; iw < n0->node_size_words || iw < n1->node_size_words; iw++)
	{	if (iw > 0)
			fprintf (cf, "%s", ne_op ? " || " : " && ");
		if (iw < n0->node_size_words)
			fprintf (cf, "(sp [%d]", n0->node_address + iw);
		else
			fprintf (cf, "(0");
		fprintf (cf, ne_op ? " != " : " == ");
		if (iw < n1->node_size_words)
			fprintf (cf, "sp [%d])", n1->node_address + iw);
		else
			fprintf (cf, "0)");
	}
	fprintf (cf, ";\n");
}

void codegen_compare_gte (FILE *cf, bptr b, bool swap_ops, bool ge_op)
{	t_node *n0;
	t_node *n1;
	int iw;

	if (swap_ops)
	{	n0 = b->inputs [1];
		n1 = b->inputs [0];
	}
	else
	{	n0 = b->inputs [0];
		n1 = b->inputs [1];
	}
	fprintf (cf, "\tsp [%d] = ", b->outputs [0]->node_address);
	for (iw = 0; iw < n0->node_size_words || iw < n1->node_size_words; iw++)
	{	fprintf (cf, "(");
	}
	fprintf (cf, "(sp [%d] %s sp [%d])", n0->node_address, ge_op ? ">=" : ">", n1->node_address);
	for (iw = 1; iw < n0->node_size_words || iw < n1->node_size_words; iw++)
	{	fprintf (cf, " && (");
		if (iw < n0->node_size_words)
		{	fprintf (cf, "sp [%d]", n0->node_address + iw);
		}
		else
		{	fprintf (cf, "0");
		}
		fprintf (cf, " == " );
		if (iw < n1->node_size_words)
		{	fprintf (cf, "sp [%d]", n1->node_address + iw);
		}
		else
		{	fprintf (cf, "0");
		}
		fprintf (cf, ")) || (");
		if (iw < n0->node_size_words)
		{	fprintf (cf, "sp [%d]", n0->node_address + iw);
		}
		else
		{	fprintf (cf, "0");
		}
		fprintf (cf, " > ");
		if (iw < n1->node_size_words)
		{	fprintf (cf, "sp [%d]", n1->node_address + iw);
		}
		else
		{	fprintf (cf, "0");
		}
		fprintf (cf, ")");

	}

	fprintf (cf, ");\n");
}

void codegen_greaterfn (FILE *cf, bptr b)
{
	codegen_compare_gte ( cf,  b, false, false);
}

void codegen_lessfn (FILE *cf, bptr b)
{
	codegen_compare_gte ( cf,  b, true, false);
}

void codegen_geqfn (FILE *cf, bptr b)
{
	codegen_compare_gte ( cf,  b, false, true);
}

void codegen_leqfn (FILE *cf, bptr b)
{
	codegen_compare_gte ( cf,  b, true, true);
}

void codegen_eqfn (FILE *cf, bptr b)
{
	codegen_compare_neq (cf, b, false);
}

void codegen_neqfn (FILE *cf, bptr b)
{
	codegen_compare_neq (cf, b, true);
}

void codegen_lshfn (FILE *cf, bptr b)
{	codegen_interp_block (cf, b, "lshfn");
}

void codegen_rshfn (FILE *cf, bptr b)
{	codegen_interp_block (cf, b, "rshfn");
}

void codegen_muxfn (FILE *cf, bptr b)
{
	codegen_interp_block (cf, b, "muxfn");
}

void codegen_demuxfn (FILE *cf, bptr b)
{
	codegen_interp_block (cf, b, "demuxfn");
}

void codegen_memfn (FILE *cf, bptr b)
{	int iout;

	codegen_interp_block (cf, b, "memfn");
}

void codegen_expandfn (FILE *cf, bptr b)
{	int iw;
	logic_value_word m;

	fprintf (cf, "\tif (sp[%d])\n", b->inputs [0]->node_address);
	fprintf (cf, "\t{\n");
	for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
	{	m = node_bitmask (b->outputs [0], iw);
		fprintf (cf, "\t\tsp [%d] = 0x%x;\n", b->outputs [0]->node_address + iw, m);
	}
	fprintf (cf, "\t}\n");
	fprintf (cf, "\telse\n");
	fprintf (cf, "\t{\n");
	for (iw = 0; iw < b->outputs [0]->node_size_words; iw++)
	{	fprintf (cf, "\t\tsp [%d] = 0;\n", b->outputs [0]->node_address + iw);
	}
	fprintf (cf, "\t}\n");
	codegen_drive_outcons (cf, b, 0);
}

void codegen_priorityfn (FILE *cf, bptr b)
{	codegen_interp_block (cf, b, "priorityfn");
}

void codegen_undefinedfn (FILE *cf, bptr b) {}
