/* copyright (c) David M. Lewis 1987 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "parser.h"
#include "tortle_types.h"
#include "blocktypes.h"
#include "debug.h"
#include "nethelp.h"
#include "scan_parse.h"
#include "utils.h"
#include "make_blocks.h"
#include "sim.h"
#include "compile_net.h"
#include "describe.h"

char nodenameprefix [maxprefixlen];
int prefixlenstack [maxcalldepth];
int stackdepth;
int prefixlen;
t_src_context_stack *src_context_stack;

int lasttempnum = 0;

const char *tempnamehdr = "$";

int nblocktypes;

int n_compile_errors_found = 0;

makeblockrec *make_and;
makeblockrec *make_nand;
makeblockrec *make_expand;
makeblockrec *make_or;
makeblockrec *make_xor;
makeblockrec *make_eqeq;
makeblockrec *make_lsh;
makeblockrec *make_rsh;
makeblockrec *make_adder;
makeblockrec *make_sub;
makeblockrec *make_greater;
makeblockrec *make_less;
makeblockrec *make_leq;
makeblockrec *make_geq;
makeblockrec *make_eq;
makeblockrec *make_neq;
makeblockrec *make_latch;
makeblockrec *make_undefined;

void init_compile_stacks (void)
{	prefixlen = 0;
	src_context_stack = NULL;
	stackdepth = 0;
}


void pushprefix (t_src_context *sc, const char *s, int n)
{	char prefixpart [maxtokenlength + 10];
	char msg [maxprefixlen + maxtokenlength + 100];
	t_src_context_stack *scs;

	if (n == -1)
		sprintf (prefixpart, "%s.", s);
	else
		sprintf (prefixpart, "%s[%d].", s, n);
	if (strlen (prefixpart) + prefixlen >= maxprefixlen - maxtokenlength)
	{	nodenameprefix [prefixlen] = '\0';
		sprintf (msg, "prefix length too long: \"%s%s\"", nodenameprefix, prefixpart);
		print_error_context (sc, msg);
		exit (2);
	}
	strcpy (nodenameprefix + prefixlen, prefixpart);
	if (stackdepth == maxcalldepth)
	{	sprintf (msg, "call stack too deep at %s", prefixpart);
		print_error_context (sc, msg);
		exit (2);
	}
	prefixlenstack [stackdepth] = prefixlen;
	stackdepth++;
	prefixlen += strlen (prefixpart);
	scs = (t_src_context_stack *) smalloc (sizeof (t_src_context_stack));
	scs->next = src_context_stack;
	scs->sc = sc;
	scs->scs_id = -1;
	src_context_stack = scs;
}


void popprefix (void)
{
	stackdepth--;
	prefixlen = prefixlenstack [stackdepth];
	if (src_context_stack != NULL)
		src_context_stack = src_context_stack->next;
}


void print_error_context (t_src_context *sc, const char *s)
{
	print_src_context_stack (src_context_stack, sc);
	if (prefixlen > 0)
	{	nodenameprefix [prefixlen] = '\0';
		fprintf (stderr, "context is block: %s\n", nodenameprefix);
	}
	else
	{	fprintf (stderr, "at top level of design\n");
	}
	fprintf (stderr, "%s\n", s);
	n_compile_errors_found++;
}

void print_error_context_stack (t_src_context_stack *scs, const char *s)
{
	fprintf (stderr, "%s", s);
	print_src_context_stack (scs->next, scs->sc);
}


void print_error (const char *s)
{	print_error_context (NULL, s);
}

/* both blocks and nodes use value and valuenext, outputval and outputvalnext as space
 * for corresponding values in the case that the data fits into 1 word. This lets
 * logic functions that are only 1 word wide directly access this without pointer indirection.
 * Also the bitmask fields will only be valid for data that fits into 1 word.
 */

nptr makenode_sc (t_src_context *sc, char *s, int width)
{	nptr n;
	int iw;

	if (debug_nodes)
		printf ("new node %s width %d\n", s, width);
	n = (nptr) smalloc (sizeof (t_node));
	nnodes++;
	n->active = false;
	n->name = s;
	n->src_context_stack.sc = sc;
	n->src_context_stack.next = src_context_stack;
	n->src_context_stack.scs_id = -1;
	n->traced = false;
	n->save_trace = false;
	n->bitmask = bitwidmask (width);
	n->node_size.bit_high = width - 1;
	n->node_size.bit_low = 0;
	n->node_size_words = words_per_logicval (width);
	nbitnodes += bitrange_width (n->node_size);

	n->value = 0;
	n->valuenext = 0;
	n->n_events = 0;

	if (n->node_size_words > 1) {
		n->node_value = (logic_value) smalloc (sizeof (logic_value_word) * n->node_size_words);
		n->node_value_next = (logic_value) smalloc (sizeof (logic_value_word) * n->node_size_words);
		for (iw = 0; iw < n->node_size_words; iw++) {
			n->node_value [iw] = 0;
			n->node_value_next [iw] = 0;
		}
	} else {
		n->node_value = &(n->value);
		n->node_value_next = &(n->valuenext);
	}

	n->uses = (blistptr) NULL;
	n->drivers = (blistptr) NULL;
	n->sub_drivers = (blistptr) NULL;
	n->multiply_driven = 0;
	n->supernode = (nptr) NULL;
	n->supernode_range.bit_low = 0;
	n->supernode_range.bit_high = 0;
	n->subnodes = (snptr) NULL;
	n->node_address = 0;
	n->nndrivers = 0;
	n->node_id = -1;
	n->clock_id = -1;
	n->marked_flag = false;

	n->hnext = allnodelist;
	allnodelist = n;
	return (n);
}


