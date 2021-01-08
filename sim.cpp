/* copyright (c) David M. Lewis 1987 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>


#pragma hdrstop

#include "config.h"
#include "parser.h"
#include "tortle_types.h"
#include "sim.h"
#include "scan_parse.h"
#include "make_net.h"
#include "command.h"
#include "trace.h"
#include "blocktypes.h"
#include "compile_net.h"
#include "nethelp.h"
#include "make_blocks.h"
#include "logic.h"
#include "codegen.h"

#include "debug.h"
#include "utils.h"
#include "compiled_runtime.h"

#include "hash_table.h"

int debug [ndebugs];
FILE *debugfile;
bool have_debugfile = false;

#define state_magic 0x127983fc


bool not_silent = true;	/* enable printing of usual prompts */

double nnodeevents = 0;
double nnodebitevents = 0;
double nblockevals = 0;
double nblockbitevals = 0;

nptr *activenodes;
nptr *activenodesnext;
bptr *activeblocks;
int nactivenodes;
int nblocks, nnodes;
int nbitblocks, nbitnodes;

nptr allnodelist = NULL;
blistptr allblocklist = NULL;

int current_tick = 0;

int net_read = 0;

int plotspice_pid = 0;

int nmultidrivenodes = 0;

symptr block_true, block_false, node_const_input;

void print_io_nodes (void)
{	nptr n;

	for (n = allnodelist; n != NULL; n = n->hnext)
	{	if (n->sub_drivers == NULL)
			printf ("%s: input node\n", n->name);
		if (n->uses == NULL)
			printf ("%s: output node\n", n->name);
	}
}

/* left shift v by lsh bits and insert the bits into dst bits given in range. v has n_logic_value_words words.
 * range must be valid and not have any negative bits
 */

void insert_shifted_bits (logic_value dst, int n_logic_value_words, logic_value v, int lsh, t_bitrange range)
{   int iw;
	int wb_low, wb_high;
	t_bitrange write_br;
	logic_value_word write_mask;
	logic_value_word w;
	int lsh_mod;
	int rsh_mod;
	int insert_w;

	lsh_mod = bit_mod_word (lsh);
	rsh_mod = bits_per_word - lsh_mod;
	/* TODO: strength reduce wb_low and insert_w */
	for (iw = 0; iw < n_logic_value_words; iw++) {
		w = v [iw];
		/* what bit positions to write the word w */
		wb_low = bits_per_word * iw + lsh;

		/* which word in dst to write the low bits */

		insert_w = bit_div_word (wb_low);

		/* this word will be written into lsh_mod of word insert_w to lsh_mod - 1 of insert_w + 1
		 * masked by the range
		 *
		 * so write lsh_mod upwards into insert_w and 0 .. lsh_mod - 1 into insert_w + 1
		 */

		write_br.bit_low = my_max (range.bit_low - insert_w * bits_per_word, lsh_mod);
		write_br.bit_high = range.bit_high - insert_w * bits_per_word;
		write_mask = bitrange_mask (write_br);
		if (write_mask != 0) {
			my_assert (insert_w >= 0, "insert into negative word position");
			dst [insert_w] = (dst [insert_w] & ~ write_mask) | ((w << lsh_mod) & write_mask);
		}

		/* if bits are unaligned across words then need to stuff the high bits into next word */

		if (lsh_mod != 0) {
			insert_w++;
			write_br.bit_low = range.bit_low - insert_w * bits_per_word;
			write_br.bit_high = my_min (range.bit_high - insert_w * bits_per_word, lsh_mod - 1);
			write_mask = bitrange_mask (write_br);
			if (write_mask != 0) {
				my_assert (insert_w >= 0, "insert into negative word position");
				dst [insert_w] = (dst [insert_w] & ~ write_mask) | ((w >> rsh_mod) & write_mask);
			}
		}
	}
}

/* increment the count in a range of bits, and return true iff the word [0] of the count
 * is 1. The actual count may be larger than one, but is tested for with the node->multidriven flag
 */

