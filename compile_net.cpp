/* copyright (c) David M. Lewis 1987 */

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


int max_loop_iters;

symptr compile_call (t_src_context *sc, exprlistptr exprlist, symptr fun, symptr s, const char *name)
{	symptr temps [maxblocknodes];
	defsynptr fundef;
	namelistptr nl;
	exprlistptr e1;
	int i;
	int nparms;
	symptr retparm;
	symptr parm;
	char msg [maxnamelen + 100];

	for (nparms = 0, e1 = exprlist; nparms < maxblocknodes && e1 != NULL; e1 = e1->next, nparms++)
	{	temps [nparms] = compile_expr (e1->expr, (symptr) NULL);
		/* really need idea of uninitialized width */
		temps [nparms] = check_temp_node (sc, temps [nparms], defaultnodewidth);
	}
	if (nparms == maxblocknodes && e1 != NULL)
	{	sprintf (msg, "too many connections to an instance type %s called %s",
			fun == (symptr) NULL ? "???" : fun->name, name);
		print_error_context (sc, msg);
		/* TODO: should this be return NULL? */
		exit (1);
	}
	fundef = fun->defdef;
	if (fundef == (defsynptr) NULL)
	{	sprintf (msg, "no function def for %s", fun->name);
		print_error_context (sc, msg);
		return ((symptr) NULL);
	}
	/* push scope */
	if (name [0] == '$')
		pushprefix (sc, fun->name, fundef->ninstances);
	else
		pushprefix (sc, name, -1);
	fundef->ninstances++;
	/* now bind the formals to the parms */
	for (i = 0, nl = fundef->namelist; nl != NULL && i < nparms; i++, nl = nl->next)
	{	/* define it to be equivalent to the actual parm by copying any node, const, or array definition */
		parm = sym_prefix_hash (nl->name->name);
		parm->nodedef = temps [i]->nodedef;
		if (has_valid_const (temps [i]))
		{	parm->const_val = new_const_val ();
			copy_const_val (parm->const_val, temps [i]->const_val);
		}
		parm->n_array_elems = temps [i]->n_array_elems;
		parm->array_elems = temps [i]->array_elems;
		if (debug_defs)
			printf ("bind %s %s to %s\n", nodenameprefix, nl->name->name, temps [i]->name);
	}
	if (i != nparms || nl != NULL)
	{	nodenameprefix [prefixlen] = '\0';
		sprintf (msg, "call to %s, instance %s: mismatched parms: i = %d, nparms = %d", fun->name, nodenameprefix, i, nparms);
		print_error_context  (sc, msg);
	}
	if (fundef->returns != NULL)
	{	s = check_temp_node (sc, s, defaultnodewidth);
		retparm = sym_prefix_hash_node (sc, fundef->returns->name, s->nodedef, 0);
	}
	compile_prog (fundef->body, 1);
	popprefix ();
	if (fundef->returns)
		return (retparm);
	else
		return (NULL);
}


symptr sym_index_array (symptr s, expr5synptr e)
{	int isub;
	symptr s_sub;
	char msg [maxnamelen + 100];

	for (isub = 0; isub < e->nsubscripts; isub++)
	{	s_sub = compile_expr (e->subscript_expr [isub], NULL);
		if (s_sub->const_val == NULL || const_val_as_int (s_sub->const_val) < 0 ||
				const_val_as_int (s_sub->const_val) >= s->n_array_elems)
		{	sprintf (msg, "subscript out of range in variable %s", s->name);
			print_error_context (e->src_context, msg);
			if (has_valid_const (s_sub))
			{	sprintf (msg, "value is %d", const_val_as_int (s_sub->const_val));
				print_error_context (e->src_context, msg);
			}
		}
		else
		{	s = s->array_elems [const_val_as_int (s_sub->const_val)];
		}
	}
	return s;
	
}

/* all compile_xxx (e, s) will compile the result as a logic netlist with its output in s, if the
 * e is a logic value. If e evaluates to a number then s is irrelevant. In any case,
 * the symptr containing the result is returned by the compile_xxx.
 */