/* make nn a subnode of n */
void make_subnode_of (t_src_context *sc, nptr n, nptr nn, int sel_low, int sel_high)
{	snptr nsp;
	nptr np;
	char msg [maxnamelen + 100];

	for (np = n; np != NULL; np=np->supernode)
	{	if (np == nn)
		{	sprintf (msg, "creating loop of subnodes as %s is a subnode of %s", n->name, nn->name);
			print_error_context (sc, msg);
			return;
		}
	}
	
	if (nn->supernode != (nptr) NULL)
	{	sprintf (msg, "internal error: multiple supernodes of %s", nn->name);
		print_error_context (sc, msg);
		exit (2);
	}
	if (sel_low > sel_high)
	{	sprintf (msg, "subnode %s of node %s has low > high", n->name, nn->name);
		print_error_context (sc, msg);
		sel_low = sel_high;
	}
	nn->supernode = n;
	nsp = (snptr) smalloc (sizeof (t_subnode));
	nsp->node = nn;
	nsp->low = sel_low;
	nsp->mask = (bitwidmask (sel_high - sel_low + 1)) << sel_low;
	nsp->subrange.bit_low = sel_low;
	nsp->subrange.bit_high = my_min (sel_high, n->node_size.bit_high);
	nsp->next = n->subnodes;
	n->subnodes = nsp;
	nn->supernode_range = nsp->subrange;
}

nptr make_subnode(t_src_context *sc, char *s, nptr n, int sel_low, int sel_high)
{	nptr nn;

	nn = makenode_sc (sc, s, sel_high - sel_low + 1);
	make_subnode_of (sc, n, nn, sel_low, sel_high);
	return (nn);
}


void define_block_types (void)
{	int i;
	symptr n;

	for (i = 0; init_block_types[i].logicfn != NULL; i++)
	{	n = sym_hash (init_block_types[i].name);
		n->makeblock = &(init_block_types [i]);
	}
	nblocktypes = i;
	make_and = sym_hash ("and")->makeblock;
	make_nand = sym_hash ("nand")->makeblock;
	make_expand = sym_hash ("expand")->makeblock;
	make_or = sym_hash ("or")->makeblock;
	make_xor = sym_hash ("xor")->makeblock;
	make_eqeq = sym_hash ("eqeq")->makeblock;
	make_lsh = sym_hash ("lsh")->makeblock;
	make_rsh = sym_hash ("rsh")->makeblock;
	make_adder = sym_hash ("adder")->makeblock;
	make_sub = sym_hash ("sub")->makeblock;
	make_greater = sym_hash ("greater")->makeblock;
	make_less = sym_hash ("less")->makeblock;
	make_leq = sym_hash ("leq")->makeblock;
	make_geq = sym_hash ("geq")->makeblock;
	make_eq = sym_hash ("eq")->makeblock;
	make_neq = sym_hash ("neq")->makeblock;
	make_latch = sym_hash ("latch")->makeblock;
	make_undefined = sym_hash ("**undefined**")->makeblock;
}




