/* copyright (c) David M. Lewis 2019 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "parser.h"
#include "tortle_types.h"
#include "utils.h"
#include "blocktypes.h"
#include "debug.h"
#include "compile_net.h"
#include "nethelp.h"
#include "make_blocks.h"
#include "make_net.h"
#include "scan_parse.h"
#include "sim.h"

const char *include_file_names [] = {
	"<stdio.h>",
	"\"config.h\"",
	"\"parser.h\"",
	"\"tortle_types.h\"",
	"\"utils.h\"",
	"\"blocktypes.h\"",
	"\"debug.h\"",
	"\"compile_net.h\"",
	"\"nethelp.h\"",
	"\"make_blocks.h\"",
	"\"make_net.h\"",
	"\"scan_parse.h\"",
	"\"sim.h\"",
	"\"logic.h\"",
	"\"compiled_runtime.h\"",
""};

void
find_clock_domains (t_compiled_data *cdata)
{	nptr np;
	blistptr bp;
	bptr b;
	int iclk;
	t_nodelist *clklist_head;
	t_nodelist *nlp;
	nptr clk_input;
	char msg [maxnamelen + 100];

	/* reset all marked flags for nodes */
	for (np = allnodelist; np != NULL; np = np->hnext)
	{	np->marked_flag = false;
		np->node_id = -1;
	}

	/* find all clocked blocks and mark clock nodes. */
	clklist_head = NULL;
	cdata->n_clock_domains = 1;	/* comb is first domain */
	for (bp = allblocklist; bp != NULL; bp = bp->next)
	{	b = bp->b;
		if (b->is_clocked)
		{	clk_input = b->inputs [0];
			if (clk_input->supernode != NULL || clk_input->subnodes != NULL)
			{	sprintf (msg, "block named %s has a clock input that has subnodes or a supernode\n", b->name);
				print_error_context_stack (&(b->src_context_stack), msg);
			}
			else
			{	if (!clk_input->marked_flag)
				{	clk_input->marked_flag = true;
					nlp = (t_nodelist *) smalloc (sizeof (t_nodelist));
					nlp->next = clklist_head;
					nlp->np = clk_input;
					clklist_head = nlp;
					cdata->n_clock_domains++;
					printf ("found clock %s\n", clk_input->name);
				}
			}
		}
	}

	/* create an array of the clock nodes */

	cdata->clock_nodes = (nptr *) smalloc (cdata->n_clock_domains * sizeof (nptr));
	cdata->clock_domain_n_state_save = (int *) smalloc (cdata->n_clock_domains * sizeof (int));
	cdata->clock_nodes [0] = NULL;
	for (iclk = 1; clklist_head != NULL; iclk++)
	{	cdata->clock_nodes [iclk] = clklist_head->np;
		cdata->clock_nodes [iclk]->clock_id = iclk;
		nlp = clklist_head->next;
		smfree (clklist_head);
		clklist_head = nlp;
	}
	cdata->domain_sorted_blocks = (t_blist **) smalloc (cdata->n_clock_domains * sizeof (t_blist *));

}


void dump_block_seeds (t_compiled_data *cdata)
{	int iclk;
	t_blist *blp;

	for (iclk = 0; iclk < cdata->n_clock_domains; iclk++)
	{	for (blp = cdata->block_seed_lists [iclk]; blp != NULL; blp = blp->next)
		{	printf ("%d: %s\n", iclk, blp->b->name);
		}
		printf ("\n");
	}
}

/* Find an initial set of blocks that are guaranteed to include all source blocks in each domain.
 * For comb, this is any undriven nodes. For clocked, it is the set of ffs driven by the clock.
 * These are not necessarily first level blocks because their may be dependencies betwen them.
 */

