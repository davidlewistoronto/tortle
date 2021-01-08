#ifndef __UTILS_H
#define __UTILS_H




#include "tortle_types.h"
#include "config.h"

#define my_max(x,y) ((x) > (y) ? (x) : (y))
#define my_min(x,y) ((x) < (y) ? (x) : (y))

#define my_assert(x,y) if (!(x)) {my_assert_exit (y, __LINE__, __FILE__);}
#define exit_with_err(y) {fprintf (stderr, "assert botched for %s at %d %s\n", y, __LINE__, __FILE__); exit(1);}

void my_assert_exit (const char *why, int linenum, const char *filename);

char *newstring (const char *s);
void *space (int n);

char *print_base (unsigned v, int minwid);
char *print_masked_val (unsigned n, int nbits);
void print_logic_value_base_string (char *s, int n_logic_value_words, logic_value v, int digit_min, int *digits_printed);
char *print_logic_value_base (logic_value v, int wid);
void init_print_tables (void);
extern int n_dig_to_print [max_node_width + 1];

unsigned bitwidmask (int n);
int bit_width (unsigned mask);
int bitrange_width (t_bitrange r);
logic_value_word node_bitmask (nptr n, int iw);
logic_value_word bitrange_mask (t_bitrange br);

int n_logic_value_bits (logic_value v, int n);

int const_val_as_int (t_const_val *cp);
bool const_val_is_int (t_const_val *cp);
void copy_const_val (t_const_val *cp_dst, t_const_val *cp_src);
logic_value_word get_const_word (t_const_val *cp, int iw);
t_const_val *new_const_val (void);
t_const_val *new_const_val_canonic (logic_value_word *v, int nw);
t_const_val *addsub_const_vals (t_const_val *op_a, t_const_val *op_b, bool neg_flag);
t_const_val *boolop_const_vals (t_const_val *op_a, t_const_val *op_b, unsigned op);

#define has_valid_const(s)	((s) != NULL && (s)->const_val != NULL)
#define has_valid_int_const(s)	(has_valid_const(s) && const_val_is_int(s->const_val))


void *smalloc (size_t n);
void smfree (void *p);

void stopfn (int);
int hash (const char *s, int size) ;
unsigned uhash (const char *s);

unsigned abtoi (char *s);
int axtoi (char *s);
void ascii_to_logic_value (char *s, logic_value wresult);
void getwords (char *line, char wordtable [maxwords] [wordlen], int *wordsfound, int wtabsize);

#ifdef __BORLANDC__
char *index (char *s, char c);
#endif


#endif

