/* copyright (c) David M. Lewis 1987 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>

#include "config.h"
#include "utils.h"

#include "debug.h"

#include "command.h"



int spacefree;
char *nextspacefree;

#define alloc_size_min		8
#define n_alloc_bins		12
#define chunk_alloc			(1 << 18)
#define alloc_aligner_extra	8

/* how many digits to print for i bit quantity */
int n_dig_to_print [max_node_width + 1];
logic_value_word word_div_output_base;
logic_value_word word_mod_output_base;

void my_assert_exit (const char *why, int linenum, const char *filename)
{	fprintf (stderr, "assert botched for %s at %d %s\n", why, linenum, filename);
	exit(1);
}

void *alloc_free_list [n_alloc_bins];

void *smalloc (size_t n) {
	int ibin;
	size_t s;
	size_t sx;
	int nbin;
	void *m;
	size_t n_m;

	if (n == 0)
		return NULL;
	for (s = alloc_size_min, ibin = 0; s < n && ibin < n_alloc_bins; ibin++)
		s <<= 1;
	if (ibin >= n_alloc_bins)
	{	m = malloc (n + alloc_aligner_extra);
		*(int *)m = -1;
		return ((void *) ((char *) m + alloc_aligner_extra));
	} else {
		if (alloc_free_list [ibin] == NULL) {
			sx = s + alloc_aligner_extra;
			nbin = (chunk_alloc / s );
			m = malloc (nbin * sx);
			while (nbin > 0)
			{	*(void **) m = (void *)alloc_free_list [ibin];
				alloc_free_list [ibin] = (char *)m;
				m = (void *) ((char *) m + sx);
				nbin--;
			}
		}
	}
	m = alloc_free_list [ibin];
	alloc_free_list [ibin] = *(void **)m;
	*(int *)m = ibin;
	return (void *) ((char *)m + alloc_aligner_extra);

}

void smfree (void *m) {
	int ibin;
	char *mx;

	if (m != NULL)
	{	mx = (char *) m - alloc_aligner_extra;
		ibin = *(int *) mx;
		if (ibin == -1)
		{	free (mx);
		}
		else
		{	*(void **)mx =  alloc_free_list [ibin];
			alloc_free_list [ibin] = mx;
		}
	}
}

void stopfn(int idontcare)
{
	if (!pleasestop)
		pleasestop = 1;
	else
		exit (3);
}

char *index (char *s, char c) {
	char *p;
	for (p = s; *p != '\0' && *p != c; p++)
	;
	if (*p != '\0') {
		return p;
	} else {
		return NULL;
	}
}

unsigned abtoi (char *s)
{	int i;
	unsigned v;
	unsigned bs;

	i = 0;
	bs = 1 << outputbaseshift;
	if (s [0] == '0')
	{	if (s [1] == 'x')
		{	bs = 16;
			i = 2;
		}
		else if (s [1] == 'b')
		{	bs = 2;
			i = 2;
		}
		else if (s [1] == 'd')
		{	bs = 10;
			i = 2;
		}
		else if (s [1] != '\0')
		{	bs = 8;
			i = 2;
		}
	}
	for (v = 0; (s [i] >= '0' && s [i] <= '9') || (s [i] >= 'a' && s [i] <= 'f'); i++)
	{	v *= bs;
		if (s [i] >= '0' && s [i] <= '9')
			v += s [i] - '0';
		else
			v += s [i] - 'a' + 10;
	}
	return (v);
}


int axtoi (char *s)
{	int i;
	char *cp;
	int base;
	int sign = 0;

	cp = s;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	base = 10;
	if (*cp == '-')
	{	sign = 1;
		cp++;
	}
	if (*cp == '0')
	{	base = 8;
		cp++;
		if (*cp == 'x')
		{	base = 16;
			cp++;
		}
		else if (*cp == 'b')
		{	base = 2;
			cp++;
		}
	}
	for (i = 0; (*cp >= '0' & *cp <= '9') || (*cp >= 'a' && *cp <= 'f'); cp++)
	{	if (*cp >= '0' && *cp <= '9')
			i = i * base + *cp - '0';
		else if (*cp >= 'a' && *cp <= 'f')
			i = i * base + *cp - 'a' + 10;
	}
	if (sign)
		i = -i;
	return (i);
}