void find_domain_seed_blocks (t_compiled_data *cdata)
{	t_nodelist *nlp;
	t_node *np;
	t_blist *blp;
	t_block *bp;
	t_blist *add_blk;
	int iclk;
	int n_saved_loc;

	cdata->block_seed_lists = (t_blist **) smalloc (cdata->n_clock_domains * sizeof (t_blist *));
	for (iclk = 0; iclk < cdata->n_clock_domains; iclk++)
	{	cdata->block_seed_lists [iclk] = NULL;
	}
	/* Clear flag of all blocks */
	for (blp = allblocklist; blp != NULL; blp = blp->next)
	{	blp->b->marked_flag = false;
	}

	/* Find any nodes without drivers; all fanouts of this node are comb seed blocks */

	for (np = allnodelist; np != NULL; np = np->hnext)
	{	if (np->sub_drivers == NULL)
		{	for (blp = np->uses; blp != NULL; blp = blp->next)
			{	if (blp->b->marked_flag == false && (blp->n_io > 1 || !blp->b->is_clocked))
				{	blp->b->marked_flag = true;
					add_blk = (t_blist *) smalloc (sizeof (t_blist));
					add_blk->next = cdata->block_seed_lists [0];
					add_blk->b = blp->b;
					add_blk->n_io = blp->n_io;
					add_blk->low = 0;
					add_blk->range.bit_low = 0;
					add_blk->range.bit_high = 0;
					cdata->block_seed_lists [0] = add_blk;
				}
			}
		}
	}


	/* Find all sync blocks and add to clock domain lists */

	for (blp = allblocklist; blp != NULL; blp = blp->next)
	{	bp = blp->b;
		if (bp->is_clocked)
		{	add_blk = (t_blist *) smalloc (sizeof (t_blist));
			iclk = bp->inputs [0]->clock_id;
			add_blk->next = cdata->block_seed_lists [iclk];
			add_blk->b = bp;
			add_blk->n_io = 0;
			add_blk->low = 0;
			add_blk->range.bit_low = 0;
			add_blk->range.bit_high = 0;
			cdata->block_seed_lists [iclk] = add_blk;
		}
	}
	if (debug_codegen_clock_domains)
	{	dump_block_seeds (cdata);
	}

	/* for clocked blocks allocated locations in node_state_saved to copy node value before updating after clock edge */
	/* note that domain 0 is comb so start at iclk = 1 */
	n_saved_loc = 0;
	for (iclk = 1; iclk < cdata->n_clock_domains; iclk++)
	{
		for (blp = cdata->block_seed_lists [iclk]; blp != NULL; blp = blp->next)
		{	blp->b->input_node_saved_loc = n_saved_loc + cdata->n_total_stuff;
			n_saved_loc += blp->b->inputs [1]->node_size_words;
		}
		if (debug_codegen_clock_domains)
			printf ("%d: %d state\n", iclk, n_saved_loc);
	}
	cdata->n_prev_node_val_words = n_saved_loc;
	cdata->n_total_stuff += n_saved_loc;
}

void print_block_fanout_graph (t_blist *domain_blp)
{   t_blist *blp;
	t_blist *eblp;

	for (blp = domain_blp; blp != NULL; blp = blp->next)
	{	printf ("block %s [%d] -> ", blp->b->name, blp->b->block_fanin_count);
		for (eblp = blp->b->block_fanout_list; eblp != NULL; eblp = eblp->next)
		{	printf (" %s:%d", eblp->b->name, eblp->n_io);
		}
		printf ("\n");
	}
}


/* build the dependency graph of blocks in this domain, using the seed set as a starting point */

