/* copyright (c) David M. Lewis 1987 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>

#include "config.h"
#include "utils.h"
#include "parser.h"
#include "tortle_types.h"
#include "scan_parse.h"
#include "sim.h"
#include "describe.h"

#include "debug.h"


void report_multi_driven_nodes (void)
{	nptr n;
	bool mdriven;
	int iw;
	int ibit;
	logic_value_word multi_driven [max_node_words];


	if (nmultidrivenodes > 0)
	{	for (n = allnodelist; n != (nptr) NULL; n = n->hnext)
		{	mdriven = false;
			for (iw = 0; iw < n->node_size_words; iw++)
			{   multi_driven [iw] = 0;
				for (ibit = 1; ibit < n->nndrivers; ibit++)
				{   if (n->node_drive_count [iw] [ibit])
					{   mdriven = true;
					}
					multi_driven [iw] |= n->node_drive_count [iw] [ibit];
				}
			}
			if (mdriven)
			{	printf ("node %s is multiply driven in bits %s\n", n->name, print_logic_value_base (multi_driven, n->node_size.bit_high + 1));
				describe_node (sym_hash (n->name));
			}
		}
	}
}


void describe_node_drives (nptr n)
{	blistptr bp;
	bptr b;
	int i;
	int iw;
	nptr n1;

#ifndef compiled_runtime
	printf ("drive bits ");
	for (i = 0; i < n->nndrivers; i++) {
		printf (" %d: ", i);
		for (iw = n->node_size_words - 1; iw >= 0; iw--) {
			printf ("%08x", n->node_drive_count [iw] [i]);
		}
	}
	printf ("\n");
#endif
	printf ("drives blocks ");
	/* TODO: this should be extended to include subnodes */
	for (bp = n->uses; bp != NULL; bp = bp->next)
		printf (" %s:%d", bp->b->name, bp->n_io);
	printf ("\ndriven by\n");
	for (bp = n->drivers; bp != NULL; bp = bp->next)
	{	b = bp->b;
		printf (" %s", b->name);
		for (i = 0; i < b->noutputs; i++)
		{	if (b->outputs [i] == n)
				printf ("-%d (%s) ", i, b->outputactive [i] ? "active" : "inactive");
		}
		printf (" (");
		for (i = 0; i < b->ninputs; i++)
		{	n1 = b->inputs [i];
			printf ("%s = %s%c ", n1->name, print_logic_value_base (n1->node_value, n1->node_size.bit_high + 1), i == b->ninputs - 1 ? ')' : ',');
		}
		printf ("\n");
	}
	printf ("sub driven by\n");
	for (bp = n->sub_drivers; bp != NULL; bp = bp->next)
	{	b = bp->b;
		if (debug_subnodes)
			printf ("\nblock %s out %d low %d bits %d..%d\n", bp->b->name, bp->n_io, bp->low, bp->range.bit_low, bp->range.bit_high);
		if (bp->low || bp->range.bit_high != n->node_size.bit_high)
			printf (" shift %d masked {%d..%d}", bp->low, bp->range.bit_low, bp->range.bit_high);
		printf (" %s-%d (%s)", b->name, bp->n_io, b->outputactive [bp->n_io] ? "active" : "inactive");
		printf (" (");
		for (i = 0; i < b->ninputs; i++)
		{	n1 = b->inputs [i];
			printf ("%s = %s%c ", n1->name, print_logic_value_base (n1->node_value, n1->node_size.bit_high + 1), i == b->ninputs - 1 ? ')' : ',');
		}
		printf ("\n");
	}
}


void print_src_context_stack (t_src_context_stack *scs, t_src_context *sc)
{	t_src_context_stack *scstk;

	if (sc != NULL)
	{	printf ("created at file %s line %d\n", sc->filenamesym->name, sc->linenum);
		for (scstk = scs; scstk != NULL; scstk = scstk->next)
		{	printf ("called from file %s line %d\n", scstk->sc->filenamesym->name, scstk->sc->linenum);
		}
	}
}