symptr compile_expr5 (expr5synptr e, symptr s)
{	symptr temps [2];
	symptr st;
	symptr s_sub;
	symptr s_const;
	int sel_low, sel_high;
	logic_value_word tv [max_words_per_logicval];
	int iw;
	t_bitrange t_br;
	int isub;
	char msg [maxnamelen + 100];

	if (e->kind == expr5_kind_number)
	{	s = new_temp ();
		s->const_val = new_const_val ();
		copy_const_val (s->const_val, e->const_num);
	}
	else if (s == (symptr) NULL)
	{	if (e->kind == expr5_kind_name)
		{	s = sym_prefix_hash (e->name->name);
			s = sym_index_array (s, e);
		}
		else if (e->kind == expr5_kind_const)
		{	if (e->expr5->kind == expr5_kind_expr_selbit ||
				e->expr5->kind == expr5_kind_expr_selrange)
			{	s_const = compile_expr (e->expr5->sel_low_exp, NULL);
				if (s_const->const_val == NULL)
				{	print_error_context (e->src_context, "selecting bitrange and bit selector low is not a constant");
				} else
				{	sel_low = const_val_as_int (s_const->const_val);
				}
				if (e->expr5->kind == expr5_kind_expr_selbit)
				{	sel_high = sel_low;
				} else
				{	s_const = compile_expr (e->expr5->sel_high_exp, NULL);
					if (s_const->const_val == NULL)
					{	print_error_context (e->src_context, "selecting bitrange and bit selector high is not a constant");
					} else
					/* TODO: check that subrange is valid */
					{	sel_high = const_val_as_int (s_const->const_val);
					}
				}
				s_const = compile_expr5 (e->expr5->expr5, NULL);
				if (has_valid_const (s_const))
				{	s = check_temp_node (e->src_context, s, sel_high - sel_low + 1);
					temps [0] = node_const_input;
					temps [1] = s;
					st = new_temp ();
#ifdef debug_gcc_error
					printf ("const sign %x n_bits %d bits[0] %08x\n", s_const->const_val->sign_bit, s_const->const_val->n_val_bits,
						s_const->const_val->val_logic != NULL ? s_const->const_val->val_logic[0] : 0);
#endif
					st->blockdef = make_const_fn (e->src_context, st->name, temps, 2, s_const->const_val);
				}
				else
				{	print_error_context (e->src_context, "defined a node as constant but the value is not numeric\n");
				}
			}
			else
			{	s_const = compile_expr5 (e->expr5, NULL);
				if (has_valid_const (s_const))
				{	s = check_temp_node (e->src_context, s, max_node_width);
					temps [0] = node_const_input;
					temps [1] = s;
					st = new_temp ();
#ifdef debug_gcc_error
					printf ("const sign %x n_bits %d bits[0] %08x\n", s_const->const_val->sign_bit, s_const->const_val->n_val_bits,
						s_const->const_val->val_logic != NULL ? s_const->const_val->val_logic[0] : 0);
#endif
					st->blockdef = make_const_fn (e->src_context, st->name, temps, 2, s_const->const_val);
				}
				else
				{	print_error_context (e->src_context, "defined a node as constant but the value is not numeric\n");
				}
			}
		}
		else if (e->kind == expr5_kind_expr)
		{	s = compile_expr (e->expr, NULL);
		}
		else if (e->kind == expr5_kind_expr_selbit || e->kind == expr5_kind_expr_selrange)
		{	/* TODO: check if e->expr is a const then optimize width */
			s = compile_expr5 (e->expr5, NULL);
			s_sub = new_temp ();
			s_const = compile_expr (e->sel_low_exp, NULL);
			if (s_const->const_val == NULL)
			{	print_error_context (e->src_context, "selecting bitrange and bit selector low is not a constant");
			} else
			{	sel_low = const_val_as_int (s_const->const_val);
			}
			if (e->kind == expr5_kind_expr_selbit)
			{	sel_high = sel_low;
			} else
			{	s_const = compile_expr (e->sel_high_exp, NULL);
				if (s_const->const_val == NULL)
				{	print_error_context (e->src_context, "selecting bitrange and bit selector high is not a constant");
				} else
				/* TODO: check that subrange is valid */
				{	sel_high = const_val_as_int (s_const->const_val);
				}
			}
			if (s->nodedef != NULL)
			{	s_sub->nodedef = make_subnode (e->src_context, s_sub->name, s->nodedef, sel_low, sel_high);
				s = s_sub;
			}
			else if (has_valid_const (s))
			{	for (iw = 0; iw < max_words_per_logicval; iw++)
				{	tv [iw] = 0;
				}
				t_br.bit_low = 0;
				t_br.bit_high = sel_high - sel_low;
				insert_shifted_bits (tv, s->const_val->n_logicval_words, s->const_val->val_logic,
							-sel_low, t_br);
				s_sub->const_val = new_const_val_canonic (tv, s->const_val->n_logicval_words);
				s = s_sub;
			}
			else
			{	print_error_context (e->src_context, "using a bit range selector and the value is not a node or a constant");
			}
		}
		else
		{	exit_with_err ("invalid kind in expr5");
		}
	}
	else if (s->nodedef == NULL)
	{	sprintf (msg, "undefined node %s", s->name);
		print_error_context (e->src_context, msg);
		s_sub = sym_prefix_hash_node (e->src_context, e->name->name, (nptr) NULL, 1);
		s->nodedef = s_sub->nodedef;
	}
	else	/* compile something to end up in a given node */
	{	switch (e->kind)
		{	case expr5_kind_name:					/* of form a = b */
				temps [0] = sym_prefix_hash (e->name->name);
				temps [0] = sym_index_array (temps [0], e);
				temps [0] = sym_check_node (e->src_context, temps [0], (nptr) NULL, 0);
				temps [1] = s;
				st = new_temp ();
				st->blockdef = make_fn (e->src_context, make_and, st->name, temps, 2, (unsigned *) NULL, 0);
				break;

			case expr5_kind_expr:		/* expr is no problem */
				s = compile_expr (e->expr, s);
				break;

			/* TODO: check that subrange is valid */
			case expr5_kind_expr_selbit:
			case expr5_kind_expr_selrange:
				s_sub = compile_expr5 (e->expr5, (symptr) NULL);
				s_const = compile_expr (e->sel_low_exp, NULL);
				if (s_const->const_val == NULL)
				{	print_error_context (e->src_context, "selecting a bit range but bit selector low is not a constant");
				} else
				{	sel_low = const_val_as_int (s_const->const_val);
				}
				if (e->kind == expr5_kind_expr_selbit)
				{	sel_high = sel_low;
				} else
				{	s_const = compile_expr (e->sel_high_exp, NULL);
					if (s_const->const_val == NULL)
					{	print_error_context (e->src_context, "selecting a bit range but bit selector high is not a constant");
					} else
					{	sel_high = const_val_as_int (s_const->const_val);
					}
				}
				if (s->nodedef->supernode == (nptr) NULL)
				{	make_subnode_of (e->src_context, s_sub->nodedef, s->nodedef, sel_low, sel_high);
				}
				else
				{	/* TODO: check ssub is a valid node? */
					st = new_temp ();
					st->nodedef = make_subnode (e->src_context, st->name, s_sub->nodedef, sel_low, sel_high);
					temps [0] = st;
					temps [1] = s;
					st->blockdef = make_fn (e->src_context, make_and, st->name, temps, 2, (unsigned *) NULL, 0);
				}
				break;

			case expr5_kind_const:
				temps [0] = node_const_input;
				temps [1] = s;
				st = new_temp ();
				s_const = compile_expr5 (e->expr5, NULL);
				st->blockdef = make_const_fn (e->src_context, st->name, temps, 2, s_const->const_val);
				break;

			}
	}
	return (s);
}