bool increment_drive_count (logic_value_word **count_words, int n_words, t_bitrange r) {
	bool count_0_is_1;
	int iw;
	t_bitrange incr_range;
	logic_value_word *wp;
	logic_value_word incr_mask;

	count_0_is_1 = false;
	for (iw = 0; iw < n_words; iw++) {
		incr_range.bit_low = r.bit_low - iw * bits_per_word;
		incr_range.bit_high = r.bit_high - iw * bits_per_word;
		incr_mask = bitrange_mask (incr_range);
		wp = &(count_words [iw] [0]);
		if (*wp & incr_mask) {
			count_0_is_1 = true;
		}
		while (incr_mask) {
			*wp ^= incr_mask;			   /* toggle this bit */
			incr_mask &= ~ (*wp);		   /* if the bit became 0, it was a 1, so continue to enable incr_mask */
			wp++;
		}
	}
	return count_0_is_1;
}

/* decrement the count fo each bit in the range of bits. Return true iff at least one
 * count was = 2 before the decrement, and all counts are <= 1 after the decrement.
 */

bool decrement_drive_count (logic_value_word **count_words, int n_words, int n_bitcount, t_bitrange r) {
	bool count_le_1;
	bool count_eq_2;
	int iw;
	int ibit;
	t_bitrange decr_range;
	logic_value_word *wp;
	logic_value_word decr_mask;

	count_le_1 = true;
	count_eq_2 = false;
	for (iw = 0; iw < n_words; iw++) {
		decr_range.bit_low = r.bit_low - iw * bits_per_word;
		decr_range.bit_high = r.bit_high - iw * bits_per_word;
		decr_mask = bitrange_mask (decr_range);
		ibit = 0;
		while (decr_mask) {
			count_words [iw] [ibit] ^= decr_mask;			   /* toggle this bit */
			decr_mask &= count_words [iw] [ibit];									/* if the bit became 1, it was a 0, so continue to enable incr_mask */
			ibit++;
		}
		if (ibit == 2) {
			for (ibit = 1; ibit < n_bitcount && count_words [iw] [ibit] == 0; ibit++)
				;
			if (ibit == n_bitcount) {
				count_eq_2 = true;
			} else {
				count_le_1 = false;
			}
		}
	}
	return (count_eq_2 & count_le_1);
}

void print_node_drive_count (nptr n, const char *s)
{   int i;
	int iw;

	printf ("node %s drive bits %s: ", n->name, s);
	for (i = 0; i < n->nndrivers; i++) {
		printf (" %d: ", i);
		for (iw = n->node_size_words - 1; iw >= 0; iw--) {
			printf ("%08x", n->node_drive_count [iw] [i]);
		}
	}
	printf ("\n");
	fflush (stdout);
}

/* drive a node. deltadrive is +1 to add a drive, -1 to remove one, 0 for
 * change of value. mask specifies those bits that are driven.
 */


#ifdef compiled_runtime

void drive_node_value (nptr n, int n_logic_value_words, logic_value v, int lsh, t_bitrange range, int deltadrive)
{
		insert_shifted_bits (n->node_value_next, n_logic_value_words, v, lsh, range);

}

#else