bptr new_block (const char *name, block_sim_fn fn, block_codegen_fn cfn, int nstate, int nins, nptr *ins, int nouts, nptr *outs, bool *init_outputactive, const char *type_name)
{	bptr b;
	int i;
	int iw;
	blistptr bp;

	b = (bptr) smalloc (sizeof (t_block));
	b->active = false;
	b->name = name;
	b->src_context_stack.sc = NULL;
	b->src_context_stack.next = NULL;
	b->src_context_stack.scs_id = -1;
	b->fun = fn;
	b->codegen_fn = cfn;
	b->nstate = nstate;
	b->state = (unsigned *) smalloc (nstate * sizeof (unsigned));
	b->state_address = 0;
	b->outputactive = (bool *) smalloc (nouts * sizeof (bool));
	b->output_widths = (t_bitrange *) smalloc (nouts * sizeof (t_bitrange));
	b->output_widths_words = (int *) smalloc (nouts * sizeof (int));
	for (i = 0; i < nstate; i++)
		b->state [i] = 0;
	b->ninputs = nins;
	b->inputs = (nptr *) smalloc (nins * sizeof (nptr));
	for (i = 0; i < nins; i++)
	{	b->inputs [i] = ins [i];
		bp = (blistptr) smalloc (sizeof (t_blist));
		bp->b = b;
		bp->n_io = i;
		bp->next = ins [i]->uses;
		bp->low = 0;
		bp->range.bit_low = 0;
		bp->range.bit_high = 0;
		ins [i]->uses = bp;
	}
	b->noutcons = (int *) smalloc (nouts * sizeof (int));
	b->outcons = (connection **) smalloc (nouts * sizeof (connection *));
	for (i = 0; i < nouts; i++)
	{	b->noutcons [i] = 0;
		b->outcons [i] = (connection *) NULL;
		b->outputactive [i] = init_outputactive [i];
		b->output_widths [i] = outs [i]->node_size;
		b->output_widths_words [i] = outs [i]->node_size_words;
	}
	b->noutputs = nouts;
	b->outputs = (nptr *) smalloc (nouts * sizeof (nptr));
	for (i = 0; i < nouts; i++)
	{	b->outputs [i] = outs [i];
		bp = (blistptr) smalloc (sizeof (t_blist));
		bp->b = b;
		bp->next = outs [i]->drivers;
		bp->n_io = i;
		bp->low = 0;
		bp->range.bit_low = 0;
		bp->range.bit_high = 0;
		outs [i]->drivers = bp;
	}
	b->outputval = (unsigned *) smalloc (nouts * sizeof (unsigned));
	b->outputvalnext = (unsigned *) smalloc (nouts * sizeof (unsigned));
	b->output_values = (logic_value *) smalloc (nouts * sizeof (logic_value));
	b->output_values_next = (logic_value *) smalloc (nouts * sizeof (logic_value));
	b->outputactive = (bool *) smalloc (nouts * sizeof (bool));
	bp = (blistptr) smalloc (sizeof (t_blist));
	bp->b = b;
	bp->n_io = -1;
	bp->next = allblocklist;
	bp->low = 0;
	bp->range.bit_low = 0;
	bp->range.bit_high = 0;
	allblocklist = bp;
	nblocks++;
	for (i = 0; i < nouts; i++)
	{	b->outputactive [i] = init_outputactive [i];
		b->outputval [i] = 0;
		b->outputvalnext [i] = 0;
		if (b->output_widths_words [i] > 1) {
			b->output_values [i] = (logic_value) smalloc (b->output_widths_words [i] * sizeof (logic_value_word));
			b->output_values_next [i] = (logic_value) smalloc (b->output_widths_words [i] * sizeof (logic_value_word));
			for (iw = 0; iw < b->output_widths_words [i]; iw++) {
				b->output_values [i] [iw] = 0;
				b->output_values_next [i] [iw] = 0;
			}
		} else {
			b->output_values [i] = (logic_value) &(b->outputval [i]);
			b->output_values_next [i] = (logic_value) &(b->outputvalnext [i]);
		}
		nbitblocks += bitrange_width (b->outputs [i]->node_size);
	}
	b->block_id = -1;
	b->debugblock = false;
	b->is_clocked = false;
	b->marked_flag = false;
	b->def_name = type_name;
	
	return (b);
}

obj_ptr find_obj (char *s)
{	return (sym_hash_lookup (s));
}

int obj_has_node (obj_ptr o)
{	return (o->nodedef != (nptr) NULL);
}

unsigned obj_node_bitmask (obj_ptr o)
{	return (bitwidmask (o->nodedef->node_size.bit_high + 1));
}

int obj_has_block (obj_ptr o)
{	return (o->blockdef != (bptr) NULL);
}

unsigned *obj_state_addr (obj_ptr o)
{	return (o->blockdef->state);
}

const char *obj_name (obj_ptr o)
{	return (o->name);
}

const char *obj_block_type_name (obj_ptr o)
{	return (o->blockdef->def_name);
}



symptr sym_array_elem_0 (symptr s)
{	while (s->n_array_elems > 0)
	{	s = s->array_elems [0];
	}
	return s;
}


symptr new_temp (void)
{	symptr r;
	char s [10];

	lasttempnum++;
	sprintf (s, "%s%d", tempnamehdr, lasttempnum);
	r = sym_hash (s);
	return (r);
}

symptr check_temp (symptr s)
{	if (s == NULL)
	{	s = new_temp ();
	}
	return (s);
}

symptr check_temp_node (t_src_context *sc, symptr s, int w)
{
	s = check_temp (s);
	if (s->nodedef == NULL)
	{	s->nodedef = makenode_sc (sc, s->name, w);
	}
	return (s);
}


void check_nodes (t_src_context *sc, symptr *sa, int n, int minmax_use_max)	/* 0 for min, 1 for max */
{	int i;
	int w;
//	unsigned w;
//	unsigned m;
	nptr nd;

	if (minmax_use_max)
		w = 0;
	else
		w = max_node_width;
	for (i = 0; i < n; i++)
	{	if (sa [i] != (symptr) NULL && (nd = sa [i]->nodedef) != (nptr) NULL)
		{
//			m = sa [i]->nodedef->bitmask;
			if (minmax_use_max)
			{	if (nd->node_size.bit_high > w)
					w = nd->node_size.bit_high;
			}
//				w |= m;
			else
			{	if (nd->node_size.bit_high < w)
					w = nd->node_size.bit_high;
			}
//				w &= m;
		}
	}
	w++;			/* convert from bit_high to width */
	for (i = 0; i < n; i++)
		sa [i] = check_temp_node (sc, sa [i], w);
}