symptr compile_expr4 (expr4synptr e, symptr s)
{	symptr temps [maxblocknodes];
	symptr st;
	int w;
	logic_value_word lprod;
	int prod;

	switch (e->kind)
	{	case expr4_kind_expr5:
			s = compile_expr5 (e->expr5, s);
			break;

		case expr4_kind_not:
			temps [0] = compile_expr4 (e->expr4, (symptr) NULL);
			if (has_valid_const (temps [0]))
			{	s = new_temp ();
				s->const_val = boolop_const_vals (temps [0]->const_val, temps [0]->const_val, 0x1);
			}
			else
			{	temps [1] = s;
				check_nodes (e->src_context, temps, 2, 0);
				st = new_temp ();
				st->blockdef = make_fn (e->src_context, make_nand, st->name, temps, 2, (unsigned *) NULL, 0);
				s = temps [1];
			}
			break;

		case expr4_kind_mod:
		case expr4_kind_div:
			temps [0] = compile_expr4 (e->expr4, (symptr) NULL);
			temps [1] = compile_expr5 (e->expr5, (symptr) NULL);
			if (!has_valid_int_const (temps [0]) || !has_valid_int_const (temps [1]))
			{	print_error_context (e->src_context, "arguments to mod or div must be constants");
				s = NULL;
			}
			else
			{	s = new_temp ();
				if (e->kind == expr4_kind_div)
					prod = const_val_as_int (temps [1]->const_val) / const_val_as_int (temps [0]->const_val);
				else
					prod = const_val_as_int (temps [1]->const_val) % const_val_as_int (temps [0]->const_val);

				lprod = (logic_value_word) prod;
				s->const_val = new_const_val_canonic (&lprod, 1);
			}
			break;


		case expr4_kind_star:
			temps [0] = compile_expr4 (e->expr4, (symptr) NULL);
			temps [1] = compile_expr5 (e->expr5, (symptr) NULL);
			if (temps [1]->const_val == NULL)
			{	print_error_context (e->src_context, "expander width must be a constant");
				w = 1;
			} else
			{	w = const_val_as_int (temps [1]->const_val);
			}
			if (s == NULL && has_valid_const (temps [0]))
			{	s = new_temp ();
				prod = const_val_as_int (temps [0]->const_val) * const_val_as_int (temps [1]->const_val);
				lprod = (logic_value_word) prod;
				s->const_val = new_const_val_canonic (&lprod, 1);
			}
			else
			{	if (w < 0 || w > max_node_width)
				{	print_error_context (e->src_context, "invalid width in expander, is either negative or larger than max node width");
					w = 1;
				}
				s = check_temp_node (e->src_context, s, w);
				temps [1] = s;
				st = new_temp ();
				st->blockdef = make_fn (e->src_context, make_expand, st->name, temps, 2, (unsigned *) NULL, 0);
			}
			break;

		case expr4_kind_call:
			s = compile_call (e->src_context, e->exprlist, e->expr5->name, s, "$");
			break;

	}
	return (s);
}

symptr compile_expr3 (expr3synptr e, symptr s)
{	symptr temps [maxblocknodes];
	symptr st;
	int i;
	int j;
	bool ops_are_consts;

	switch (e->kind)
	{	case expr3_kind_and:
			ops_are_consts = true;
			for (i = 0; i < maxblocknodes - 2 && e->kind == expr3_kind_and; i++)
			{	temps [i] = compile_expr4 (e->expr4, (symptr) NULL);
				ops_are_consts &= has_valid_const (temps [i]);
				e = e->expr3;
			}
			temps [i] = compile_expr3 (e, (symptr)NULL);
			ops_are_consts &= has_valid_const (temps [i]);
			i++;
			if (ops_are_consts)
			{	s = new_temp ();
				s->const_val = boolop_const_vals (temps [0]->const_val, temps [1]->const_val, 0x8);
				for (j = 2; j < i; j++)
				{	s->const_val = boolop_const_vals (s->const_val, temps [j]->const_val, 0x8);
				}
			}
			else
			{	temps [i] = s;
				check_nodes (e->src_context, temps, i + 1, 0);
				st = new_temp ();
				st->blockdef = make_fn (e->src_context, make_and, st->name, temps, i + 1, (unsigned *) NULL, 0);
				s = temps [i];
			}
			break;

		case expr3_kind_expr4:
			s = compile_expr4 (e->expr4, s);
			break;
	}
	return (s);
}