void drive_node_value (nptr n, int n_logic_value_words, logic_value v, int lsh, t_bitrange range, int deltadrive)
{   blistptr bp;
	bptr b;
	int i;
	t_bitrange r;

#ifdef countbits
	nnodeevents++;
	n->n_events++;
#endif
#ifdef countbitwidths
	nnodebitevents += bitrange_width (n->node_size);
#endif


	if (!n->active)
	{	n->active = true;
		activenodes [nactivenodes++] = n;
#ifdef detail_debugging
		if (debug_traceall)
		{	fprintf (debugfile, "active_node_sim %s\n", n->name);
		}
#endif
	}
	if (deltadrive >= 0)
	{
		insert_shifted_bits (n->node_value_next, n_logic_value_words, v, lsh, range);
		if (deltadrive == 1) {
			if (debug_node_drive_count) {
				print_node_drive_count (n, "before inc");
			}
			if (increment_drive_count (n->node_drive_count, n->node_size_words, range) && !n->multiply_driven)
			{	n->multiply_driven = 1;
				nmultidrivenodes++;
			}
			if (debug_node_drive_count) {
				print_node_drive_count (n, " after inc");
			}
		}
	}
	else /* deltadrive == -1, so recalculate node value */
	{	for (bp = n->sub_drivers; bp != NULL; bp = bp->next)
		{	b = bp->b;
			i = bp->n_io;
			if (bp->b->outputactive [i])
			{   r.bit_low = my_max (bp->low, bp->range.bit_low);
				r.bit_high = my_min (b->output_widths [i].bit_high+ bp->low, bp->range.bit_high);
				insert_shifted_bits (n->node_value_next, n_logic_value_words, b->output_values [i], bp->low, r);
			}
		}
		if (debug_node_drive_count) {
			print_node_drive_count (n, "before dec");
		}
		if (decrement_drive_count (n->node_drive_count, n->node_size_words, n->nndrivers, range)) {
			n->multiply_driven = false;
			nmultidrivenodes--;
		}
		if (debug_node_drive_count) {
			print_node_drive_count (n, " after dec");
		}
	}

#ifdef detail_debugging
	if (debug_traceall)
	{	fprintf (debugfile, "%s <- %s ", n->name, print_logic_value_base (n->node_value_next, n->node_size.bit_high + 1));
		fprintf (debugfile, " @ %d\n", current_tick);
	}
#endif
	if (n->traced)
	{	printf ("%s <- %s ", n->name, print_logic_value_base (n->node_value_next, n->node_size.bit_high + 1));
		printf (" @ %d\n", current_tick);
	}
}

#endif





/* would like user to see change immediately when he sets a node */

void drive_and_set_value_recur (nptr n, int sh, t_bitrange range, int n_logic_value_words, logic_value v, int deltadrive, nptr except_n)
{	snptr snp;
	t_bitrange nr;

	my_assert (range.bit_low >= 0, "drive_and_set_value_recur bit_low < 0");
	drive_node_value (n, n_logic_value_words, v, sh, range, deltadrive);
//	drive_node (n, bdir_lsh (v, sh), bitwidmask (n->node_size.bit_high + 1) & mask, deltadrive);

	nr.bit_low = range.bit_low;
	nr.bit_high = my_min (n->node_size.bit_high, range.bit_high);
	insert_shifted_bits (n->node_value, n_logic_value_words, v, sh, nr);
//	n->value = n->value & ~ mask | bdir_lsh (v, sh) & mask & bitwidmask (n->node_size.bit_high + 1);

	if (n->supernode != (nptr) NULL && n->supernode != except_n)
	{   nr.bit_low = my_max (range.bit_low + n->supernode_range.bit_low, n->supernode_range.bit_low);
		nr.bit_high = my_min (range.bit_high + n->supernode_range.bit_low, n->supernode_range.bit_high);
		drive_and_set_value_recur (n->supernode, sh + n->supernode_range.bit_low, nr, n_logic_value_words, v, deltadrive, n);
//		drive_and_set_recur (n->supernode, sh + n->supernode_range.bit_low,
//			bdir_lsh (mask, n->supernode_range.bit_low) & n->super_mask, v, deltadrive, n);

	}
	for (snp = n->subnodes; snp != (snptr) NULL; snp = snp->next)
	{	if (snp->node != except_n)
		{   nr.bit_low = my_max (my_max (range.bit_low, snp->subrange.bit_low) - snp->subrange.bit_low, 0);
			nr.bit_high = my_min (range.bit_high, snp->subrange.bit_high) - snp->subrange.bit_low;
			drive_and_set_value_recur (snp->node, sh - snp->low, nr, n_logic_value_words, v, deltadrive, n);
//			drive_and_set_recur (snp->node, sh - snp->low,
//				bdir_rsh (mask & snp->mask, snp->low), v, deltadrive, n);
		}
	}
}

void drive_and_set_node_value (obj_ptr s, logic_value v, int deltadrive)
{   nptr n;

	n = s->nodedef;
	drive_and_set_value_recur (n, 0, n->node_size, max_node_words, v, deltadrive, n);
}

