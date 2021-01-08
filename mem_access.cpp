/* copyright (c) David M. Lewis 1987 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "utils.h"
#include "parser.h"
#include "tortle_types.h"
#include "logic.h"
#include "debug.h"
#include "command.h"
#include "sim.h"
#include "nethelp.h"

void dumpram (obj_ptr ob, int s, int e)
{	int i;
	int j;
	int k;
	int stride;
	int wordnum;
	int bitpos;
	unsigned mask;
	int charperword;
	int state_words_per_word;
	int words_per_state_word;
	int width;
	int depth;
	unsigned lastline [70];
	unsigned *blk_state;
	const char *btname;
	logic_value_word w;

	blk_state = obj_state_addr (ob);
	btname = obj_block_type_name (ob);
	width = blk_state [state_word_ram_width];
	depth = blk_state [state_word_ram_depth];
	state_words_per_word = blk_state [state_word_ram_state_words_per_word];
	words_per_state_word = blk_state [state_word_ram_words_per_state_word];

	if (strcmp (btname, "ram") && strcmp (btname, "rom"))
	{	fprintf (stderr, "block %s is not a ram or rom\n", obj_name (ob));
		return;
	}
	charperword = n_dig_to_print [width];
	stride = 70 / (charperword + 1);
	for (i = 0; stride != 1; i++, stride >>= 1)
		;
	stride = 1 << i;
	for (i = s; i < e && i < depth && !pleasestop;)
	{	printf ("%06x ", i);
		for (j = 0; j < stride && i < e && i < depth && !pleasestop; i++, j++)
		{	wordnum = i * state_words_per_word / words_per_state_word;
			if (width <= bits_per_word)
			{	bitpos = (i % words_per_state_word) * width;
				mask = bitwidmask (width);
				w = (blk_state [state_word_ram_state_data + wordnum] >> bitpos) & mask;
				printf (" %s", print_logic_value_base (&w, width));
			} else
			{	printf (" %s", print_logic_value_base (blk_state + state_word_ram_state_data + wordnum, width));
			}
		}
		printf ("\n");
	}
}




void changeram (obj_ptr ob, int *n, char *s)
{
	int wordnum;
	int bitpos;
	unsigned *blk_state;
	const char *btname;
	int state_words_per_word;
	int words_per_state_word;
	int width;
	int depth;
	logic_value_word v [max_node_words];
	t_bitrange r;

	blk_state = obj_state_addr (ob);
	btname = obj_block_type_name (ob);
	width = blk_state [state_word_ram_width];
	depth = blk_state [state_word_ram_depth];
	state_words_per_word = blk_state [state_word_ram_state_words_per_word];
	words_per_state_word = blk_state [state_word_ram_words_per_state_word];

	blk_state = obj_state_addr (ob);
	if (strcmp (btname, "ram") && strcmp (btname, "rom"))
	{	fprintf (stderr, "block %s is not a ram or rom\n", obj_name (ob));
		return;
	}
	if (*n >= depth || *n < 0)
	{	fprintf (stderr, "change ram address out of bounds\n");
	}
	wordnum = *n * state_words_per_word / words_per_state_word;
	if (width <= bits_per_word)
	{	r.bit_low = ((*n) % words_per_state_word) * width;
		r.bit_high = r.bit_low + width - 1;
	} else
	{	r.bit_low = 0;
		r.bit_high = width - 1;
	}
	ascii_to_logic_value (s, v);
	insert_shifted_bits (blk_state + wordnum + state_word_ram_state_data, max_node_words, v, r.bit_low, r);

	(*n)++;
}

void examineram (obj_ptr ob, int *nn)
{	
	int n;
	int stride;
	int wordnum;
	int bitpos;
	unsigned mask;
	int charperword;
	int state_words_per_word;
	int words_per_state_word;
	int width;
	int depth;
	unsigned lastline [70];
	unsigned *blk_state;
	const char *btname;
	logic_value_word w;
	logic_value_word v [max_node_words];
	bool more;
	char s [100];


	blk_state = obj_state_addr (ob);
	btname = obj_block_type_name (ob);
	width = blk_state [state_word_ram_width];
	depth = blk_state [state_word_ram_depth];
	state_words_per_word = blk_state [state_word_ram_state_words_per_word];
	words_per_state_word = blk_state [state_word_ram_words_per_state_word];

	if (strcmp (btname, "ram") && strcmp (btname, "rom"))
	{	fprintf (stderr, "block %s is not a ram or rom\n", obj_name (ob));
		return;
	}
	n = *nn;
	more = true;
	while (more && n < depth && !pleasestop)
	{	wordnum = n * state_words_per_word / words_per_state_word;
		if (width <= bits_per_word)
		{	bitpos = (n % words_per_state_word) * width;
			mask = bitwidmask (width);
			w = (blk_state [state_word_ram_state_data + wordnum] >> bitpos) & mask;
			printf (" %s ", print_logic_value_base (&w, width));
		} else
		{	printf (" %s ", print_logic_value_base (blk_state + state_word_ram_state_data + wordnum, width));
		}
		if (fgets (s, 100, stdin) == (char *) NULL || s [0] == 'q')
		{	more = false;
			clearerr (stdin);
		}
		else if (s [0] == '-')
			n--;
		else if (s [0] == '\n')
			n++;
		else
		{	changeram (ob, &n, s);
		}
	}
	*nn = n;
}



void romload (obj_ptr ob, char *fs)
{	int nwords;
	int nread;
	unsigned *blk_state;
	const char *btname;
	int state_words_per_word;
	int words_per_state_word;
	int width;
	int depth;
	FILE *f;



	blk_state = obj_state_addr (ob);
	btname = obj_block_type_name (ob);
	if (strcmp (btname, "ram") && strcmp (btname, "rom"))
	{	fprintf (stderr, "block %s is not a ram or rom\n", obj_name (ob));
		return;
	}
	depth = blk_state [state_word_ram_depth];
	state_words_per_word = blk_state [state_word_ram_state_words_per_word];
	words_per_state_word = blk_state [state_word_ram_words_per_state_word];
	if ((f = fopen (fs, "r")) == NULL)
	{	fprintf (stderr, "can't open %s\n", fs);
		return;
	}
	nwords = (depth * state_words_per_word + words_per_state_word - 1) / words_per_state_word;
	nread = fread ((char *) &(blk_state [state_word_ram_state_data]), sizeof (unsigned), nwords, f);
	fclose (f);
	printf ("read %d words into %s\n", nread * blk_state [2], obj_name (ob));
}
