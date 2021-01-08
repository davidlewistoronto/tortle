/* copyright (c) David M. Lewis 1987 */

#ifndef __PARSER_H
#define __PARSER_H

#include "tortle_types.h"


typedef enum {
	token_no_symbol = -1,
	token_symbol,
	token_num,
	token_semicolon,
	token_lpar,
	token_rpar,
	token_equal,
	token_eqeq,
	token_lsquig,
	token_rsquig,
	token_plus,
	token_xor,
	token_slash,
	token_star,
	token_greater,
	token_dotdot,
	token_comma,
	token_pound,
	token_less,
	token_leq,
	token_geq,
	token_or,
	token_and,
	token_minus,
	token_neq,
	token_lsh,
	token_rsh,
	token_concat,
	token_lsquare,
	token_rsquare,
	token_colon,
	token_up,
	token_question,
	token_assign,
	token_EOF

} e_token;

typedef struct progsyntax {
	defsynptr def;
	progsynptr next;
	} t_progsyntax;

typedef struct defsyntax {
	int kind;
	t_src_context *src_context;
	exprsynptr width_expr;
	int ninstances;
	symptr name;
	int nsubscripts;
	exprsynptr *subscript_expr;
	symptr type;
	exprsynptr expr;	/* RHS in assignment, condition in if, for, or while statment */
	expr2synptr clk_expr;
	expr2synptr latch_expr;
	expr2synptr buf_expr;
	expr5synptr lhs;
	symptr returns;
	numlistptr numlist;
	namelistptr namelist;
	exprlistptr exprlist;
	progsynptr body;
	defsynptr stmt1;	/* in for statement, initial statment; in if statement, the then clause */
	defsynptr stmt2;    /* in for statement, update statment; in if statement, the else clause */
	defsynptr loop_body;
	} t_defsyntax;

#define def_kind_def	0
#define def_kind_node	1
#define def_kind_eqn	2
#define def_kind_block	3
#define def_kind_ionode	4
#define def_kind_macro	5
#define def_kind_int	6
#define def_kind_logicvar	7
#define def_kind_for	8
#define def_kind_while	9
#define def_kind_if		10
#define def_kind_compound	11
#define def_kind_assign	12

typedef struct exprsyntax {
	int kind;
	t_src_context *src_context;
	exprsynptr expr;		/* if clocked, then the data; if an cond then the if clause */
	exprsynptr exprelse;	/* only used for else clause of cond */
	expr0synptr expr0; 		/* clock if a latched or clocked expression, or the expr0 if just an expr0 */
	} t_exprsyntax;

#define expr_kind_expr0		0
#define expr_kind_3state	1
#define expr_kind_clocked	2
#define expr_kind_latched	3
#define expr_kind_cond		4

typedef struct expr0syntax {
	int kind;
	t_src_context *src_context;
	symptr name;
	expr1synptr expr1a;
	expr1synptr expr1b;
	} t_expr0syntax;

#define expr0_kind_expr1 0
#define expr0_kind_less 1
#define expr0_kind_greater 2
#define expr0_kind_leq 3
#define expr0_kind_geq 4
#define expr0_kind_equal 5
#define expr0_kind_neq 6

typedef struct expr1syntax {
	int kind;
	t_src_context *src_context;
	expr1synptr expr1;
	expr2synptr expr2;
	} t_expr1syntax;

#define expr1_kind_expr2 0
#define expr1_kind_plus 1
#define expr1_kind_minus 2


typedef struct expr2syntax {
	int kind;
	t_src_context *src_context;
	expr3synptr expr3;
	expr2synptr expr2;
	exprsynptr sub_start;
	exprsynptr sub_end;
	} t_expr2syntax;

#define expr2_kind_expr3 0
#define expr2_kind_or 1
#define expr2_kind_xor 2
#define expr2_kind_eqeq 3
#define expr2_kind_lsh 4
#define expr2_kind_rsh 5
#define expr2_kind_concat 6
#define expr2_kind_concat_range 7

typedef struct expr3syntax {
	int kind;
	t_src_context *src_context;
	symptr name;
	expr4synptr expr4;
	expr3synptr expr3;
	} t_expr3syntax;

#define expr3_kind_expr4 0
#define expr3_kind_and 1

typedef struct expr4syntax {
	int kind;
	t_src_context *src_context;
	unsigned num1, num2;
	expr4synptr expr4;
	exprsynptr expr;
	exprlistptr exprlist;
	expr5synptr expr5;
	} t_expr4syntax;

#define expr4_kind_expr5	0
#define expr4_kind_not		1
#define expr4_kind_star		2
#define expr4_kind_call		3
#define expr4_kind_mod		4
#define expr4_kind_div		5

/* An expr5 is some form of connection to a node, and is used by make_block routines */

typedef struct expr5syntax {
	int kind;
	t_src_context *src_context;
	t_const_val *const_num;
	symptr name;
	exprsynptr expr;
	expr5synptr expr5;
	int nsubscripts;
	exprsynptr *subscript_expr;
	exprsynptr sel_low_exp;
	exprsynptr sel_high_exp;
	} t_expr5syntax;

/* since selranges are handled by iteration in parser, all combinations of name vs expr,
 * selrange or not.
 */

#define expr5_kind_name				0
#define expr5_kind_expr 			2
#define expr5_kind_expr_selbit	 	3
#define expr5_kind_expr_selrange 	4
#define expr5_kind_const 			5
#define expr5_kind_number		   	6
#define expr5_kind_neg				7

typedef struct namelist {
	t_src_context *src_context;
	symptr name;
	int n_dims;
	exprsynptr *subscripts;
	namelistptr next;
	} t_namelist;


typedef struct exprlist {
	t_src_context *src_context;
	exprsynptr expr;
	exprlistptr next;
	} t_exprlist;


#endif