void check_output_next (bptr blk, int iout, int delta)
{	int iii;
	int iw;
	bool val_changed;

#ifdef compiled_runtime
	val_changed = true;
#else
	val_changed = false;

	for (iw = 0; iw < blk->output_widths_words [iout]; iw++) {
		if (blk->output_values [iout] [iw] != blk->output_values_next [iout] [iw]) {
			val_changed = true;
			blk->output_values [iout] [iw] = blk->output_values_next [iout] [iw];
		}
	}

#endif

	if (val_changed)
	{	for (iii = 0; iii < blk->noutcons [iout]; iii++) {
//			drive_node (blk->outcons [iout] [iii].node, bdir_lsh (blk->outputvalnext [iout], blk->outcons [iout] [iii].conn_sh),
//					blk->outcons [iout] [iii].conn_mask, delta);
			drive_node_value (blk->outcons [iout] [iii].node, blk->output_widths_words [iout], blk->output_values [iout],
					 blk->outcons [iout] [iii].conn_sh, blk->outcons [iout] [iii].conn_bitrange, delta);
		}
	}
}



/* next value has been loaded into output_values_next. load into values and drive the nodex */

void sched_block_output_next (bptr blk, int iout, int delta)
{	int iconn;
	int iw;

	for (iw = 0; iw < blk->output_widths_words [iout]; iw++)
	{   blk->output_values [iout] [iw] = blk->output_values_next [iout] [iw];
	}

	for (iconn = 0; iconn < blk->noutcons [iout]; iconn++)
	{	drive_node_value (blk->outcons [iout] [iconn].node, blk->output_widths_words [iout], blk->output_values [iout],
			blk->outcons [iout] [iconn].conn_sh, blk->outcons [iout] [iconn].conn_bitrange, delta);
	}

}

void sched_block_output (bptr blk, int iout, logicval nv, int delta)
{	blk->output_values_next [iout] [0] = nv;
	sched_block_output_next (blk, iout, delta);
}


void simulate (unsigned n)
{	int i;
	int j;
	int iw;
	blistptr bp;
	int prev_tick;
	nptr np;
	nptr *npp;
	bptr *bpp;
	bptr b;
	bptr bu;

	prev_tick = current_tick;
	for (i = 0; i < (int) n && (nactivenodes > 0 || make_tracefile) && !pleasestop; i++)
	{	if (make_tracefile && current_tick % trace_sample == 0)
			write_trace ();
		current_tick++;
		bpp = activeblocks;
		activenodes [nactivenodes] = NULL;
		npp = activenodes;
		while ((np = *npp++) != NULL)
		{	/* activenodes [j]->value = activenodes [j]->valuenext; */
			np->active = false;
			for (iw = 0; iw < np->node_size_words; iw++)
			{   np->node_value [iw] = np->node_value_next [iw];
			}

#ifdef detail_debugging
			if (debug_traceall)
			{	fprintf (debugfile, "fanouts of %s\n", np->name);
			}
#endif
			for (bp = np->uses; bp != NULL; bp = bp->next)
			{	bu = bp->b;
				if (!bu->active)
				{	*bpp++ = bu;
#ifdef detail_debugging
					if (debug_traceall)
					{	fprintf (debugfile, "activate %s\n", bp->b->name);
					}
#endif
					bu->active = true;
				}
			}
		}
		*bpp = NULL;
		nactivenodes = 0;
		bpp = activeblocks;
		while ((b = *bpp++) != NULL)
		{	(*(b->fun)) (b);
			b->active = false;
#ifdef countbits
			nblockevals++;
#endif
#ifdef countbitwidths
			nblockbitevals += b->outputs [0]->node_size.bit_high + 1;
#endif
		}
	}
	if (pleasestop)
		printf ("*** simulation interrupted at %d\n", current_tick);
	else
		current_tick = prev_tick + n;
	if (nmultidrivenodes > 0)
	{	printf ("%d multiply driven node drivers at t=%d\n", nmultidrivenodes, current_tick);
	}
}


/* if the supernode is driving output shifted by super_low and the bit_range super_range, what
 * bits intersect the bits shifted by low and in the bit_range range */

t_bitrange super_drive_bitrange (int low, t_bitrange range, int super_low, t_bitrange super_range)
{   t_bitrange r;

	r.bit_low = my_max (my_max (super_range.bit_low - super_low, 0) + low, range.bit_low);
	r.bit_high = my_min (super_range.bit_high - super_low + low, range.bit_high);
	return r;
}
/* if (x<<low)&mask generates stuff for node, what is mask for
 * supernode?
 */