symptr compile_expr2 (expr2synptr e, symptr s)
{	symptr temps [maxblocknodes];
	symptr st;
	int i;
	int j;
	int w;
	int iidx;
	int idx_start;
	int idx_end;
	int idx_delta;
	symptr s_concat_array;
	symptr s_idx_start;
	symptr s_idx_end;
	int this_ekind;
	bool ops_are_consts;
	unsigned const_operation;
	char msg [maxnamelen + 100];

	switch (e->kind)
	{	case expr2_kind_expr3:
			s = compile_expr3 (e->expr3, s);
			break;

		case expr2_kind_or:
		case expr2_kind_xor:
		case expr2_kind_eqeq:
			this_ekind = e->kind;
			ops_are_consts = true;
			for (i = 0; i < maxblocknodes - 2 && e->kind == this_ekind; i++)
			{	temps [i] = compile_expr3 (e->expr3, (symptr) NULL);
				ops_are_consts &= has_valid_const (temps [i]);
				e = e->expr2;
			}
			temps [i] = compile_expr2 (e, (symptr) NULL);
			ops_are_consts &= has_valid_const (temps [i]);
			i++;
			if (ops_are_consts)
			{	s = new_temp ();
				if (this_ekind == expr2_kind_or)
				{	const_operation = 0xe;
				}
				else if (this_ekind == expr2_kind_xor)
				{	const_operation = 0x6;
				}
				else if (this_ekind == expr2_kind_eqeq)
				{	const_operation = 0x9;
				}
				s->const_val = boolop_const_vals (temps [0]->const_val, temps [1]->const_val, const_operation);
				for (j = 2; j < i; j++)
				{	s->const_val = boolop_const_vals (s->const_val, temps [j]->const_val, const_operation);
				}
			}
			else
			{	temps [i] = s;
				check_nodes (e->src_context, temps, i + 1, 1);
				st = new_temp ();
				if (this_ekind == expr2_kind_or)
				{	st->blockdef = make_fn (e->src_context, make_or, st->name, temps, i + 1, (unsigned *) NULL, 0);
				}
				else if (this_ekind == expr2_kind_xor)
				{	st->blockdef = make_fn (e->src_context, make_xor, st->name, temps, i + 1, (unsigned *) NULL, 0);
				}
				else if (this_ekind == expr2_kind_eqeq)
				{	st->blockdef = make_fn (e->src_context, make_eqeq, st->name, temps, i + 1, (unsigned *) NULL, 0);
				}
				s = temps [i];
			}
			break;


		case expr2_kind_lsh:
			temps [0] = check_temp_node (e->src_context, compile_expr3 (e->expr3, (symptr) NULL), defaultnodewidth);
			temps [1] = check_temp_node (e->src_context, compile_expr2 (e->expr2, (symptr) NULL), defaultnodewidth);
			temps [2] = check_temp_node (e->src_context, s, bitrange_width (temps[0]->nodedef->node_size));
			st = new_temp ();
			st->blockdef = make_fn (e->src_context, make_lsh, st->name, temps, 3, (unsigned *) NULL, 0);
			s = temps [2];
			break;

		case expr2_kind_rsh:
			temps [0] = check_temp_node (e->src_context, compile_expr3 (e->expr3, (symptr) NULL), defaultnodewidth);
			temps [1] = check_temp_node (e->src_context, compile_expr2 (e->expr2, (symptr) NULL), defaultnodewidth);
			temps [2] = check_temp_node (e->src_context, s, bitrange_width (temps[0]->nodedef->node_size));
			st = new_temp ();
			st->blockdef = make_fn (e->src_context, make_rsh, st->name, temps, 3, (unsigned *) NULL, 0);
			s = temps [2];
			break;

		case expr2_kind_concat:
			for (w = 0, i = 0; i < maxblocknodes - 2 && e->kind == expr2_kind_concat; i++)
			{	temps [i] = check_temp_node (e->src_context, compile_expr3 (e->expr3, (symptr) NULL), 0);
				e = e->expr2;
				w += bitrange_width (temps [i]->nodedef->node_size);
			}
			temps [i] = compile_expr2 (e, (symptr) NULL);
			w += bitrange_width (temps [i]->nodedef->node_size);
			i++;
			s = check_temp_node (e->src_context, s, w);
			temps [i] = s;
			check_nodes (e->src_context, temps, i + 1, 0);
			st = new_temp ();
			if (w > max_node_width)
			{	sprintf (msg, "output of concat %s too wide", st->name);
				print_error_context (e->src_context, msg);
			}
			st->blockdef = make_concat_fn (e->src_context, st->name, temps, i + 1, (unsigned *) NULL, 0);
			s = temps [i];
			break;

		case expr2_kind_concat_range:
			s_concat_array = compile_expr3 (e->expr3, NULL);
			if (s_concat_array->n_array_elems == 0)
			{	print_error_context (e->src_context, "concat array but base is not an array");
			}
			else
			{	s_idx_start = compile_expr (e->sub_start, NULL);
				s_idx_end = compile_expr (e->sub_end, NULL);
				if (s_idx_start->const_val == NULL || !const_val_is_int (s_idx_start->const_val) ||
							s_idx_end->const_val == NULL || !const_val_is_int (s_idx_start->const_val))
				{	print_error_context (e->src_context, "concat array range bound is not a valid integer");
				}
				else
				{	idx_start = const_val_as_int (s_idx_start->const_val);
					idx_end = const_val_as_int (s_idx_end->const_val);
					if (idx_start < 0 || idx_start >= s_concat_array->n_array_elems ||
							idx_end < 0 || idx_end >= s_concat_array->n_array_elems)
					{	print_error_context (e->src_context, "array bounds out of range in concat array");
					}
					else
					{	if (idx_end >= idx_start)
							idx_delta = 1;
						else
							idx_delta = -1;
						w = 0;
						for (i = 0, iidx = idx_start; (iidx - idx_end) * idx_delta <= 0; iidx += idx_delta, i++)
						{	temps [i] = s_concat_array->array_elems [iidx];
							w += bitrange_width (temps [i]->nodedef->node_size);

						}
						s = check_temp_node (e->src_context, s, w);
						temps [i] = s;
						check_nodes (e->src_context, temps, i + 1, 0);
						st = new_temp ();
						if (w > max_node_width)
						{	sprintf (msg, "output of concat %s too wide", st->name);
							print_error_context (e->src_context, msg);
						}
						st->blockdef = make_concat_fn (e->src_context, st->name, temps, i + 1, (unsigned *) NULL, 0);
						s = temps [i];
					}
				}
			}
			break;
	}
	return (s);
}

