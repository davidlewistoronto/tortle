#ifndef __NETHELP_H
#define __NETHELP_H

extern char nodenameprefix [maxprefixlen];
extern int prefixlen;
extern int stackdepth;
extern t_src_context_stack *src_context_stack;
extern int n_compile_errors_found;

void init_compile_stacks (void);

symptr check_temp_node (t_src_context *sc, symptr s, int w);
nptr makenode_sc (t_src_context *sc, char *s, int width);
void define_block_types (void);

nptr make_subnode(t_src_context *sc, char *s, nptr n, int sel_low, int sel_high);
void make_subnode_of (t_src_context *sc, nptr n, nptr nn, int sel_low, int sel_high);
void check_nodes (t_src_context *sc, symptr *sa, int n, int minmax);
bptr new_block (const char *name, block_sim_fn fn, block_codegen_fn cfn, int nstate, int nins, nptr *ins, int nouts, nptr *outs, bool *init_outputactive, const char *type_name);

obj_ptr find_obj (char *s);
int obj_has_node (obj_ptr o);
unsigned obj_node_bitmask (obj_ptr o);
int obj_has_block (obj_ptr o);
unsigned *obj_state_addr (obj_ptr o);
const char *obj_name (obj_ptr o);
const char *obj_block_type_name (obj_ptr o);
symptr sym_array_elem_0 (symptr s);

symptr new_temp (void);

symptr sym_prefix_hash_node (t_src_context *sc, char *s, nptr newn, int newwidth);
symptr sym_prefix_hash (char *s);

void pushprefix (t_src_context *sc, const char *s, int n);
void popprefix (void);
void print_error (const char *s);
void print_error_context (t_src_context *sc, const char *s);
void print_error_context_stack (t_src_context_stack *scs, const char *s);

extern makeblockrec *make_and;
extern makeblockrec *make_nand;
extern makeblockrec *make_expand;
extern makeblockrec *make_or;
extern makeblockrec *make_xor;
extern makeblockrec *make_eqeq;
extern makeblockrec *make_lsh;
extern makeblockrec *make_rsh;
extern makeblockrec *make_adder;
extern makeblockrec *make_sub;
extern makeblockrec *make_greater;
extern makeblockrec *make_less;
extern makeblockrec *make_leq;
extern makeblockrec *make_geq;
extern makeblockrec *make_eq;
extern makeblockrec *make_neq;
extern makeblockrec *make_latch;
extern makeblockrec *make_undefined;

#endif
