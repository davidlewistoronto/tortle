/* copyright (c) David M. Lewis 2019 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tortle_types.h"
#include "utils.h"
#include "compiled_runtime.h"
#include "scan_parse.h"
#include "sim.h"

t_compiled_data compiled_code_data;

void read_syms (FILE *df, t_compiled_data *cd)
{	int isym;
	int nsubs;
	int isub;
	int isym_sub;
	t_symrec *sp;
	char sym_name [maxnamelen + 1];
	int inode;
	int iblock;

	for (isym = 0; isym < cd->n_syms; isym++)
	{	fscanf (df, " %s %d %d %d", sym_name, &inode, &iblock, &nsubs);
//		printf ("%d %d %s %d %d\n", isym, cd->n_syms, sym_name, inode, iblock);
		sp = cd->ordered_syms [isym];
		sp->defdef = NULL;
		sp->makeblock = NULL;
		sp->const_val = NULL;
		sp->tracebits = NULL;
		sp->next = NULL;
		sp->next_match = NULL;
		sp->sym_id = isym;
		sp->is_filename = false;;
		sp->node_save_trace = false;;
		sp->file_included = false;;
		sp->name = newstring (sym_name);
		if (inode == -1)
		{	sp->nodedef = NULL;
		} else
		{	sp->nodedef = cd->ordered_nodes [inode];
		}
		if (iblock == -1)
		{	sp->blockdef = NULL;
		}
		else
		{	sp->blockdef = cd->ordered_blocks [iblock];
		}
		sp->n_array_elems = nsubs;
		if (sp->n_array_elems != 0)
		{	sp->array_elems = (t_symrec **) smalloc (nsubs * sizeof (t_symrec *));
			for (isub = 0; isub < nsubs; isub++)
			{	fscanf (df, " %d", &isym_sub);
				sp->array_elems [isub] = cd->ordered_syms [isym_sub];
			}
		}
		else
		{	sp->array_elems = NULL;
		}
		add_to_sym_hash_tbl (sp);
	}
}

t_blist *read_blist (FILE *df, t_compiled_data *cd)
{	t_blist *blp;
	t_blist *blp_last;
	t_blist *blp_first;
	int iblock;

	blp_first = NULL;
	do
	{	fscanf (df, " %d", &iblock);
		if (iblock != -1)
		{	blp = (t_blist *) smalloc (sizeof (t_blist));
			blp->b = cd->ordered_blocks [iblock];
			fscanf (df, " %d %d %d %d", &(blp->n_io), &(blp->low), &(blp->range.bit_low), &(blp->range.bit_high));
			if (blp_first == NULL)
				blp_first = blp;
			else
				blp_last->next = blp;
			blp->next = NULL;
			blp_last = blp;
		}
	} while (!feof (df) && iblock != -1);

	return blp_first;
}

void read_nodes (FILE *df, t_compiled_data *cd)
{	int inode;
	int isupernode;
	int isubnode;
	t_bitrange br;
	t_node *np;
	int isym;
	int isc;
	int iscs;
	int iclk;
	int ilow;
	t_subnode *last_snp;
	t_subnode *snp;

	for (inode = 0; inode < cd->n_nodes; inode++)
	{	np = cd->ordered_nodes [inode];
		/* default stuff */

		if (inode > 0)
			np->hnext = cd->ordered_nodes [inode - 1];
		else
			np->hnext = NULL;

		np->node_id = inode;
		np->traced = false;
		np->save_trace = false;
		np->marked_flag = false;
		np->nndrivers = 0;
		np->multiply_driven = 0;

		fscanf (df, " %d %d %d %d", &isym, &np->node_size.bit_low, &np->node_size.bit_high, &np->node_size_words);
		np->name = cd->ordered_syms [isym]->name;
		np->bitmask = bitwidmask (np->node_size.bit_high + 1);


		fscanf (df, " %d %d %d", &iscs, &isc, &iclk);
		if (iscs != -1)
			np->src_context_stack.next = &(cd->ordered_src_context_stacks [iscs]);
		else
			np->src_context_stack.next = NULL;
		if (isc != -1)
			np->src_context_stack.sc = &(cd->ordered_src_contexts [isc]);
		else
			np->src_context_stack.sc = NULL;

		np->clock_id = iclk;

		np->uses = read_blist (df, cd);
		np->drivers = read_blist (df, cd);
		np->sub_drivers = read_blist (df, cd);

		fscanf (df, " %d", &isupernode);

		/* read supernode, if any */

		if (isupernode != -1)
		{	fscanf (df, " %d %d", &br.bit_low, &br.bit_high);
			np->supernode = cd->ordered_nodes [isupernode];
			np->supernode_range = br;
		}
		else
		{	np->supernode = NULL;
		}

		/* read subnodes and append in order written */

		np->subnodes = NULL;
		do
		{	fscanf (df, " %d", &isubnode);
			if (isubnode != -1)
			{	fscanf (df, " %d %d %d", &br.bit_low, &br.bit_high, &ilow);
				snp = (t_subnode *) smalloc (sizeof (t_subnode));
				snp->node = cd->ordered_nodes [isubnode];
				snp->subrange = br;
				snp->low = ilow;
				snp->next = NULL;
				if (np->subnodes == NULL)
				{	np->subnodes = snp;
				} else {
					last_snp->next = snp;
				}
				last_snp = snp;
			}

		} while (!feof (df) && isubnode != -1);

		fscanf (df, " %d", &np->node_address);
		np->node_value = &(cd->stuff [np->node_address]);
		np->node_value_next = &(cd->stuff [np->node_address]);
	}
	allnodelist = cd->ordered_nodes [cd->n_nodes - 1];
}