symptr compile_expr1 (expr1synptr e, symptr s)
{	symptr temps [maxblocknodes];
	symptr st;

	if (e->kind == expr1_kind_expr2)
		s = compile_expr2 (e->expr2, s);
	else	/* plus or minus */
	{	st = new_temp ();
		temps [0] = compile_expr1 (e->expr1, (symptr) NULL);
		temps [1] = compile_expr2 (e->expr2, (symptr) NULL);
		if (has_valid_const (temps [0]) && has_valid_const (temps [1]))
		{	s = new_temp ();
			if (e->kind == expr1_kind_plus)
			{	s->const_val = addsub_const_vals (temps [0]->const_val, temps [1]->const_val, false);
			}
			else
			{	s->const_val = addsub_const_vals (temps [0]->const_val, temps [1]->const_val, true);
			}
		}
		else
		{	check_nodes (e->src_context, temps, 2, 1);
			s = check_temp_node (e->src_context, s, my_max (temps [0]->nodedef->node_size.bit_high, temps [1]->nodedef->node_size.bit_high));
			temps [3] = s;
			if (e->kind == expr1_kind_plus)
			{	temps [2] = block_false;
				st->blockdef = make_fn (e->src_context, make_adder, st->name, temps, 4, (unsigned *) NULL, 0);
			}
			else
			{	temps [2] = block_true;
				st->blockdef = make_fn (e->src_context, make_sub, st->name, temps, 4, (unsigned *) NULL, 0);
			}
		}
	}
	return (s);
}

symptr compile_expr0 (expr0synptr e, symptr s)
{	symptr temps [maxblocknodes];
	symptr st;
	t_const_val *r;
	t_const_val *cmp;
	logic_value_word w;
	int icmp;


	if (e->kind == expr0_kind_expr1)
	{	s = compile_expr1 (e->expr1a, s);
		return (s);
	}
	temps [0] = compile_expr1 (e->expr1a, (symptr) NULL);
	temps [1] = compile_expr1 (e->expr1b, (symptr) NULL);
	if (has_valid_const (temps [0])&& has_valid_const (temps [1]))
	{	r = addsub_const_vals (temps [0]->const_val, temps [1]->const_val, true);
		switch (e->kind)
		{	case expr0_kind_greater:
				icmp = r->sign_bit == 0 && r->n_val_bits > 0;
				break;
			case expr0_kind_less:
				icmp = r->sign_bit == 1;
				break;
			case expr0_kind_leq:
				icmp = r->sign_bit == 1 || r->n_val_bits == 0;
				break;
			case expr0_kind_geq:
				icmp = r->sign_bit == 0;
				break;
			case expr0_kind_equal:
				icmp = r->sign_bit == 0 && r->n_val_bits == 0;
				break;
			case expr0_kind_neq:
				icmp = r->sign_bit != 0 || r->n_val_bits > 0;
				break;
		}
		w = icmp;
		st = new_temp ();
		st->const_val = new_const_val_canonic (&w, 1);
		return (st);
	}
	else
	{
		temps [2] = s;
		check_nodes (e->src_context, temps, 3, 1);
		st = new_temp ();
		switch (e->kind)
		{	case expr0_kind_greater:
				st->blockdef = make_fn (e->src_context, make_greater, st->name, temps, 3, (unsigned *) NULL, 0);
				break;
			case expr0_kind_less:
				st->blockdef = make_fn (e->src_context, make_less, st->name, temps, 3, (unsigned *) NULL, 0);
				break;
			case expr0_kind_leq:
				st->blockdef = make_fn (e->src_context, make_leq, st->name, temps, 3, (unsigned *) NULL, 0);
				break;
			case expr0_kind_geq:
				st->blockdef = make_fn (e->src_context, make_geq, st->name, temps, 3, (unsigned *) NULL, 0);
				break;
			case expr0_kind_equal:
				st->blockdef = make_fn (e->src_context, make_eq, st->name, temps, 3, (unsigned *) NULL, 0);
				break;
			case expr0_kind_neq:
				st->blockdef = make_fn (e->src_context, make_neq, st->name, temps, 3, (unsigned *) NULL, 0);
				break;
		}
		return (temps [2]);
	}
}

