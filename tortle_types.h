#ifndef __TORTLE_TYPES_H
#define __TORTLE_TYPES_H


//#include "parser.h"
#include "config.h"

/* copyright (c) David M. Lewis 1987 */


typedef struct symrec *obj_ptr;
typedef struct symrec obj;
typedef struct symrec *symptr;

typedef struct block *bptr;
typedef struct node *nptr;
typedef struct subnode *snptr;
typedef struct blist *blistptr;

typedef struct progsyntax *progsynptr;
typedef struct defsyntax *defsynptr;
typedef struct funsyntax *funsynptr;
typedef struct namelist *namelistptr;
typedef struct exprlist *exprlistptr;
typedef struct numlist *numlistptr;
typedef struct nodesyntax *nodeptr;
typedef struct exprsyntax *exprsynptr;
typedef struct expr0syntax *expr0synptr;
typedef struct expr1syntax *expr1synptr;
typedef struct expr2syntax *expr2synptr;
typedef struct expr3syntax *expr3synptr;
typedef struct expr4syntax *expr4synptr;
typedef struct expr5syntax *expr5synptr;

typedef struct symrec t_symrec;

typedef struct {
	t_symrec *filenamesym;
	int linenum;
	int sc_id;
} t_src_context;

typedef struct s_src_context_stack t_src_context_stack;
typedef struct blist t_blist;

typedef struct s_src_context_stack {
	t_src_context_stack *next;
	t_src_context *sc;
	int scs_id;
} t_src_context_stack;


typedef bptr (*make_block_fn) (t_src_context *sc, const char *name, symptr *nodes, int nnodes, unsigned *nums, int nnums);

typedef unsigned logicval;
typedef unsigned logic_value_word;
typedef logic_value_word *logic_value;


/* a t_const_val stores a variable width signed int. The number of data bits is in n_val_bits.
 * The value is sum (i = 0 .. n_val_bits - 1, val_logic [bit i] * 2 ^ i) - sign_bit * 2 ^ n_val_bits
 * That is, there are a total of n_val_bits in the value and the sign bit is at bit n_val_bits and extends to 
 * the left. So the value 0 is represented with n_val_bits = 0 and sign_bit = 0, while -1 is represented
 * by n_val_bits = 0 and sign_bit = 1. Representations are potentially redundant (eg. n_val_bits = 1 and val_logic = 0)
 * which also represents 0. The canonic form is the smallest value of n_val_bits that represents the value.
 */

typedef struct {
	int n_val_bits;
	unsigned sign_bit;
	int n_logicval_words;
	logic_value_word *val_logic;
} t_const_val;


/* a bitrange represents bits high <= i <= low so a total of high - low + 1 bits */

typedef struct s_bitrange {
	int bit_low;
	int bit_high;
	} t_bitrange;

/* a connection represents a mapping between a boolean value supplied by a
   block to specific bits in a node.
   Note that connections aren't required for inputs, since these are handled
   with sel_ranges. This means that the atomic logic functions can't have
   bidirectional ports, since there is no facility to perform bit
   extraction on inputs, and an IO port might be connected to a subrange.
*/

typedef struct connection_rec {
	nptr node;
	int conn_sh;				/* output value of block << conn_sh, then apply mask */
	t_bitrange conn_bitrange;   /* must only refer to bits that are valid in the node */
	} connection;

/* nptrs are used for inputs or outputs in two state logic because all inputs are guaranteed
   to be aligned to bit 0, using selranges if necessary. Outputs are also aligned to
   bit 0 using nptrs in outputs for non-tristate blocks, and outranges for non-aligned.
   Two-state logic uses state bits to represent the bit connections.
   Three state logic always uses the connections for outputs.
*/
typedef void (*block_sim_fn) (bptr b);
typedef void (*block_codegen_fn) (FILE *f, bptr b);

typedef struct block {
	const char *name;
	t_src_context_stack src_context_stack;
	block_sim_fn fun;
	block_codegen_fn codegen_fn;
	unsigned nstate;
	unsigned *state;
	int ninputs;
	nptr *inputs;
	int *noutcons;
	connection **outcons;
	int noutputs;
	nptr *outputs;
	t_bitrange *output_widths;	  /* size of drive on each output */
	int *output_widths_words;
	logicval *outputval;
	logicval *outputvalnext;
	logic_value *output_values;
	logic_value *output_values_next;
	int block_id;
	int block_fanin_count;
	t_blist *block_fanout_list;
	int state_address;
	int input_node_saved_loc;		/* loc in node_state_saved for any clocked blocks in compiled netlists */
	const char *def_name;				/* what kind of block */
	bool *outputactive;		/* true if the output is being driven */
	bool active;
	bool debugblock;
	bool is_clocked;				/* if true the block is clocked by input 0 and input 1 is synchronous; all others are async */
	bool marked_flag;
	} t_block;