t_blist *build_block_graph (t_blist *seeds)
{	t_blist *blp;
	t_blist *all_blp;
	t_blist *this_blp;
	t_blist *fanout_blp;
	t_block *fanout_bp;
	t_blist *new_edge;
	t_blist *new_blp;
	t_block *bp;
	int iout;
	int iconn;

	/* mark all seed blocks */
	for (blp = seeds; blp != NULL; blp = blp->next)
	{	blp->b->marked_flag = true;
	}

	/* create list of blocks starting with seeds, and as long as not empty, add the fanouts */

	all_blp = NULL;
	blp = seeds;
	while (blp != NULL)
	{	this_blp = (t_blist *) smalloc (sizeof (t_blist));
		this_blp->b = blp->b;
		this_blp->next = all_blp;
		this_blp->n_io = 0;
		this_blp->low = 0;
		this_blp->range.bit_low = 0;
		this_blp->range.bit_high = 0;
		blp = blp->next;
		all_blp = this_blp;
		bp = this_blp->b;
		for (iout = 0; iout < bp->noutputs; iout++)
		{	for (iconn = 0; iconn < bp->noutcons [iout]; iconn++)
			{	/* look at every output connection and add uses of all nodes in each connection as fanouts */
				for (fanout_blp = bp->outcons [iout] [iconn].node->uses; fanout_blp != NULL; fanout_blp = fanout_blp->next)
				{	fanout_bp = fanout_blp->b;
					if (fanout_blp->n_io > 1 || !fanout_bp->is_clocked)
					{	new_edge = (t_blist *) smalloc (sizeof (t_blist));
						new_edge->b = fanout_bp;
						new_edge->next = bp->block_fanout_list;
						new_edge->n_io = fanout_blp->n_io;
						new_edge->low = 0;
						new_edge->range.bit_low = 0;
						new_edge->range.bit_high = 0;
						bp->block_fanout_list = new_edge;
//						printf ("edge %s %s:%d\n", bp->name, fanout_bp->name, fanout_blp->n_io);
						fanout_bp->block_fanin_count++;
						/* if this is the first occurence of this block, add it to the list to follow */
						if (!fanout_bp->marked_flag)
						{	fanout_bp->marked_flag = true;
							new_blp = (t_blist *) smalloc (sizeof (t_blist));
							new_blp->b = fanout_bp;
							new_blp->next = blp;
							blp = new_blp;
							new_blp->n_io = -1;
							new_blp->low = 0;
							new_blp->range.bit_low = 0;
							new_blp->range.bit_high = 0;
						}
					}
				}
			}
		}
	}

	return all_blp;
}

t_blist *sort_block_graph (t_blist *unordered_graph)
{	t_blist *ready_head;
	t_blist *ready_tail;
	t_blist *new_blp;
	t_blist *blp;
	t_blist *eblp;
	t_block *bp;
	t_blist *fanout_blp;
	t_block *fanout_bp;
	int iout;
	int iconn;
	int n_blocks;

	ready_head = NULL;
	ready_tail = NULL;

	/* initialize the ready list to the set of blocks with 0 fanin */
	/* keep track of number of blocks in n_blocks and it should be 0 when done or there are loops */

	n_blocks = 0;
	for (blp = unordered_graph; blp != NULL; blp = blp->next)
	{	n_blocks++;
		if (blp->b->block_fanin_count == 0)
		{	new_blp = (t_blist *) smalloc (sizeof (t_blist));
			*new_blp = *blp;
			new_blp->next = NULL;
			if (ready_head == NULL)
			{	ready_head = new_blp;
			}
			else
			{	ready_tail->next = new_blp;
			}
			ready_tail = new_blp;
			n_blocks--;
			blp->b->marked_flag = false;
		}
	}

	/* walk down the ready list, decrementing all fanouts, and appending the fanout to the ready list if fanin count becomes 0 */

	for (blp = ready_head; blp != NULL; blp = blp->next)
	{   bp = blp->b;
		while (bp->block_fanout_list != NULL)
		{	eblp = bp->block_fanout_list;
			bp->block_fanout_list = bp->block_fanout_list->next;
			/* decrement fanin count of target block, if 0 it can now be scheduled */
			eblp->b->block_fanin_count--;
			if (eblp->b->block_fanin_count == 0)
			{	new_blp = (t_blist *) smalloc (sizeof (t_blist));
				*new_blp = *eblp;
				new_blp->next = NULL;
				if (ready_head == NULL)
				{	ready_head = new_blp;
				}
				else
				{	ready_tail->next = new_blp;
				}
				ready_tail = new_blp;
				n_blocks--;
				eblp->b->marked_flag = false;
			}
			smfree (eblp);
		}
	}
	if (n_blocks != 0)
	{	printf ("combinational loop\n");
	}
    return (ready_head);

}