/* convert string to logic_value, must have max_node_words */

void ascii_to_logic_value (char *s, logic_value wresult)
{
	int iw;
	logic_value_word w;
	logic_value_word wl, wh;
	logic_value_word cin;
	char *cp;

	for (iw = 0; iw < max_node_words; iw++) {
		wresult [iw] = 0;
	}
	for (cp = s; (*cp >= '0' && *cp <= '9') || (*cp >= 'a' && *cp <= 'f'); cp++)
	{   if (*cp >= '0' && *cp <= '9')
		{	cin = *cp - '0';
		} else if (*cp >= 'a' && *cp <= 'f')
		{   cin = *cp + 10 - 'a';
		}
		for (iw = 0; iw < max_node_words; iw++)
		{   w = wresult [iw];
			wl = w & halfword_all_bit_mask;
			wh = (w >> bits_per_halfword) & halfword_all_bit_mask;
			wl *= outputbase;
			wl += cin;
			cin = wl >> bits_per_halfword;
			wl &= halfword_all_bit_mask;
			wh *= outputbase;
			wh += cin;
			cin = wh >> bits_per_halfword;
			wresult [iw] = (wh << bits_per_halfword) | wl;
		}
	}
}

void *oldspace (int n)
{
	if (n == 0)
		return ((char *) NULL);
	n = (n + 3) & ~3;
	if (spacefree < n)
	{	if (n > hunk)
			return (malloc (n));
		nextspacefree = (char *) malloc (hunk);
		if (nextspacefree == (char *) -1)
		{	fprintf (stderr, "out of space\n");
			exit (2);
		}
		nextspacefree += hunk;
		spacefree = hunk;
	}
	nextspacefree -= n;
	spacefree -= n;
	return (nextspacefree);
}

char *newstring (const char *s)
{	char *r;
	int len;
	static char *string_free;
	static int n_string_free = 0;

	len = strlen (s) + 1;
	if (n_string_free < len)
	{	string_free = (char *) malloc (string_hunk);
		n_string_free = string_hunk;
		if (string_free == NULL)
		{	fprintf (stderr, "no core for strings\n");
			exit (2);
		}
	}
	r = string_free;
	string_free += len;
	n_string_free -= len;
	strcpy (r, s);
	return (r);
}

int hash (const char *s, int size)
{	unsigned i, j;
	const char *cp;

	for (j = 0, cp = s; *cp != '\0'; cp++)
	{	i = *cp & 0xff;
		j = j * 243 + (i << 3) - i;		/* quick i * 7 */
	}
	j = ((j << 4) + j) % size;
	return (j);
}

unsigned uhash (const char *s)
{	unsigned i, j;
	const char *cp;

	for (j = 0, cp = s; *cp != '\0'; cp++)
	{	i = *cp & 0xff;
		j = j &0xffffff;
		j = j * 117 + (i << 3) - i;
	}
	return (j);
}

void init_print_tables (void)
{   int ibit;
	int ibit_done;
	int iw;
	char s [max_node_width + 1];
	int ndig;
	logic_value v;
	logic_value_word vw [max_node_words];


	/* determine the div and mod of logic_value_word with respect to outputbase
	 * avoid unsigned overflow in the process by computing for logic_value_word / 2
	 */

	 word_div_output_base = ((unsigned) (1 << (bits_per_word - 1))) / outputbase;
	 word_div_output_base *= 2;
	 word_mod_output_base = ((unsigned) (1 << (bits_per_word - 1))) % outputbase;
	 word_div_output_base += (word_mod_output_base + word_mod_output_base) / outputbase;
	 word_mod_output_base = (word_mod_output_base + word_mod_output_base) % outputbase;



	/* for each possible number of bits, determine the largest possible number and how many digits it
	 * takes to print that value in the current base.
	 */

	v = vw;
	for (iw = 0; iw < max_node_words; iw++)
	{   vw [iw] = 0;
	}

	ibit_done = 0;

	for (ibit = 1; ibit <= max_node_width; ibit++)
	{   vw [(ibit - 1) / bits_per_word] |= 1 << (ibit - 1);
		print_logic_value_base_string (s, max_node_words, v, 0, &ndig);
		while (ibit_done <= ibit)
		{   n_dig_to_print [ibit_done++] = ndig;
		}
	}
}