t_bitrange sub_drive_bitrange (int low, t_bitrange range, int sub_low, t_bitrange sub_range)
{   t_bitrange r;

	r.bit_low = my_max (my_max (sub_range.bit_low + low, range.bit_low), 0);
	r.bit_high = my_min (sub_range.bit_high + low, range.bit_high);
	return r;
}

unsigned sub_drive_mask (int low, unsigned mask, int sub_low, unsigned sub_mask)
{
/* for generality, sub_mask is an arg */
	return (bdir_lsh (sub_mask, low) & mask);
}


int n_node_drive_count (nptr np, nptr except_np, int low, t_bitrange range)
{	int n;
	blistptr bp;
	snptr snp;
	t_bitrange br;

	br.bit_low = my_max (range.bit_low, low);
	br.bit_high = my_min (range.bit_high, np->node_size.bit_high + low);
	if (br.bit_high < br.bit_low) {
		return 0;
	}
	for (n = 0, bp = np->drivers; bp != (blistptr) NULL; bp = bp->next)
	{	n++;
		bp->b->noutcons [bp->n_io]++;
	}
	if (np->supernode != (nptr) NULL && np->supernode != except_np)
		n += n_node_drive_count (np->supernode, np, low - np->supernode_range.bit_low,
			super_drive_bitrange (low, br, np->supernode_range.bit_low, np->supernode_range));
	for (snp = np->subnodes; snp != (snptr) NULL; snp = snp->next)
	{	if (snp->node != except_np)
			n += n_node_drive_count (snp->node, np, low + snp->low,
				sub_drive_bitrange (low, br, snp->low, snp->subrange));
	}
	return n;
}


/* search drivers and supernodes of the node np to find any drivers of nd, that when shifted by low and masked with range are non-zero */

void init_node_driver_set (nptr nd, nptr np, nptr except_np, int low, t_bitrange range)
{	unsigned bmask;
	int i;
	int nout;
	snptr snp;
	blistptr bp;
	blistptr nbp;
	t_bitrange br;
	t_bitrange drive_range;

	br.bit_low = my_max (my_max (range.bit_low, low), 0);
	br.bit_high = my_min (range.bit_high, np->node_size.bit_high + low);
//	mask &= bdir_lsh (np->bitmask, low);
	if (debug_subnodes)
	{	printf ("init_node_drivers for node %s subnode %s except %s low %d range %d..%d\n",
			nd->name, np->name, except_np != (nptr) NULL ? except_np->name : "(none)", low, br.bit_low, br.bit_high);
	}
	if (br.bit_high < br.bit_low)
		return;
	for (bp = np->drivers; bp != (blistptr) NULL; bp = bp->next)
	{	nbp = (blistptr) smalloc (sizeof (t_blist));
		nbp->next = nd->sub_drivers;
		nbp->range = range;
		nbp->low = low;
		nbp->b = bp->b;
		nbp->n_io = bp->n_io;
		nd->sub_drivers = nbp;
		nout = bp->n_io;
		if (debug_subnodes)
			printf ("add sub_driver block %s nout %d low %d range %d..%d to node %s\n",
				nbp->b->name, nbp->n_io, nbp->low, nbp->range.bit_low, nbp->range.bit_high, nd->name);
		i = bp->b->noutcons [bp->n_io];
		bp->b->outcons [nout] [i].node = nd;
		bp->b->outcons [nout] [i].conn_sh = low;
		bp->b->outcons [nout] [i].conn_bitrange = range;
		bp->b->noutcons [nout]++;
		if (debug_subnodes)
			printf ("add outcon[%d][%d] node %s low %d range %d..%d to block %s\n",
				nout, i, nd->name, low, range.bit_low, range.bit_high, bp->b->name);
		if (bp->b->outputactive [bp->n_io]) {
			drive_range.bit_low = range.bit_low;
			drive_range.bit_high = my_min (bp->b->output_widths [bp->n_io].bit_high + low, range.bit_high);
//			bmask = bdir_lsh (bitwidmask (bp->b->output_widths [bp->n_io].bit_high + 1), low) & mask;
			increment_drive_count (nd->node_drive_count, nd->node_size_words, drive_range);
#ifdef foo_incr_drive
			if (increment_drive_count (nd->node_drive_count, nd->node_size_words, drive_range) && !nd->multiply_driven)
			{	nd->multiply_driven = 1;
				nmultidrivenodes++;
			}
#endif
		}
	}
	if (np->supernode != (nptr) NULL && np->supernode != except_np)
	{	init_node_driver_set (nd, np->supernode, np, low - np->supernode_range.bit_low,
			super_drive_bitrange (low, br, np->supernode_range.bit_low, np->supernode_range));
	}
	for (snp = np->subnodes; snp != (snptr) NULL; snp = snp->next)
	{	if (snp->node != except_np)
		{   init_node_driver_set (nd, snp->node, np, low + snp->low,
				sub_drive_bitrange (low, br, snp->low, snp->subrange));
		}
	}
}