void levelize_clock_domains (t_compiled_data *cdata)
{	int iclk;
	t_blist *domain_blocks;
	t_blist *blp;

	for (blp = allblocklist; blp != NULL; blp = blp->next)
	{	blp->b->marked_flag = false;
		blp->b->block_fanout_list = NULL;
		blp->b->block_fanin_count = 0;
	}
	
	for (iclk = 0; iclk < cdata->n_clock_domains; iclk++)
	{	domain_blocks = build_block_graph (cdata->block_seed_lists [iclk]);
		if (debug_block_fanout_graph)
		{	printf ("clock %s fanout graph\n", iclk == 0 ? "<comb>" : cdata->clock_nodes [iclk]->name);
			print_block_fanout_graph (domain_blocks);
		}
		cdata->domain_sorted_blocks [iclk] = sort_block_graph (domain_blocks);
		if (debug_block_fanout_graph)
		{	printf ("clock %s fanout graph\n", iclk == 0 ? "<comb>" : cdata->clock_nodes [iclk]->name);
			print_block_fanout_graph (cdata->domain_sorted_blocks [iclk]);
		}
	}
}

void enum_src_context (t_compiled_data *cdata, t_src_context *sc, bool reset_id, bool append_to_list)
{	if (sc != NULL)
	{	if (reset_id)
		{	sc->sc_id = -1;
		} else if (sc->sc_id == -1)
		{	sc->sc_id = cdata->n_src_context++;
			if (append_to_list)
			{	cdata->ordered_src_contexts [sc->sc_id].filenamesym = sc->filenamesym;
				cdata->ordered_src_contexts [sc->sc_id].linenum = sc->linenum;
				cdata->ordered_src_contexts [sc->sc_id].sc_id = sc->sc_id;
			}
		}
	}
}

void enum_src_context_stack (t_compiled_data *cdata, t_src_context_stack *scs, bool reset_id, bool append_to_list)
{	if (scs != NULL)
	{	enum_src_context (cdata, scs->sc, reset_id, append_to_list);
		if (reset_id)
		{	scs->scs_id = -1;
			enum_src_context (cdata, scs->sc, reset_id, append_to_list);
			enum_src_context_stack (cdata, scs->next, reset_id, append_to_list);
		} else
		{	if (scs->scs_id == -1)
			{	scs->scs_id = cdata->n_src_context_stack++;
				if (append_to_list)
				{	cdata->ordered_src_context_stacks [scs->scs_id].next = scs->next;
					cdata->ordered_src_context_stacks [scs->scs_id].sc = scs->sc;
					cdata->ordered_src_context_stacks [scs->scs_id].scs_id = scs->scs_id;
				}
				enum_src_context (cdata, scs->sc, reset_id, append_to_list);
				enum_src_context_stack (cdata, scs->next, reset_id, append_to_list);
			}
		}
	}
}

void print_blist (FILE *tf, t_blist *bp)
{	t_blist *tbp;

	for (tbp = bp; tbp != NULL; tbp = tbp->next)
	{	my_assert (tbp->b->block_id != -1, "block_id is -1 in compile");
		fprintf (tf, " %d %d %d %d %d", tbp->b->block_id, tbp->n_io, tbp->low, tbp->range.bit_low, tbp->range.bit_high);
	}
	fprintf (tf, " -1\n");
}