typedef struct node {
	char *name;
	t_src_context_stack src_context_stack;
	nptr hnext;					/* for list of all nodes in circuit */
	logic_value_word **node_drive_count;  /* bit mask count of drivers active in each bit position of the node */
	 						/* this is represented as a vector bitmask where bit j of ndrivers [i] is the bit i of the binary value of the count of
							   number of drivers of bit j. that is the number of drivers of bit j is sum ((ndrivers [i] >> j) & 1) * 2 ^ i */
							/* includes all active drivers of subnodes */
	int nndrivers;			/* number of words in node_drive_count, ceil (log2 (fanin + 1)) */
	unsigned bitmask;
	t_bitrange node_size;	/* bit_low will always be 0 but storing it as a t_bitrange allows operations on it using bitranges */
	int node_size_words;
	logicval value;
	logicval valuenext;
	logic_value node_value;	 /* node value must be 0 in any unused bits */
	logic_value node_value_next;
	int n_events;
	blistptr uses;
	blistptr drivers;
	blistptr sub_drivers;	/* all drivers, including sub and super nodes. includes low and range */
	int multiply_driven;
	nptr supernode;			/* if not NULL, this is a subnode of supernode */
	t_bitrange supernode_range;
	snptr subnodes;
	int node_address;		/* address in node_value array in compiled code */
	int node_id;
	int clock_id;
	bool active;
	bool traced;
	bool save_trace;
	bool marked_flag;
	} t_node;

typedef struct s_nodelist t_nodelist;

typedef struct s_nodelist {
	t_nodelist *next;
	nptr np;
	} t_nodelist;


typedef struct subnode {
	nptr node;
	t_bitrange subrange;
	int low;
	unsigned mask;
	snptr next;
	} t_subnode;


typedef struct blist {
	bptr b;
	blistptr next;
	/* if this is an output list, which output of b drives this node */
	/* if this is a uses list, which input of b is driven */
	int n_io;
	/* range is not valid for t_node.drivers or t_node.uses; only for t_node.sub_drivers */
	int low;
	t_bitrange range;
	} t_blist;

typedef struct smakeblockrec {
	const char *name;
	make_block_fn f;	   /* will use make_general if NULL */
	block_sim_fn logicfn;
	block_codegen_fn codegenfn;
	int need_in_nodes;
	int need_out_nodes;
	} makeblockrec;

typedef struct s_fast_logic_desc {
	block_sim_fn slow_fn;
	block_sim_fn fast_fn;
	int n_inputs;				/* number of inputs the fast function handles. -1 if any number */
} t_fast_logic_desc;


typedef struct symrec {
	char *name;
	nptr nodedef;
	bptr blockdef;
	defsynptr defdef;
	makeblockrec *makeblock;
	t_const_val *const_val;
	int n_array_elems;
	t_symrec **array_elems;
	logicval *tracebits;
	symptr next;
	symptr next_match;				/* to create a list of symrecs that match some pattern */
	int sym_id;
	bool is_filename;
	bool node_save_trace;
	bool file_included;
	} t_symrec;



typedef struct {
	int n_nodes;
	int n_blocks;
	int n_syms;
	int n_total_stuff;
	int n_prev_node_val_words;
	int n_src_context_stack;
	int n_src_context;
	int n_clock_domains;	/* number of domains, including 1 for comb */
	int *clock_domain_n_state_save;
	logic_value_word *stuff;
	nptr *clock_nodes;
	t_blist **block_seed_lists;
	t_blist **domain_sorted_blocks;
	t_node **ordered_nodes;
	t_block **ordered_blocks;
	t_symrec **ordered_syms;
	t_src_context_stack *ordered_src_context_stacks;
	t_src_context *ordered_src_contexts;
	FILE *code_file;
	FILE *descr_file;
	const char *descr_file_name;
	} t_compiled_data;


#define bdir_lsh(x,sh)	((sh) > 0 ? (x) << (sh) : (x) >> (-(sh)))
#define bdir_rsh(x,sh)	((sh) > 0 ? (x) >> (sh) : (x) << (-(sh)))

#endif