void init_driver_counts (void)
{	nptr np;
	int i;
	int j;
	int n;
	blistptr bp;
	int iw;
	int ibit;
	bool multi_driven;

	for (np = allnodelist; np != NULL; np = np->hnext)
	{

		n = n_node_drive_count (np, (nptr) NULL, 0, np->node_size);
		/* TODO: This adds an extra word sometimes. Should be ((1 << j) - 1) < n and no post inc of j */
		for (j = 0; (1 << j) < n; j++)
			;
		j++;
		np->nndrivers = j;
		np->node_drive_count = (logic_value_word **) smalloc (np->node_size_words * sizeof (logic_value_word *));
		for (iw = 0; iw < np->node_size_words; iw++) {
			np->node_drive_count [iw] = (logic_value_word *) smalloc (np->nndrivers * sizeof (logic_value_word));
			for (ibit = 0; ibit < np->nndrivers; ibit++) {
				np->node_drive_count [iw] [ibit] = 0;
			}
		}
	}
	for (bp = allblocklist; bp != (blistptr) NULL; bp = bp->next)
	{	for (i = 0; i < bp->b->noutputs; i++)
		{	bp->b->outcons [i] = (connection *) smalloc (bp->b->noutcons [i] * sizeof (connection));
			for (j = 0; j < bp->b->noutcons [i]; j++)
				bp->b->outcons [i] [j].node = (nptr) NULL;		/* just zap it so uninit ref will dump core */
			bp->b->noutcons [i] = 0;
		}
	}
	for (np = allnodelist; np != NULL; np = np->hnext)
	{

		init_node_driver_set (np, np, (nptr) NULL, 0, np->node_size);
		multi_driven = false;
		for (iw = 0; iw < np->node_size_words; iw++)
		{   for (i = 1; i < np->nndrivers && !np->node_drive_count [iw] [i]; i++)
				;
			if (i != np->nndrivers)
			{   multi_driven = true;
			}
		}
		if (multi_driven) {
			np->multiply_driven = 1;
			nmultidrivenodes++;
			fprintf (stderr, "node %s is multiply driven\n", np->name);
		}
	}
}

void optimize_fast_logic ()
{	blistptr blp;
	bptr b;
	int n_fast;
	bool fast_flag;
	int n_fast_repl;
	int i;

	n_fast = 0;
	n_fast_repl = 0;
	for (blp = allblocklist; blp != NULL; blp = blp->next)
	{	fast_flag = true;
		b = blp->b;
		for (i = 0; i < b->ninputs; i++)
		{	if (b->inputs [i]->node_size_words > 1)
			{	fast_flag = false;
			}
		}
		if (b->noutputs != 1)
		{	fast_flag = false;
		}
		if (fast_flag && (b->noutcons [0] != 1 || b->outcons [0] [0].conn_sh != 0 || b->outcons [0] [0].node->node_size_words != 1) )
		{	fast_flag = false;
		}
		if (fast_flag)
		{	n_fast++;
		}
		for (i = 0; fast_flag && fast_block_list [i].slow_fn != NULL; i++)
		{	if (fast_block_list [i].slow_fn == b->fun && (fast_block_list [i].n_inputs == -1 || fast_block_list [i].n_inputs == b->ninputs))
			{	b->fun = fast_block_list [i].fast_fn;
				fast_flag = false;
				n_fast_repl++;
			}
		}
	}
	printf  ("fast possible: %d replaced: %d\n", n_fast, n_fast_repl);
	
}