/* print a logic_value in the outputbase and set digits_printed. Use at least digit_min.
 * prints the digits in order of significance, so s[0] is least siginificant digit
 */
 
void print_logic_value_base_string (char *s, int n_logic_value_words, logic_value v, int digit_min, int *digits_printed)
{   int iw;
	logic_value_word q [max_node_words];
	logic_value_word r [max_node_words];
	int idig;
	bool more;
	static char hexchars [] = "0123456789abcdef";

	for (iw = 0; iw < n_logic_value_words; iw++)
	{   q [iw] = v [iw];
		r [iw] = 0;
	}
	more = true;
	idig = 0;
	while (more || digit_min > 0)
	{   more = false;
		for (iw = 0; iw < n_logic_value_words; iw++)
		{   r [iw] = q [iw] % outputbase;
			q [iw] = q [iw] / outputbase;
		}
		for (iw = n_logic_value_words - 2; iw >= 0; iw--)
		{   q [iw] += r [iw + 1] * word_div_output_base;
			r [iw] += r [iw + 1] * word_mod_output_base;
			q [iw] += r [iw] / outputbase;
			r [iw] = r [iw] % outputbase;
			if (q [iw] > 0)
			{   more = true;
			}
		}
		s [idig] = hexchars [r [0]];
		idig++;
		digit_min--;
	}
	*digits_printed = idig;
}


/* print logic value that is wid bits wide */

char *print_logic_value_base (logic_value v, int wid)
{	static char buff [10] [max_node_width + 1];
	static int buffindex = 0;
	char s [max_node_width + 1];
	int ndig;
	int nprinted;
	int i;

	buffindex += 1;
	buffindex %= 10;
	ndig = n_dig_to_print [wid];
	print_logic_value_base_string (s, words_per_logicval (wid), v, ndig, &nprinted);
	for (i = 0; i < nprinted; i++)
	{   buff [buffindex] [nprinted - i - 1] = s [i];
	}
	buff [buffindex] [nprinted] = '\0';

	return buff [buffindex];
}

char *print_base (unsigned v, int minwid)
{	static char buff [10] [33];
	static int buffindex = 0;
	static char hexchars [] = "0123456789abcdef";
	char *cp;
	char *cpstart;
	unsigned mask;

	buffindex += 1;
	buffindex %= 10;
	cp = &(buff [buffindex] [32]);
	*cp-- = '\0';
	if (outputbasebinary)
	{	mask = bitwidmask (outputbaseshift);
		cpstart = cp;
		for (; v > 0 || cp == cpstart || minwid > 0; v >>= outputbaseshift, minwid--)
			*cp-- = hexchars [v & mask];
	}
	else
	{	for (; v > 0 || cp == cpstart || minwid > 0; v /= outputbase, minwid--)
			*cp-- = hexchars [v % outputbase];
	}
	return (cp + 1);
}

int bitrange_width (t_bitrange r)
{
	return (r.bit_high - r.bit_low + 1);
}

int bit_width (unsigned mask)
{	int i;

	for (i = bits_per_word - 1; i >= 0; i--) {
		if (((1 << i) | mask) == mask) {
			return (i + 1);
		}
	}
	return (0);
}

int n_logic_value_bits (logic_value v, int n)
{	int r;
	int iw;

	r = 0;
	for (iw = 0; iw < n; iw++)
	{	if (v [iw] != 0) {
			r = bit_width (v [iw]) + bits_per_word * iw + 1;
		}
	}
	return r;
}