void read_blocks (FILE *df, t_compiled_data *cd)
{	int inode;
	int iblock;
	t_block *bp;
	t_bitrange br;
	t_node *np;
	int isym;
	int isc;
	int iscs;
	int iclk;
	int iin;
	int iout;
	int iconn;
	int i;
	connection *cp;
	char def_name [maxnamelen + 1];

	for (iblock = 0; iblock < cd->n_blocks; iblock++)
	{	bp = cd->ordered_blocks [iblock];

		bp->block_id = iblock;
		bp->block_fanin_count = 0;
		bp->active = false;
		bp->debugblock = false;
		bp->is_clocked = false;
		bp->marked_flag = false;


		fscanf (df, " %d %s %d %d %d %d %d %d %d", &isym, def_name, &iscs, &isc, &(bp->ninputs),
				&(bp->noutputs), &(bp->nstate), &(bp->state_address), &(bp->input_node_saved_loc));
		bp->name = cd->ordered_syms [isym]->name;
		bp->def_name = newstring (def_name);
		bp->inputs = (t_node **) smalloc (bp->ninputs * sizeof (t_node *));
		bp->outputs = (t_node **) smalloc (bp->noutputs * sizeof (t_node *));
		bp->output_widths = (t_bitrange *) smalloc (bp->noutputs * sizeof (t_bitrange));
		bp->output_widths_words = (int *) smalloc (bp->noutputs * sizeof (int));
		bp->block_fanin_count = 0;
		
		if (iscs != -1)
			bp->src_context_stack.next = &(cd->ordered_src_context_stacks [iscs]);
		else
			bp->src_context_stack.next = NULL;
		if (isc != -1)
			bp->src_context_stack.sc = &(cd->ordered_src_contexts [isc]);
		else
			bp->src_context_stack.sc = NULL;

		bp->state = &(cd->stuff [bp->state_address]);
		for (iin = 0; iin < bp->ninputs; iin++)
		{	fscanf (df, " %d", &inode);
			bp->inputs [iin] = cd->ordered_nodes [inode];
		}
		bp->noutcons = (int *) smalloc (bp->noutputs * sizeof (int));
		bp->outcons = (connection **) smalloc (bp->noutputs * sizeof (connection *));
		bp->output_values = (logic_value *) smalloc (bp->noutputs * sizeof (logic_value));
		bp->output_values_next = (logic_value *) smalloc (bp->noutputs * sizeof (logic_value));
		bp->outputactive = (bool *) smalloc (bp->noutputs * sizeof (bool));
		for (iout = 0; iout < bp->noutputs; iout++)
		{	fscanf (df, " %d %d", &inode, &(bp->noutcons [iout]));
			bp->outputs [iout] = cd->ordered_nodes [inode];
			bp->outputactive [iout] = false;
			bp->output_widths [iout] = bp->outputs [iout]->node_size;
			bp->output_widths_words [iout] = bp->outputs [iout]->node_size_words;

			/* Point output values to node directly since 0 delay simulation will update instantly.
			 * This enables reusing original unit delay event driven block simulation code
			 */
			bp->output_values [iout] = bp->outputs [iout]->node_value;
			bp->output_values_next [iout] = bp->outputs [iout]->node_value;
			bp->outcons [iout] = (connection *) smalloc (bp->noutcons [iout] * sizeof (connection));
			for (iconn = 0; iconn < bp->noutcons [iout]; iconn++)
			{	cp = &(bp->outcons [iout] [iconn]);
				fscanf (df, " %d %d %d %d", &inode, &(cp->conn_sh), &(cp->conn_bitrange.bit_low), &(cp->conn_bitrange.bit_high));
				cp->node = cd->ordered_nodes [inode];
			}
		}
		for (i = 0; i < (int) bp->nstate; i++)
		{	fscanf (df, " %x", &(bp->state [i]));
		}
	}
}