void make_net (progsynptr prog)
{	progsynptr p;
	symptr s;
	symptr ctemps [2];
//	unsigned iu;
	logic_value_word v;
	t_const_val cv;

	init_compile_stacks ();

	for (p = prog; p != NULL; p = p->next)
	{	if (p->def->kind == def_kind_def || p->def->kind == def_kind_macro)
		{	s = p->def->name;
			if (s->defdef != NULL)
				fprintf (stderr, "duplicate def: %s\n", s->name);
			else
				s->defdef = p->def;
		}
	}

	define_block_types ();

	cv.n_val_bits = 0;
	cv.sign_bit = 0;
	cv.n_logicval_words = 0;
	cv.val_logic = &v;

	block_false = check_temp_node ((t_src_context *) NULL, (symptr) NULL, 1);
	node_const_input = check_temp_node ((t_src_context *) NULL, (symptr) NULL, 1);

//	iu = 0;
	ctemps [0] = node_const_input;
	ctemps [1] = block_false;
//	block_false->blockdef = make_const_fn (block_false->name, ctemps, 2, &iu, 1, 1);
	v = 0;
	block_false->blockdef = make_const_fn (NULL, block_false->name, ctemps, 2, &cv);

	block_true = check_temp_node ((t_src_context *) NULL, (symptr) NULL, 1);
//	iu = 0x1;
	ctemps [0] = node_const_input;
	ctemps [1] = block_true;
	v = 1;
	cv.n_val_bits = 1;
	cv.n_logicval_words = 1;
	block_true->blockdef = make_const_fn (NULL, block_true->name, ctemps, 2, &cv);

	compile_prog (prog, 0);
	if (n_compile_errors_found >= max_compile_errors)
	{	fprintf (stderr, "compilation aborted due to too many errors\n");
	}
	init_driver_counts ();
	if (!debug_disable_fast_blocks)
	{	optimize_fast_logic ();
	}

	printf ("%d nodes %d blocks\n", nnodes, nblocks);
	printf ("%d bitnodes %d bitblocks\n", nbitnodes, nbitblocks);
}

void init_net (char *s)
{	progsynptr p;
	nptr n;

	if (net_read)
	{	printf ("net already read\n");
		return;
	}

	p = scan_parse (s);
	if (p != NULL && !syntax_error_found)
	{	make_net (p);
		net_read = 1;
		activenodes = (nptr *) smalloc ((nnodes + 1) * sizeof (nptr));
		activeblocks = (bptr *) smalloc ((nblocks + 1) * sizeof (bptr));
		nactivenodes = 0;

		for (n = allnodelist; n != NULL; n = n->hnext)
		{	if (!n->active)
			{	n->active = true;
				activenodes [nactivenodes++] = n;
#ifdef detail_debugging
				if (debug_traceall)
				{	fprintf (debugfile, "active_node_init %s\n", n->name);
				}
#endif
			}
		}
	}
}

/* drive_node_value (nptr n, int n_logic_value_words, logic_value v, int lsh, t_bitrange range, int deltadrive) */

void test_drive_node_value (void) {
	t_node n;
	logic_value_word nv [10];
	logic_value_word w [10];
	t_bitrange r;
	int i;

	n.node_value_next = nv;
	n.active = true;
	nv [0] = 0xaaaaaaaa;
	nv [1] = 0xaaaaaaaa;
	nv [2] = 0xaaaaaaaa;

	w [0] = 0xf4f3f2f1;
	w [1] = 0xf8f7f6f5;
	w [2] = 0xfcfbfaf9;
	w [3] = 0xfffefdfc;

	r.bit_low = 13;
	r.bit_high = 47;

	for (i = -64; i < 64; i++) {
		nv [0] = 0xaaaaaaaa;
		nv [1] = 0xaaaaaaaa;
		nv [2] = 0xaaaaaaaa;
		insert_shifted_bits (nv, 3, w, i, r);
		printf ("%2d %08x%08x%08x\n", i, nv [2], nv [1], nv [0]);
	}
	for (i = -64; i < 64; i++) {
		nv [0] = 0xaaaaaaaa;
		nv [1] = 0xaaaaaaaa;
		nv [2] = 0xaaaaaaaa;
		r.bit_low = 13 + i;
		r.bit_high = 47 + i;
		insert_shifted_bits (nv, 3, w, i, r);
		printf ("%2d %08x%08x%08x\n", i, nv [2], nv [1], nv [0]);
	}
	for (i = 0; i < 64; i++) {
		nv [0] = 0x00000000;
		nv [1] = 0x00000000;
		nv [2] = 0x00000000;
		w [0] = 0xffffffff;
		w [1] = 0xffffffff;
		w [2] = 0xffffffff;
		w [3] = 0xffffffff;
		r.bit_low = 13;
		r.bit_high = 12 + i;
		insert_shifted_bits (nv, 3, w, i, r);
		printf ("%2d %08x%08x%08x\n", i, nv [2], nv [1], nv [0]);
	}
}