char *print_masked_val (unsigned n, int nbits)
{	return (print_base (n, (nbits + outputbaseshift - 1) / outputbaseshift));
}


/* return a bit mask with 1's in any location in the range br */

logic_value_word bitrange_mask (t_bitrange br)
{	logic_value_word r;

	if (br.bit_low >= bits_per_word || br.bit_high < 0) {
		r = 0;
	} else {
		if (br.bit_high >= bits_per_word) {
			r = word_all_bit_mask;
		} else {
			r = (1 << br.bit_high) + ((1 << br.bit_high) - 1);
		}
		if (br.bit_low > 0) {
			r &= (word_all_bit_mask << br.bit_low);
		}
	}
	return r;
}

unsigned bitwidmask (int n)
{	if (n <= 0)
		return 0;
	else if (n < bits_per_word)
		return ((1 << n) - 1);
	else
		return (word_all_bit_mask);
}

logic_value_word node_bitmask (nptr n, int iw)
{   if (n->node_size.bit_high >= (iw + 1) * bits_per_word)
	{   return word_all_bit_mask;
	} else
	{   return bitwidmask (n->node_size.bit_high + 1 - iw * bits_per_word);
	}
}

int const_val_as_int (t_const_val *cp)
{	if (cp->n_val_bits == 0)
		return (-(int)cp->sign_bit);
	else
		return (cp->val_logic [0] - (cp->sign_bit << cp->n_val_bits));
}

bool const_val_is_int (t_const_val *cp)
{	return (cp->n_val_bits < bits_per_word);
}

void copy_const_val (t_const_val *cp_dst, t_const_val *cp_src)
{	int iw;
	logic_value_word *dw;

	cp_dst->n_val_bits = cp_src->n_val_bits;
	cp_dst->sign_bit = cp_src->sign_bit;
	cp_dst->n_logicval_words = cp_src->n_logicval_words;

	/* copy dst val in case copy to self */
	
	dw = cp_dst->val_logic;
	cp_dst->val_logic = (logic_value) smalloc (cp_src->n_logicval_words * sizeof (logic_value_word));
	for (iw = 0; iw < cp_src->n_logicval_words; iw++)
	{	cp_dst->val_logic [iw] = cp_src->val_logic [iw];
	}
	smfree (dw);
}

t_const_val *new_const_val (void)
{	t_const_val *r;

	r = (t_const_val *) smalloc (sizeof (t_const_val));
	r->n_val_bits = 0;
	r->sign_bit = 0;
	r->n_logicval_words = 0;
	r->val_logic = NULL;
	return r;
}

t_const_val *new_const_val_canonic (logic_value_word *v, int nw)
{	t_const_val *r;
	int ibit;
	int iw;

	r = (t_const_val *) smalloc (sizeof (t_const_val));
	if (nw > 0)
		r->sign_bit = (v [nw - 1] >> (bits_per_word - 1)) & 1;
	else
		r->sign_bit = 0;
	ibit = nw * bits_per_word - 1;
	while (ibit >= 0 && (((v [bit_div_word (ibit)] >> bit_mod_word (ibit)) & 1) == r->sign_bit))
		ibit--;
	r->n_val_bits = ibit + 1;
	r->n_logicval_words = bit_div_word (r->n_val_bits + bits_per_word - 1);
	if (r->n_val_bits > 0)
	{	r->val_logic = (logic_value_word *) smalloc (r->n_logicval_words * sizeof (logic_value_word));
		for (iw = 0; iw < r->n_logicval_words; iw++)
		{	r->val_logic [iw] = v [iw];
		}
		r->val_logic [r->n_logicval_words - 1] += r->sign_bit << bit_mod_word (r->n_val_bits);
	}
	else
		r->val_logic = NULL;
#ifdef debug_gcc_error
	printf ("sign %d bits %d\n", r->sign_bit, r->n_val_bits);
#endif
	return r;
}