void read_src_contexts (FILE *df, t_compiled_data *cd)
{	int isc;
	int isym;

	for (isc = 0; isc < cd->n_src_context; isc++)
	{	fscanf (df, " %d %d", &isym, &(cd->ordered_src_contexts [isc].linenum));
		cd->ordered_src_contexts [isc].filenamesym = cd->ordered_syms [isym];
	}
}

void read_src_context_stacks (FILE *df, t_compiled_data *cd)
{	int iscs;
	int isc;
	int iscs_next;

	for (iscs = 0; iscs < cd->n_src_context_stack; iscs++)
	{	fscanf (df, " %d %d", &iscs_next, &isc);
		if (iscs_next == -1)
		{	cd->ordered_src_context_stacks [iscs].next = NULL;
		} else
		{	cd->ordered_src_context_stacks [iscs].next = &(cd->ordered_src_context_stacks [iscs_next]);
		}
		cd->ordered_src_context_stacks [iscs].sc = &(cd->ordered_src_contexts [isc]);
	}
}


void read_descr_and_alloc_net (void)
{	FILE *df;
	t_compiled_data *cd;
	int i;
	int nsubs;
	t_symrec *sp;

	df = fopen (descr_file_name, "rb");
	cd = &compiled_code_data;
	if (df == NULL)
	{	fprintf (stderr, "can't open compiled description file %s\n", descr_file_name);
		exit (1);
	}
	fscanf (df, " %d %d %d %d %d %d %d %d\n", &cd->n_nodes, &cd->n_blocks, &cd->n_syms,
			&cd->n_total_stuff, &cd->n_prev_node_val_words,
			&cd->n_src_context, &cd->n_src_context_stack, &cd->n_clock_domains);

	cd->stuff = (logic_value_word *) smalloc (cd->n_total_stuff * sizeof (logic_value_word));
	for (i = 0; i < cd->n_total_stuff; i++)
		cd->stuff [i] = 0;

	cd->ordered_nodes = (t_node **) smalloc (cd->n_nodes * sizeof (t_node *));
	for (i = 0; i < cd->n_nodes; i++)
	{	cd->ordered_nodes [i] = (t_node *) smalloc (sizeof (t_node));
	}

	cd->ordered_blocks = (t_block **) smalloc (cd->n_blocks * sizeof (t_block *));
	for (i = 0; i < cd->n_blocks; i++)
	{	cd->ordered_blocks [i] = (t_block *) smalloc (sizeof (t_block));
	}

	cd->ordered_syms = (t_symrec **) smalloc (cd->n_syms * sizeof (t_symrec *));
	for (i = 0; i < cd->n_syms; i++)
	{	cd->ordered_syms [i] = (t_symrec *) smalloc (sizeof (t_symrec));
	}

	cd->ordered_src_context_stacks = (t_src_context_stack *) smalloc (cd->n_src_context_stack * sizeof (t_src_context_stack));
	cd->ordered_src_contexts = (t_src_context *) smalloc (cd->n_src_context * sizeof (t_src_context));

	read_syms (df, cd);
	read_nodes (df, cd);
	read_blocks (df, cd);
	read_src_contexts (df, cd);
	read_src_context_stacks (df, cd);

}
