/* copyright (c) David M. Lewis 1987 */
#ifndef __CONFIG_H
#define __CONFIG_H


/* bits_per_word must be a power of 2 */
#define log2_bits_per_word  5
#define word_all_bit_mask   0xffffffff
#define log2_bits_per_halfword  (log2_bits_per_word - 1)
#define bits_per_halfword   (1 << log2_bits_per_halfword)
#define halfword_all_bit_mask (((unsigned) 1 << bits_per_halfword) - 1)
#define defaultnodewidth    32
#define bits_per_word       (1 << log2_bits_per_word)
#define max_node_width      256
#define max_node_words		(max_node_width/bits_per_word)
#define bit_mod_word(n)     ((n) & (bits_per_word - 1))
#define bit_div_word(n)     ((n) >> log2_bits_per_word)
#define words_per_logicval(n)       (((unsigned) (n) + (bits_per_word - 1)) / bits_per_word)
#define max_words_per_logicval      words_per_logicval(max_node_width)

/* number of words to be written as a chunk in trace */
#define wordsperclick       1

#define hunk				0x40000

#define sym_hunk			0x4000
#define string_hunk			0x40000

#define maxwords			100
/* wordlen must be >= max_node_width to be able to parse a logic value in binary */
#define wordlen				300


#define max_line_len		1000

#define maxtokenlength		200

#define symtablesize		993177

#define filesymtablesize	97

#define maxprefixlen		1000
#define maxcalldepth		25

/* max length of a name including prefix for call stack */

#define maxnamelen			(maxprefixlen + maxtokenlength)

#define maxblocknodes		1000

#define max_subscripts		10
#define max_array_size		100000

#define default_max_loop_iters	1000

#define max_command_loop_depth 25

#define max_file_depth		10

#define max_include_dirs	20
#define max_include_name_len	100

/* see related stuff in utils.h */

#define desc_table_hunk		17
#define desc_table_size(x)		((x + 17) / 11 * 17)

#define max_vector_nodes	500

#define envname "tortledir"

#define helpfile "/%s/tortle/helpfile"


#define max_compile_errors	5


#endif