logic_value_word get_const_word (t_const_val *cp, int iw)
{	logic_value_word r;

	/* if a valid word return it */
	if ((iw + 1) * bits_per_word <= cp->n_val_bits)
	{	r = cp->val_logic [iw];
	} else if (iw * bits_per_word >= cp->n_val_bits)
	/* if off the top word, return sign bit extended to word */
	{	r = (~ cp->sign_bit) + 1;
	} else
	{	/* top word, sign extend it */
		r = cp->val_logic [iw] + ((~(cp->sign_bit << (cp->n_val_bits - iw * bits_per_word))) + 1);
	}
	return r;
}

t_const_val *addsub_const_vals (t_const_val *op_a, t_const_val *op_b, bool neg_flag)
{	logic_value_word r [max_words_per_logicval];
	int result_bits;
	int result_words;
	logic_value_word wa;
	logic_value_word wb;
	logic_value_word carry;
	logic_value_word wh;
	int iw;
	logic_value_word s0, c0;

	/* +1 for sign bit and +1 for extra bit due to adding two things together */
	result_bits = my_max (op_a->n_val_bits, op_b->n_val_bits) + 2;
	result_words = bit_div_word (result_bits + bits_per_word - 1);
	if (neg_flag)
	{	carry = 1;
	}
	else
	{	carry = 0;
	}
	for (iw = 0; iw < result_words; iw++)
	{	wa = get_const_word (op_a, iw);
		wb = get_const_word (op_b, iw);
		if (neg_flag)
		{	wb = ~wb;
		}
		s0 = (wa ^ wb ^ carry) &1;
		c0 = ((wa & 1) + (wb & 1) + carry) >> 1;
		wh = (wa >> 1) + (wb >> 1) + c0;
		r [iw] = (wh << 1) | s0;
		carry = (wh >> (bits_per_word - 1)) & 1;
	}
	return (new_const_val_canonic (r, result_words));
}

t_const_val *boolop_const_vals (t_const_val *op_a, t_const_val *op_b, unsigned op)
{	logic_value_word r [max_words_per_logicval];
	int result_bits;
	int result_words;
	logic_value_word wa;
	logic_value_word wb;
	logic_value_word carry;
	logic_value_word wr;
	int iw;
	int ibit;

	/* +1 for sign bit */
	result_bits = my_max (op_a->n_val_bits, op_b->n_val_bits) + 1;
	result_words = bit_div_word (result_bits + bits_per_word - 1);
	/* round up to integral words so canonic works */
	result_bits = result_words * bits_per_word;
	for (iw = 0; iw < result_words; iw++)
	{	wa = get_const_word (op_a, iw);
		wb = get_const_word (op_b, iw);
		wr = 0;
		for (ibit = 0; ibit + iw * iw < result_bits; ibit++)
		{	/* gaaaahh*/
			wr |= ((op >> ((((wa >> ibit) & 1) << 1) | ((wb >> ibit) & 1))) & 1) << ibit;
		}
		r [iw] = wr;
	}
	return (new_const_val_canonic (r, result_words));
}

void getwords (char *line, char wordtable [maxwords] [wordlen], int *wordsfound, int wtabsize)
{	int i;
	int j;
	int nfound;

	nfound = 0;
	for (i = 0; line [i] != '\n' && line [i] != '\0' && nfound < wtabsize;)
	{	while (line [i] == ' ' || line [i] == '\t')
			i++;
		if (line [i] != '\n' && line [i] != '\0')
		{	if (nfound == maxwords)
			{	fprintf (stderr, "word buffer overflow while parsing something\n");
				exit (2);
			}
			if (line [i] == '"')
			{	i++;
				for (j = 0; j < wordlen - 1 && line [i] != '"' && line [i] != '\n'; j++, i++)
	
					wordtable [nfound] [j] = line [i];
				wordtable [nfound] [j] = '\0';
				if (line [i] == '"')
					i++;
			}
			else
			{	for (j = 0; j < wordlen - 1 && line [i] != ' ' && line [i] != '\t' && line [i] != '\n'; j++, i++)
	
					wordtable [nfound] [j] = line [i];
				wordtable [nfound] [j] = '\0';
			}
			nfound++;
		}
	}
	*wordsfound = nfound;
}