void describe_node (symptr s)
{	int ibit;
	nptr n;
	nptr n1;
	logic_value_word multi_driven [max_node_words];
	logic_value_word is_driven [max_node_words];
	int iw;
	bool mdriven;

	if (s->nodedef == (nptr) NULL)
		printf ("no such node: %s\n", s->name);
	else
	{	n = s->nodedef;
		printf ("node %s", s->name);
		if (strcmp (s->name, n->name))
			printf (" -> %s ", n->name);
		printf (" = %s ", print_logic_value_base (n->node_value, n->node_size.bit_high + 1));
		for (n1 = n; n1->supernode != NULL; n1 = n1->supernode)
		{	printf (" is {%d..%d} of %s ", n1->supernode_range.bit_low, n1->supernode_range.bit_high,
				n1->supernode->name);
		}
		printf ("\n");
		print_src_context_stack (n->src_context_stack.next, n->src_context_stack.sc);

#ifndef compiled_runtime

		for (iw = 0; iw < n->node_size_words; iw++)
		{   is_driven [iw] = 0;
			for (ibit = 0; ibit < n->nndrivers; ibit++)
			{   is_driven [iw] |= n->node_drive_count [iw] [ibit];
			}
		}

		mdriven = false;
		for (iw = 0; iw < n->node_size_words; iw++)
		{   multi_driven [iw] = 0;
			for (ibit = 1; ibit < n->nndrivers; ibit++)
			{   multi_driven [iw] |= n->node_drive_count [iw] [ibit];
			}
			if (multi_driven [iw] != 0)
			{   mdriven = true;
			}
		}

		if (mdriven)
			printf ("multiply driven in bits %s ", print_logic_value_base (multi_driven, n->node_size.bit_high + 1));
		printf ("driven in bits %s", print_logic_value_base (is_driven, n->node_size.bit_high + 1));
#endif
		printf (" width %d ", n->node_size.bit_high + 1);

#ifdef compiled_runtime
		printf ("\n");
#else
		if (n->active)
			printf ("is active\n");
		else
			printf ("is inactive\n");
#endif
		describe_node_drives (n);
	}
}


void describe_block (symptr s)
{	int i;
	int j;
	bptr b;

	b = s->blockdef;
	printf ("block %s", s->name);
	if (strcmp (s->name, b->name))
		printf (" -> %s ", b->name);
	printf (" is a %s\n", b->def_name);
	print_src_context_stack (b->src_context_stack.next, b->src_context_stack.sc);
	printf ("inputs ");
	for (i = 0; i < b->ninputs; i++)
		printf (" %s = %s%s", b->inputs [i]->name, print_logic_value_base (b->inputs [i]->node_value, b->inputs [i]->node_size.bit_high + 1),
			i == b->ninputs - 1 ? "" : ",");
	printf ("\noutputs ");
	for (i = 0; i < b->noutputs; i++)
	{	printf (" %s = %s", b->outputs [i]->name, print_logic_value_base (b->outputs [i]->node_value, b->outputs [i]->node_size.bit_high + 1));
#ifndef compiled_runtime
		printf (" is %s driven", b->outputactive [i] ? "" : "not");
#endif
		printf ("%s", i == b->noutputs - 1 ? "" : ",");
	}
	printf ("\n");
	for (i = 0; i < b->noutputs; i++)
	{	printf ("outcons %d:\n", i);
		{	for (j = 0; j < b->noutcons [i]; j++)
			{	printf ("  node %s shift %d bits %d..%d\n",
					b->outcons [i] [j].node->name,
					b->outcons [i] [j].conn_sh,
					b->outcons [i] [j].conn_bitrange.bit_low,
					b->outcons [i] [j].conn_bitrange.bit_high);
			}
		}
	}
}