symptr compile_expr (exprsynptr e, symptr s)
{	symptr s1;
	symptr s0;
	symptr st;
	symptr temps [4];

	if (e->kind == expr_kind_expr0)
		s = compile_expr0 (e->expr0, s);
	else
	{	s0 = compile_expr0 (e->expr0, (symptr) NULL);
		s1 = compile_expr (e->expr, (symptr) NULL);
		switch (e->kind)
		{	case expr_kind_clocked:
			case expr_kind_latched:
				temps [0] = s0;
				temps [1] = s1;
				temps [2] = s;
				check_nodes (e->src_context, temps, 3, 1);
				st = new_temp ();
				if (e->kind == expr_kind_clocked)
					st->blockdef = make_reg_fn (e->src_context, st->name, temps, 3, (unsigned *) NULL, 0);
				else
					st->blockdef = make_fn (e->src_context, make_latch, st->name, temps, 3, (unsigned *) NULL, 0);
				s = temps [2];
				break;

			case expr_kind_3state:
				temps [0] = s1;
				temps [1] = s0;
				temps [2] = s;
				check_nodes (e->src_context, temps, 3, 1);
				st = new_temp ();
				st->blockdef = make_buf3s_fn (e->src_context, st->name, temps, 3, (unsigned *) NULL, 0);
				s = temps [2];
				break;

			case expr_kind_cond:
				temps [0] = s0;
				temps [1] = compile_expr (e->exprelse, (symptr) NULL);
				temps [2] = s1;
				if (has_valid_const (temps [0])&& has_valid_const (temps [1]) && has_valid_const (temps [2]))
				{	if (temps [0]->const_val->sign_bit != 0 || temps [0]->const_val->n_val_bits != 0)
					{	s = temps [2];
					}
					else
					{	s = temps [1];
					}
				}
				else
				{	temps [3] = s;
					check_nodes (e->src_context, temps, 1, 1);
					check_nodes (e->src_context, temps + 1, 3, 1);
					st = new_temp ();
					st->blockdef = make_mux_fn (e->src_context, st->name, temps, 4, (unsigned *) NULL, 0);
					s = temps [3];
				}
		}
	}
	return (s);
}

void def_eqn (defsynptr d)
{	symptr lhs;
	symptr temps [3];
	symptr st;
	char msg [maxnamelen + 100];


	lhs = compile_expr5 (d->lhs, (symptr) NULL);
	if (lhs->nodedef == NULL && lhs->const_val == NULL)
	{	sprintf (msg, "no node or variable named %s", lhs->name);
		print_error_context (d->src_context, msg);
		return;
	}
	if (has_valid_const (lhs))
	{	if (	d->clk_expr != (expr2synptr) NULL ||
				d->latch_expr != (expr2synptr) NULL ||
				d->buf_expr != (expr2synptr) NULL)
		{	sprintf (msg, "assignment to variable %s has a clock qualifier and is ignored", lhs->name);
			print_error_context (d->src_context, msg);
		}
		st = compile_expr (d->expr, NULL);
		if (st->const_val == NULL)
		{	sprintf (msg, "assignment to variable %s with RHS is not a variable", lhs->name);
			print_error_context (d->src_context, msg);
		}
		else
		{	copy_const_val (lhs->const_val, st->const_val);
		}
	}
	else
	{	if (	d->clk_expr == (expr2synptr) NULL &&
				d->latch_expr == (expr2synptr) NULL &&
				d->buf_expr == (expr2synptr) NULL)
			(void) compile_expr (d->expr, lhs);
		else
		{	temps [0] = compile_expr (d->expr, (symptr) NULL);
			if (d->latch_expr != (expr2synptr) NULL)
			{	temps [1] = temps [0];
				temps [0] = compile_expr2 (d->latch_expr, (symptr) NULL);
				if (d->buf_expr == (expr2synptr) NULL)
					temps [2] = lhs;
				else
					temps [2] = check_temp_node (d->src_context, (symptr) NULL, bitrange_width (temps [1]->nodedef->node_size));
				check_nodes (d->src_context, temps, 3, 1);
				st = new_temp ();
				st->blockdef = make_fn (d->src_context, make_latch, st->name, temps, 3, (unsigned *) NULL, 0);
				temps [0] = temps [2];
			}
			else if (d->clk_expr != (expr2synptr) NULL)
			{	temps [1] = temps [0];
				temps [0] = compile_expr2 (d->clk_expr, (symptr) NULL);
				if (d->buf_expr == (expr2synptr) NULL)
					temps [2] = lhs;
				else
					temps [2] = check_temp_node (d->src_context, (symptr) NULL, bitrange_width (temps [1]->nodedef->node_size));
				check_nodes (d->src_context, temps, 3, 1);
				st = new_temp ();
				st->blockdef = make_reg_fn (d->src_context, st->name, temps, 3, (unsigned *) NULL, 0);
				temps [0] = temps [2];
			}
			if (d->buf_expr != (expr2synptr) NULL)
			{	temps [1] = compile_expr2 (d->buf_expr, (symptr) NULL);
				temps [2] = lhs;
				st = new_temp ();
				st->blockdef = make_buf3s_fn (d->src_context, st->name, temps, 3, (unsigned *) NULL, 0);
			}
		}
	}
}

void def_assign (defsynptr d)
{	symptr lhs;
	symptr temp;
	nptr temp_node;
	char *ct;
	char msg [maxnamelen + 100];


	lhs = compile_expr5 (d->lhs, (symptr) NULL);
	temp = new_temp ();
	temp->nodedef = makenode_sc (d->src_context, temp->name, lhs->nodedef->node_size.bit_high + 1);
	(void) compile_expr (d->expr, temp);
	ct = temp->nodedef->name;
	temp->nodedef->name = lhs->nodedef->name;
	lhs->nodedef->name = ct;
	temp_node = temp->nodedef;
	temp->nodedef = lhs->nodedef;
	lhs->nodedef = temp_node;
}