#ifdef test_hash_table
void test_hash_table_and_smalloc (void) {

	char * c;
	char s[10];
	int ih;
	hash_table<int> t;
	int *ip;

	if (t.lookup_val ("foo") == (int) NULL) {
		printf ("y\n");
	} else {
		printf ("n\n");
	}
	t.add ("foo", 2);
	if (t.lookup_val ("foo") == (int) NULL) {
		printf ("y\n");
	} else {
		printf ("n\n");
	}
	t.lookup_val ("foo") = 3;
	ip = &t.lookup_val ("foo");
	*ip = 4;
	printf ("%d\n", t.lookup ("foo"));
	for (ih = 0; ih < 1000; ih++) {
		sprintf (s, "%d", ih);
		t.add (s, ih);
	}
	c = (char *) smalloc (40);
	smfree (c);
}
#endif

#ifdef compiled_runtime

int main (int argc, char *argv[])
{	int i;

	debugfile = stdout;

	signal (SIGINT, stopfn);
	nodevalfile = stdout;

	init_sym_table ();
	init_print_tables ();

	for (i = 0; i < ndebugs; i++)
		debug [i] = 0;

	nnodes = 0;
	nblocks = 0;

	max_loop_iters = default_max_loop_iters;

	while (argc >= 2 && argv [1] [0] == '-')
	{	switch (argv [1] [1])
		{	case 'd':
				debug [atoi (argv [1] + 2) - 1] = 1;
				break;

			case 's':
				not_silent = false;
				break;

			default:
				fprintf (stderr, "unrecognized flag %s\n", argv [1]);
				break;
		}
		argc--;
		argv++;
	}

	read_descr_and_alloc_net ();
	command_loop ();

	return (0);
}


#else


int main (int argc, char *argv[])
{	int i;
	bool compile_netlist = false;

	debugfile = stdout;

	signal (SIGINT, stopfn);


	nodevalfile = stdout;

	init_sym_table ();
	init_print_tables ();

	for (i = 0; i < ndebugs; i++)
		debug [i] = 0;

	nnodes = 0;
	nblocks = 0;

	max_loop_iters = default_max_loop_iters;

	while (argc >= 2 && argv [1] [0] == '-')
	{	switch (argv [1] [1])
		{	case 'c':
				compile_netlist = true;
				break;

			case 'd':
				debug [atoi (argv [1] + 2) - 1] = 1;
				break;

			case 'I':
				add_include_dir (argv [1] + 2);
				break;

			case 's':
				not_silent = false;
				break;

			case 'l':
				if (sscanf (argv [1] + 2, "%d", &max_loop_iters) != 1)
					fprintf (stderr, "can't parse -l arg\n");
				break;

			default:
				fprintf (stderr, "unrecognized flag %s\n", argv [1]);
				break;
		}
		argc--;
		argv++;
	}

#ifdef test_hash_table
	if (debug_hash_table_smalloc) {
		test_hash_table_and_smalloc ();
		return (0);
	}
#endif


	if (debug_wide_node_drive) {
		test_drive_node_value ();
		return (0);
	}
	if (argc == 2)
		init_net (argv [1]);

	if (compile_netlist)
	{	codegen_net ();
	}
	else
	{	command_loop ();
	}

/*
	if (plotspice_pid > 0)
	{	kill (plotspice_pid, SIGINT);
	}
*/
	return (0);
}

#endif