void order_and_alloc_blocks_and_nodes (t_compiled_data *cdata)
{	t_blist *bp;
	t_node *np;
	t_symrec *sp;
	t_symrec *all_syms;

	cdata->n_nodes = 0;
	cdata->n_blocks = 0;
	cdata->n_syms = 0;
	cdata->n_total_stuff = 0;
	cdata->n_src_context = 0;
	cdata->n_src_context_stack = 0;

	for (np = allnodelist; np != NULL; np = np->hnext)
	{	np->node_id = cdata->n_nodes++;
		np->node_address = cdata->n_total_stuff;
		cdata->n_total_stuff += np->node_size_words;
		/* we will only merge next entries of src_context_stack since each node and block is allocated an entire unique entry.
		 * Could change this if we uniquify src_context_stacks and point to them from nodes and blocks
		 */
		enum_src_context_stack (cdata, np->src_context_stack.next, false, false);
		enum_src_context (cdata, np->src_context_stack.sc, false, false);
	}

	cdata->ordered_nodes = (t_node **) smalloc (cdata->n_nodes * sizeof (t_node *));

	for (np = allnodelist; np != NULL; np = np->hnext)
	{	cdata->ordered_nodes [np->node_id] = np;
	}

	for (bp = allblocklist; bp != NULL; bp = bp->next)
	{	bp->b->block_id = cdata->n_blocks++;
		bp->b->state_address = cdata->n_total_stuff;
		cdata->n_total_stuff += bp->b->nstate;
		enum_src_context_stack (cdata, bp->b->src_context_stack.next, false, false);
		enum_src_context (cdata, bp->b->src_context_stack.sc, false, false);
	}

	cdata->ordered_blocks = (t_block **) smalloc (cdata->n_blocks * sizeof (t_block *));

	for (bp = allblocklist; bp != NULL; bp = bp->next)
	{	cdata->ordered_blocks [bp->b->block_id] = bp->b;
	}

	/* allocate src_context and stacks, reset ids, and then create actual data */

	cdata->ordered_src_context_stacks = (t_src_context_stack *) smalloc (cdata->n_src_context_stack * sizeof (t_src_context_stack));
	cdata->ordered_src_contexts = (t_src_context *) smalloc (cdata->n_src_context * sizeof (t_src_context));

	for (np = allnodelist; np != NULL; np = np->hnext)
	{	enum_src_context_stack (cdata, np->src_context_stack.next, true, false);
		enum_src_context (cdata, np->src_context_stack.sc, true, false);
	}

	for (bp = allblocklist; bp != NULL; bp = bp->next)
	{	enum_src_context_stack (cdata, bp->b->src_context_stack.next, true, false);
		enum_src_context (cdata, bp->b->src_context_stack.sc, true, false);
	}

	cdata->n_src_context = 0;
	cdata->n_src_context_stack = 0;

	for (np = allnodelist; np != NULL; np = np->hnext)
	{	enum_src_context_stack (cdata, np->src_context_stack.next, false, true);
		enum_src_context (cdata, np->src_context_stack.sc, false, true);
	}

	for (bp = allblocklist; bp != NULL; bp = bp->next)
	{	enum_src_context_stack (cdata, bp->b->src_context_stack.next, false, true);
		enum_src_context (cdata, bp->b->src_context_stack.sc, false, true);
	}

	all_syms = all_sym_list ();
	for (sp = all_syms; sp != NULL; sp = sp->next_match)
	{	if (sp->n_array_elems > 0 || sp->nodedef != NULL || sp->blockdef != NULL || sp->is_filename)
		{	sp->sym_id = cdata->n_syms++;
		}
	}

	cdata->ordered_syms = (t_symrec **) smalloc (cdata->n_syms * sizeof (t_symrec *));

	for (sp = all_syms; sp != NULL; sp = sp->next_match)
	{	if (sp->n_array_elems > 0 || sp->nodedef != NULL || sp->blockdef != NULL || sp->is_filename)
		{	cdata->ordered_syms [sp->sym_id] = sp;
		}
	}
}

void write_node_descr (FILE *df, t_node *np)
{	t_subnode *snp;
	t_symrec *sp;

	sp = sym_hash_lookup (np->name);
	my_assert (sp->sym_id >= 0, "node name must be a symbol");
	fprintf (df, "%d", sp->sym_id);
	fprintf (df, " %d %d %d", np->node_size.bit_low, np->node_size.bit_high, np->node_size_words);
	fprintf (df, " %d", np->src_context_stack.next == NULL ? -1 : np->src_context_stack.next->scs_id);
	fprintf (df, " %d", np->src_context_stack.sc == NULL ? -1 : np->src_context_stack.sc->sc_id);
	fprintf (df, " %d\n", np->clock_id);
	print_blist (df, np->uses);
	print_blist (df, np->drivers);
	print_blist (df, np->sub_drivers);
	if (np->supernode != NULL)
	{	fprintf (df, "%d %d %d\n", np->supernode->node_id, np->supernode_range.bit_low, np->supernode_range.bit_high);
	} else
	{	fprintf (df, "%d\n", -1);
	}
	for (snp = np->subnodes; snp != NULL; snp = snp->next)
	{	fprintf (df, " %d %d %d %d", snp->node->node_id, snp->subrange.bit_low, snp->subrange.bit_high, snp->low);
	}
	fprintf (df, " %d\n", -1);
	fprintf (df, "%d\n", np->node_address);
}