void eval_subscripts (t_namelist *nl, int *subs)
{	int isub;
	symptr s;
	char msg [maxnamelen + 100];

	for (isub = 0; isub < nl->n_dims; isub++)
	{	s = compile_expr (nl->subscripts [isub], NULL);
		if (s->const_val == NULL)
		{	sprintf (msg, "subscript range %s is not a constant", s->name);
			print_error_context (nl->src_context, msg);
			subs [isub] = 1;
		}
		else
		{	subs [isub] = const_val_as_int (s->const_val);
			if (subs [isub] < 1 || subs [isub] > max_array_size)
			{	sprintf (msg, "subscript range %s is not valid", s->name);
				print_error_context (nl->src_context, msg);
				subs [isub] = 1;
			}
		}
	}
}

symptr def_node_array_recur (t_src_context *sc, char *name, int width, int iindex, int *indexes, int nindex, int *nelems)
{	int iidx;
	symptr s;
	char msg [maxnamelen + 100];

	s = sym_prefix_hash_indexed (name, iindex, indexes);
	if (iindex == nindex)
	{	if (s->nodedef != NULL)
		{	sprintf (msg, "duplicate def for node %s", s->name);
			print_error_context (sc, msg);
		}
		else
		{	s->nodedef = makenode_sc (sc, s->name, width);
		}
	}
	else
	{	s->n_array_elems = nelems [iindex];
		s->array_elems = (symptr *) smalloc (nelems [iindex] * sizeof (symptr));
		for (iidx = 0; iidx < nelems [iindex]; iidx++)
		{	indexes [iindex] = iidx;
			s->array_elems [iidx] = def_node_array_recur (sc, name, width, iindex + 1, indexes, nindex, nelems);
		}
	}
	return s;
}

void def_node_array (defsynptr d)
{	namelistptr nl;
	symptr s;
	symptr sym_width;
	int ranges [max_subscripts];
	int subs [max_subscripts];
	char msg [maxnamelen + 100];

	sym_width = compile_expr (d->width_expr, NULL);
	if (sym_width->const_val == NULL || const_val_as_int (sym_width->const_val) < 0 ||
			const_val_as_int (sym_width->const_val) > max_node_width)
	{	sprintf (msg, "illegal or missing width in declaration of node %s", d->name->name);
		print_error_context (d->src_context, msg);
		return;
	}

	for (nl = d->namelist; nl != NULL; nl = nl->next)
	{	eval_subscripts (nl, ranges);
		(void) def_node_array_recur (d->src_context, nl->name->name, const_val_as_int (sym_width->const_val), 0, subs, nl->n_dims, ranges);
	}
}

symptr def_logic_var_array_recur (char *name, int iindex, int *indexes, int nindex, int *nelems)
{	int iidx;
	symptr s;

	s = sym_prefix_hash_indexed (name, iindex, indexes);
	if (iindex == nindex)
	{	s->const_val = new_const_val ();
	}
	else
	{	s->n_array_elems = nelems [iindex];
		s->array_elems = (symptr *) smalloc (nelems [iindex] * sizeof (symptr));
		for (iidx = 0; iidx < nelems [iindex]; iidx++)
		{	indexes [iindex] = iidx;
			s->array_elems [iidx] = def_logic_var_array_recur (name, iindex + 1, indexes, nindex, nelems);
		}
	}
	return s;
}

void def_logicvar_array (defsynptr d)
{	namelistptr nl;
	symptr s;
	int ranges [max_subscripts];
	int subs [max_subscripts];

	for (nl = d->namelist; nl != NULL; nl = nl->next)
	{	eval_subscripts (nl, ranges);
		(void) def_logic_var_array_recur (nl->name->name, 0, subs, nl->n_dims, ranges);
	}
}

