#ifndef __SCAN_PARSE_H
#define __SCAN_PARSE_H

progsynptr scan_parse (char *s);
void add_include_dir (char *s);
symptr sym_hash (const char *s);
symptr sym_hash_lookup (const char *s);
symptr get_sym_matches (char *pattern, bool exact_match, bool start_match);
symptr all_sym_list (void);

symptr sym_prefix_hash_indexed (char *s, int nindex, int *index);
symptr sym_check_node (t_src_context *sc, symptr p, nptr newn, int newwidth);
void match_token (int tok);
void syntax_error (const char *s);
void init_sym_table (void);
void add_to_sym_hash_tbl (t_symrec *sp);

extern symptr symboltable [symtablesize];

extern int syntax_error_found;

FILE *ifopen (const char *name, const char *mode);



#endif
 