void write_block_descr (FILE *df, t_block *bp)
{   int i;
	int iconn;
	connection *cp;

	fprintf (df, "%d", sym_hash_lookup (bp->name)->sym_id);
	fprintf (df, " %s", bp->def_name);
	fprintf (df, " %d", bp->src_context_stack.next == NULL ? -1 : bp->src_context_stack.next->scs_id);
	fprintf (df, " %d", bp->src_context_stack.sc == NULL ? -1 : bp->src_context_stack.sc->sc_id);
	fprintf (df, " %d", bp->ninputs);
	fprintf (df, " %d", bp->noutputs);
	fprintf (df, " %d", bp->nstate);
	fprintf (df, " %d", bp->state_address);
	fprintf (df, " %d", bp->input_node_saved_loc);
	fprintf (df, "\n");

	for (i = 0; i < bp->ninputs; i++)
	{	fprintf (df, " %d", bp->inputs [i]->node_id);
	}
	fprintf (df, "\n");

	for (i = 0; i < bp->noutputs; i++)
	{	fprintf (df, " %d %d", bp->outputs [i]->node_id, bp->noutcons [i]);
		for (iconn = 0; iconn < bp->noutcons [i]; iconn++)
		{	cp = &(bp->outcons [i] [iconn]);
			fprintf (df, " %d %d %d %d", cp->node->node_id, cp->conn_sh, cp->conn_bitrange.bit_low, cp->conn_bitrange.bit_high);
		}
		fprintf (df, "\n");
	}

	for (i = 0; i < (int) bp->nstate; i++)
	{	fprintf (df, " %x", bp->state [i]);
		if (i % 16 == 15 || i == (int) bp->nstate - 1)
			fprintf (df, "\n");

	}

}


void write_sym_descr (FILE *df, t_symrec *sp)
{	int i;

	fprintf (df, "%s", sp->name);
	if (sp->nodedef != NULL)
	{	fprintf (df, " %d", sp->nodedef->node_id);
	} else
	{	fprintf (df, " %d", -1);
	}
	if (sp->blockdef != NULL)
	{	fprintf (df, " %d", sp->blockdef->block_id);
	} else
	{	fprintf (df, " %d", -1);
	}
	fprintf (df, " %d", sp->n_array_elems);
	for (i = 0; i < sp->n_array_elems; i++)
	{	fprintf (df, " %d", sp->array_elems [i]->sym_id);
	}
	fprintf (df, "\n");
}


void write_src_context_descr (FILE *df, t_src_context sc)
{	fprintf (df, "%d %d\n", sc.filenamesym == NULL ? -1 : sc.filenamesym->sym_id, sc.linenum);
}

void write_src_context_stack_descr (FILE *df, t_src_context_stack scs)
{	fprintf (df, "%d %d\n", scs.next == NULL ? -1 : scs.next->scs_id,
		scs.sc == NULL ? -1 : scs.sc->sc_id);

}