void def_block (defsynptr d)
{	symptr block_type;
	symptr block_name;
	symptr block_conn [maxblocknodes];
	symptr p;
	unsigned block_nums [maxblocknodes];
	int nn, nc;
	exprlistptr el;
	char msg [maxnamelen + 100];
	char blockname_string [maxnamelen + 1];
	char sub_string [15];
	int sub_vals [max_subscripts];
	int isub;
	symptr s_sub;

	block_type = d->type;
	if (block_type->makeblock == NULL && block_type->defdef == NULL)
	{	sprintf (msg, "block %s: no such block type %s", d->name->name, block_type->name);
		print_error_context (d->src_context, msg);
		return;
	}
	for (isub = 0; isub < d->nsubscripts; isub++)
	{	s_sub = compile_expr (d->subscript_expr [isub], NULL);
		if (s_sub->const_val == NULL || !const_val_is_int (s_sub->const_val))
		{	sprintf (msg, "subscript for block starting with %s is not an integer constant", nodenameprefix);
			print_error_context (d->src_context, msg);
		}
		else
		{	sub_vals [isub] = const_val_as_int (s_sub->const_val);
		}
	}
	if (block_type->makeblock == NULL)
	{	if (block_type->defdef->returns != (symptr) NULL)
		{	sprintf (msg, "block %s: a call of block %s returns a value that is not used\n",
				d->name->name, block_type->name);
			print_error_context (d->src_context, msg);
		}
		strcpy (blockname_string, d->name->name);
		for (isub = 0; isub < d->nsubscripts; isub++)
		{	sprintf (sub_string, "[%d]", sub_vals [isub]);
			if (strlen (blockname_string) + strlen (sub_string) < maxnamelen)
			{	strcat (blockname_string, sub_string);
			}
			else
			{	sprintf (msg, "block name starting with %s is too long", blockname_string);
				print_error_context (d->src_context, msg);
			}
		}

		(void) compile_call (d->src_context, d->exprlist, block_type, (symptr) NULL, blockname_string);
	}
	else
	{
		strcpy (nodenameprefix + prefixlen, d->name->name);
		for (isub = 0; isub < d->nsubscripts; isub++)
		{	sprintf (sub_string, "[%d]", sub_vals [isub]);
			if (strlen (nodenameprefix) + strlen (sub_string) < maxnamelen)
			{	strcat (nodenameprefix, sub_string);
			}
			else
			{	sprintf (msg, "block name starting with %s is too long", nodenameprefix);
				print_error_context (d->src_context, msg);
			}
		}

		block_name = sym_hash (nodenameprefix);
		if (block_name->blockdef != NULL)
		{	sprintf (msg, "duplicate def for block %s", block_name->name);
			print_error_context (d->src_context, msg);
			return;
		}
		nc = 0;
		nn = 0;
		for (el = d->exprlist; el != NULL; el = el->next)
		{	p = compile_expr (el->expr, (symptr) NULL);
			if (p->nodedef != NULL && nc < maxblocknodes)
			{	block_conn [nc++] = p;
			}
			else if (has_valid_const (p) && nn < maxblocknodes)
			{	block_nums [nn++] = const_val_as_int (p->const_val);
			}
			else
			{	sprintf (msg, "incomprehensible param %d or too many params for block %s", nn + nc + 1, block_name->name);
				print_error_context (d->src_context, msg);
			}
		}
		if (el != NULL)
		{	sprintf (msg, "block %s too many names", block_name->name);
			print_error_context (d->src_context, msg);
			return;
		}
		if (block_type->makeblock->f != NULL)
			block_name->blockdef = (*block_type->makeblock->f) (d->src_context, block_name->name, block_conn, nc, block_nums, nn);
		else
			block_name->blockdef = make_fn (d->src_context, block_type->makeblock, block_name->name, block_conn, nc, block_nums, nn);
	}
}

void compile_def (defsynptr d, int flag_defs)
;

void def_for (defsynptr d)
{	symptr s;
	bool more;
	char msg [maxnamelen + 100];
	int iiter;

	compile_def (d->stmt1, 1);
	more = true;
	iiter = 0;
	while (more && iiter < max_loop_iters)
	{	compile_def (d->loop_body, 1);
		compile_def (d->stmt2, 1);
		s = compile_expr (d->expr, NULL);
		if (s->const_val == NULL)
		{	print_error_context (d->src_context, "condition in for statement is not a compile time variable");
			more = false;
		} else
		{	more = (s->const_val->sign_bit != 0 || s->const_val->n_val_bits != 0);
		}
		iiter++;
	}
	if (iiter == max_loop_iters)
		print_error_context (d->src_context, "too many iterations in for statement");
}

void def_while (defsynptr d)
{	symptr s;
	bool more;
	int iiter;

	more = true;
	iiter = 0;
	while (more && iiter < max_loop_iters)
	{	compile_def (d->loop_body, 1);
		s = compile_expr (d->expr, NULL);
		if (s->const_val == NULL)
		{	print_error_context (d->src_context, "condition in while statement is not a compile time variable");
			more = false;
		} else
		{	more = (s->const_val->sign_bit != 0 || s->const_val->n_val_bits != 0);
		}
		iiter++;
	}
	if (iiter == max_loop_iters)
		print_error_context (d->src_context, "too many iterations in while statement");
}
void def_if (defsynptr d)
{	symptr s;

	s = compile_expr (d->expr, NULL);
	if (s->const_val == NULL)
	{	print_error_context (d->src_context, "condition in for statement is not a compile time variable");
	} else
	{	if (s->const_val->sign_bit != 0 || s->const_val->n_val_bits != 0)
		{	compile_def (d->stmt1, 1);
		}
		else
		{	if (d->stmt2 != NULL)
			{	compile_def (d->stmt2, 1);
			}
		}
	}
}

void compile_prog (progsynptr prog, int flag_defs);

void compile_def (defsynptr d, int flag_defs)
{	char msg [maxnamelen + 100];

	if (n_compile_errors_found > max_compile_errors)
		return;

	switch (d->kind)
	{	case def_kind_node:
			def_node_array (d);
			break;

		case def_kind_block:
			def_block (d);
			break;

		case def_kind_eqn:
			def_eqn (d);
			break;

		case def_kind_assign:
			def_assign (d);
			break;

		case def_kind_def:
		case def_kind_macro:
			if (flag_defs)
			{	sprintf (msg, "def of %s is inside a def and will be ignored", d->name->name);
				print_error_context (d->src_context, msg);
			}
			break;

		case def_kind_logicvar:
			def_logicvar_array (d);
			break;

		case def_kind_for:
			def_for (d);
			break;

		case def_kind_while:
			def_while (d);
			break;

		case def_kind_if:
			def_if (d);
			break;

		case def_kind_compound:
			compile_prog (d->body, flag_defs);
			break;

	}
}

void compile_prog (progsynptr prog, int flag_defs)
{	progsynptr p;

	for (p = prog; p != NULL && n_compile_errors_found < max_compile_errors; p = p->next)
	{	compile_def (p->def, flag_defs);
	}
}
