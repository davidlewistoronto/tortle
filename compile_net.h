#ifndef __COMPILE_NET_H
#define __COMPILE_NET_H

#include "parser.h"
symptr compile_call (t_src_context *sc, exprlistptr exprlist, symptr fun, symptr s, const char *name);
symptr compile_expr (exprsynptr e, symptr s);

void compile_prog (progsynptr prog, int flag_defs);

extern int max_loop_iters;

#endif
 