void write_descr (t_compiled_data *cdata)
{	FILE *df;
	int iidx;

	df = cdata->descr_file;
	fprintf (df, "%d %d %d %d %d %d %d %d\n", cdata->n_nodes, cdata->n_blocks, cdata->n_syms, cdata->n_total_stuff,
			cdata->n_prev_node_val_words, cdata->n_src_context, cdata->n_src_context_stack, cdata->n_clock_domains);

	for (iidx = 0; iidx < cdata->n_syms; iidx++)
	{	write_sym_descr (df, cdata->ordered_syms [iidx]);
	}

	for (iidx = 0; iidx < cdata->n_nodes; iidx++)
	{	write_node_descr (df, cdata->ordered_nodes [iidx]);
	}

	for (iidx = 0; iidx < cdata->n_blocks; iidx++)
	{	write_block_descr (df, cdata->ordered_blocks [iidx]);
	}

	for (iidx = 0; iidx < cdata->n_src_context; iidx++)
	{	my_assert (cdata->ordered_src_contexts [iidx].sc_id == iidx, "enumerating src_contexts in write_descr");
		write_src_context_descr (df, cdata->ordered_src_contexts [iidx]);
	}

	for (iidx = 0; iidx < cdata->n_src_context_stack; iidx++)
	{	my_assert (cdata->ordered_src_context_stacks [iidx].scs_id == iidx, "enumerating src_context_stacks in write_descr");
		write_src_context_stack_descr (df, cdata->ordered_src_context_stacks [iidx]);
	}


}

void generate_clock_code (t_compiled_data *cdata, int iclk)
{   FILE *cf;
	t_blist *blp;
	t_block *bp;
	int iw;

	cf = cdata->code_file;
	fprintf (cf, "void sim_domain_%d (void)\n", iclk);
	fprintf (cf, "{\n");
	fprintf (cf, "\tlogic_value_word *sp;\n");
	fprintf (cf, "\tsp = compiled_code_data.stuff;\n");

	/* copy current node state for all FFs. */
	if (iclk > 0)
	{	for (blp = cdata->block_seed_lists [iclk]; blp != NULL; blp = blp->next)
		{	bp = blp->b;
			for (iw = 0; iw < bp->inputs [1]->node_size_words; iw++)
			{	fprintf (cf, "\tsp [%d] = sp [%d];\n", bp->input_node_saved_loc + iw, bp->inputs [1]->node_address + iw);
			}
		}
	}

	/* generate all code */

	for (blp = cdata->domain_sorted_blocks [iclk]; blp != NULL; blp = blp->next)
	{	if (debug_compiled_comments)
		{	fprintf (cf, "\t/* block %s */\n", blp->b->name);
		}
		if (blp->b->codegen_fn == NULL)
		{	print_error_context_stack (&blp->b->src_context_stack, "block type is not implemented for compiled code");
		}
		else
		{	(*(blp->b->codegen_fn)) (cf, blp->b);
		}
	}
	fprintf (cf, "}\n");

}

void generate_code (t_compiled_data *cdata)
{	FILE *cf;
	int iclk;
	int i;


	cf = cdata->code_file;

	for (i = 0; include_file_names [i] [0] != '\0'; i++)
	{	fprintf (cf, "#include %s\n", include_file_names [i]);
	}
	fprintf (cf, "\n");
	fprintf (cf, "char *descr_file_name = \"%s\";\n", cdata->descr_file_name);

	for (iclk = 0; iclk < cdata->n_clock_domains; iclk++)
	{	generate_clock_code (cdata, iclk);
	}

	fprintf (cf, "void simulate_domain (int idomain)\n");
	fprintf (cf, "{\n");
	fprintf (cf, "\tswitch (idomain)\n");
	fprintf (cf, "\t{\n");
	for (iclk = 0; iclk < cdata->n_clock_domains; iclk++)
	{	fprintf (cf, "\tcase %d:\n", iclk);
		fprintf (cf, "\t\tsim_domain_%d ();\n", iclk);
		fprintf (cf, "\t\tbreak;\n");
	}
	fprintf (cf, "\t}\n");
	fprintf (cf, "}\n");

}


void codegen_net (void)
{	t_compiled_data cdata;

	cdata.descr_file_name = "foo.descr";
	cdata.code_file = fopen ("codegen_netlist.cpp", "wb");
	cdata.descr_file = fopen (cdata.descr_file_name, "wb");

	find_clock_domains (&cdata);
	order_and_alloc_blocks_and_nodes (&cdata);
	find_domain_seed_blocks (&cdata);
	levelize_clock_domains (&cdata);
	write_descr (&cdata);
	generate_code (&cdata);

	fclose (cdata.code_file);
	fclose (cdata.descr_file);

}
