/* copyright (c) David M. Lewis 1987 */



#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#ifndef __BORLANDC__
#include <unistd.h>
#endif

// #include <regex.h>
#include <pcre.h>

#include "config.h"
#include "parser.h"
#include "tortle_types.h"
#include "debug.h"
#include "utils.h"
#include "nethelp.h"
#include "scan_parse.h"

#ifdef use_fancy_hash_table
#include "hash_table.h"
#endif

FILE *input_file;
FILE *file_stack [max_file_depth];

char *input_file_name;
t_symrec *input_file_name_sym = NULL;

char *input_file_name_stack [max_file_depth];
t_symrec *input_file_name_sym_stack [max_file_depth];

int file_depth = 0;
t_src_context *current_src_context;

int n_include_dirs = 0;
char include_dirs [max_include_dirs] [max_include_name_len];

int linenum = 1;
int linenum_stack [max_file_depth];

int char_in_line = 0;
char linebuf [max_line_len];
symptr next_token_sym;
unsigned next_token_num;
logic_value_word next_token_logic_value [max_node_words];
t_const_val next_token_const_val;
int next_token;
int syntax_error_found = 0;
int statement_syntax_error_found;

#ifdef use_fancy_hash_table
hash_table<bool> file_included_hash;
#endif

#define max_syntax_errs 5

#define err_ret(t)	if (statement_syntax_error_found || syntax_error_found > max_syntax_errs) return ((t) NULL)

symptr sym_def;
symptr sym_node;
symptr sym_block;
symptr sym_blk;
symptr sym_end;
symptr sym_ionode;
symptr sym_macro;
symptr sym_for;
symptr sym_while;
symptr sym_if;
symptr sym_else;
symptr sym_logicval;
symptr sym_div;
symptr sym_mod;

symptr symboltable [symtablesize];

symptr filesymboltable [symtablesize];

exprsynptr parse_expr(void);

const char *token_strings [token_EOF + 1];

void init_token_strings ()
{
	token_strings [token_symbol] = "name";
	token_strings [token_num] = "number";
	token_strings [token_semicolon] = ";";
	token_strings [token_lpar] = "(";
	token_strings [token_rpar] = ")";
	token_strings [token_equal] = "=";
	token_strings [token_eqeq] = "==";
	token_strings [token_lsquig] = "{";
	token_strings [token_rsquig] = "}";
	token_strings [token_plus] = "+";
	token_strings [token_xor] = ":+:";
	token_strings [token_slash] = "/";
	token_strings [token_star] = "*";
	token_strings [token_greater] = ">";
	token_strings [token_dotdot] = "..";
	token_strings [token_comma] = ",";
	token_strings [token_pound] = "#";
	token_strings [token_less] = "<";
	token_strings [token_leq] = "<=";
	token_strings [token_geq] = ">=";
	token_strings [token_or] = "|";
	token_strings [token_and] = "&";
	token_strings [token_minus] = "-";
	token_strings [token_neq] = "!=";
	token_strings [token_lsh] = "<<";
	token_strings [token_rsh] = ">>";
	token_strings [token_concat] = "||";
	token_strings [token_lsquare] = "[";
	token_strings [token_rsquare] = "]";
	token_strings [token_colon] = ":";
	token_strings [token_up] = "^";
	token_strings [token_question] = "?";
	token_strings [token_assign] = ":=";
	token_strings [token_EOF] = "EOF";

}


symptr sym_hash_tbl (const char *s, symptr tbl [], int tbl_size, bool create)
{	int i;
	symptr p;
	char *s1;

	i = hash (s, tbl_size);
	for (p = tbl [i]; p != NULL && strcmp (p->name, s); p = p->next)
		;
	if (p == NULL && create)
	{	s1 = newstring (s);
		p = (t_symrec *) smalloc (sizeof (t_symrec));
		p->next = tbl [i];
		tbl [i] = p;
		p->name = s1;
		p->nodedef = NULL;
		p->is_filename = false;
		p->file_included = false;
		p->blockdef = NULL;
		p->defdef = NULL;
		p->makeblock = NULL;
		p->const_val = NULL;
		p->n_array_elems = 0;
		p->array_elems = (t_symrec **) NULL;
	}
	if (debug_hash)
		printf ("hash \"%s\" = %d, sym =  %x\n", s, i, (unsigned) p);
	return (p);
}

void add_to_sym_hash_tbl (t_symrec *sp)
{	int i;

	i = hash (sp->name, symtablesize);
	sp->next = symboltable [i];
	symboltable [i] = sp;
}

symptr sym_hash (const char *s)
{	return (sym_hash_tbl (s, symboltable, symtablesize, true));
}

symptr sym_hash_lookup (const char *s)
{	return (sym_hash_tbl (s, symboltable, symtablesize, false));
}

symptr sym_prefix_hash (char *s)
{	symptr p;

	strcpy (nodenameprefix + prefixlen, s);
	p = sym_hash (nodenameprefix);
	return p;
}

/* compare two strings but treating sequence of digits in corresponding locations in each string as a number */

int strcmp_with_nums (char *c1, char *c2)
{	int r;
	int num1, num2;

	r = 0;
	while (r == 0 && (*c1 != '\0' || *c2 != '\0'))
	{	if (*c1 == '\0')
			r = -1;
		else if (*c2 == '\0')
			r = 1;
		else if (isdigit (*c1) && isdigit (*c2))
		{	num1 = 0;
			num2 = 0;
			while (isdigit (*c1))
			{	num1 = num1 * 10 + (*c1 - '0');
				c1++;
			}
			while (isdigit (*c2))
			{	num2 = num2 * 10 + (*c2 - '0');
				c2++;
			}
			if (num1 < num2)
				r = -1;
			else if (num1 > num2)
				r = 1;
		}
		else if (*c1 < *c2)
		{	r = -1;
		}
		else if (*c1 > *c2)
		{	r = 1;
		}
		else
		{	c1++;
			c2++;
		}
	}
	return r;
}

int compare_syms (symptr s1, symptr s2)
{	int cmp;

	if (s1 == NULL)
		return 1;
	else if (s2 == NULL)
		return -1;
	else
	{	cmp = strcmp_with_nums (s1->name, s2->name);
		return cmp;
	}
}

symptr sorted_sym_list (symptr p)
{   symptr slist;
	symptr slist_last;
	symptr s_part1, s_part2;
	symptr pmid;
	symptr pmid_prev;
	symptr pmid_x2;
	symptr t;
	int cmp;

	if (p == NULL || p->next_match == NULL)
		return p;
	pmid = p;
	pmid_prev = NULL;
	pmid_x2 = p;
	{	while (pmid_x2 != NULL)
		{	pmid_prev = pmid;
			pmid = pmid->next_match;
			pmid_x2 = pmid_x2->next_match;
			if (pmid_x2 != NULL)
			{	pmid_x2 = pmid_x2->next_match;
			}
		}
		pmid_prev->next_match = NULL;
		s_part1 = sorted_sym_list (p);
		s_part2 = sorted_sym_list (pmid);
	}

	if (compare_syms (s_part1, s_part2) <= 0)
	{	slist = s_part1;
		s_part1 = s_part1->next_match;
	}
	else
	{	slist = s_part2;
		s_part2 = s_part2->next_match;
	}
	slist_last = slist;
	while (s_part1 != NULL || s_part2 != NULL)
	{	cmp = compare_syms (s_part1, s_part2);
		if (cmp < 0)
		{	slist_last->next_match = s_part1;
			s_part1 = s_part1->next_match;
		}
		else
		{   slist_last->next_match = s_part2;
			s_part2 = s_part2->next_match;
		}
		slist_last = slist_last->next_match;

	}
	slist_last->next_match = NULL;
	return slist;
}


/* scan the symbol table and return a list of all symbols that match the pattern, using the next_match link.
 * if pattern is NULL or a null string then match all symbols.
 * if exact_match is true then name must exactly match. useful to unify treatment of single name and pattern matching
 */

symptr get_sym_matches (char *pattern, bool exact_match, bool start_match)
{	symptr head;
	int i;
	symptr s;
	bool match;
	bool have_pcre;
	pcre *pcre_result;
	char *errptr;
	int erroffset;
	bool pcre_matches;
	bool match_all;
	int patt_len;

	/* if it is an exact match then just look up the symbol in the hash table and be done with it */

	if (exact_match)
	{	head = sym_hash_lookup (pattern);
		if (head != NULL)
			head->next_match = NULL;
	}
	else
	{	if (start_match)
		{   patt_len = strlen (pattern);
			head = (symptr) NULL;
			for (i = 0; i < symtablesize; i++)
			{	for (s = symboltable [i]; s != (symptr) NULL; s = s->next)
				{	if (strncmp (s->name, pattern, patt_len) == 0)
					{	s->next_match = head;
						head = s;
					}
				}
			}
		}
		else
		{	have_pcre = false;
			pcre_matches = false;	/* set this to false and only set to true if have a pcre and it matches */
			match_all = (pattern == NULL) || (pattern [0] == '\0');
			if (!match_all)
			{	pcre_result =  pcre_compile (pattern, PCRE_CASELESS, (const char **) &errptr, &erroffset, NULL);
				if (pcre_result == NULL)
					fprintf (stderr, "bad expr '%s'\n", pattern);
				else
					have_pcre = true;
			}
			head = (symptr) NULL;
			for (i = 0; i < symtablesize; i++)
			{	for (s = symboltable [i]; s != (symptr) NULL; s = s->next)
				{	if (have_pcre)
					{
#ifdef __BORLANDC__
						pcre_matches = pcre_exec (pcre_result, NULL, s->name, strlen (s->name), 0, NULL, 0) >= 0;
#else
						pcre_matches = pcre_exec (pcre_result, NULL, s->name, strlen (s->name), 0, 0, NULL, 0) >= 0;
#endif
					}
					if (match_all || pcre_matches)
					{	s->next_match = head;
						head = s;
					}
				}
			}
		}
	}
//	return head;
	return sorted_sym_list (head);
}

symptr all_sym_list (void)
{	int iidx;
	symptr r;
	symptr s;

	r = NULL;
	for (iidx = 0; iidx < symtablesize; iidx++)
	{	for (s = symboltable [iidx]; s != (symptr) NULL; s = s->next)
		{	s->next_match = r;
			r = s;
		}
	}
	return r;
}


symptr sym_prefix_hash_indexed (char *s, int nindex, int *index)
{	symptr p;
	int iidx;
	int pos;
	char cidx [15];

	pos = prefixlen + strlen (s);
	strcpy (nodenameprefix + prefixlen, s);
	for (iidx = 0; iidx < nindex; iidx++)
	{	sprintf (cidx, "[%d]", index [iidx]);
		strcpy (nodenameprefix + pos, cidx);
		pos += strlen (cidx);
	}
	p = sym_hash (nodenameprefix);
	return p;
}

symptr sym_check_node (t_src_context *sc, symptr p, nptr newn, int newwidth)
{	char msg [maxnamelen + 100];

	if (p->nodedef == NULL)
	{	if (newn == NULL)
		{	if (newwidth == 0)
			{	sprintf (msg, "no node named %s", nodenameprefix);
				print_error_context (sc, msg);
				p->nodedef = makenode_sc (sc, p->name, 1);
			}
			else
			{	p->nodedef = makenode_sc (sc, p->name, newwidth);
			}
		}
		else
		{	p->nodedef = newn;
		}
	}
	else if (newn != NULL || newwidth != 0)
	{	sprintf (msg, "duplicate def for %s", nodenameprefix);
		print_error_context (sc, msg);
	}
	return (p);
}

symptr sym_prefix_hash_node (t_src_context *sc, char *s, nptr newn, int newwidth)
{	symptr p;

	strcpy (nodenameprefix + prefixlen, s);
	p = sym_hash (nodenameprefix);
	return (sym_check_node (sc, p, newn, newwidth));
}

void update_src_context (void)
{	current_src_context = (t_src_context *) smalloc (sizeof (t_src_context));
	current_src_context->filenamesym = input_file_name_sym;
	current_src_context->linenum = linenum;
	current_src_context->sc_id = -1;
}

void syntax_error (const char *s)
{	int i;

	fprintf (stderr, "file %s line %d: %s\n", input_file_name_sym->name, linenum, s);
	linebuf [char_in_line++] = '\0';
	fprintf (stderr, "%s\n", linebuf);
	for (i = 0; i < char_in_line - 1; i++)
	{	if (linebuf [i] == '\t')
			putc ('\t', stderr);
		else
			putc (' ', stderr);
	}
	fprintf (stderr, "^\n");
	if (prefixlen > 0)
	{	nodenameprefix [prefixlen] = '\0';
		fprintf (stderr, "context is block: %s\n", nodenameprefix);
	}
	else
	{	fprintf (stderr, "at top level of design\n");
	}
	if (file_depth > 0)
	{	fprintf (stderr, "file included from\n");
		for (i = file_depth - 1; i >= 0; i--)
		{	fprintf (stderr, "file %s line %d\n", input_file_name_stack [i], linenum_stack [i]);
		}
	}
	syntax_error_found++;
	statement_syntax_error_found++;
}

FILE *ifopen (const char *name, const char *mode)
{	static char fname [wordlen + max_include_name_len];
	FILE *f;
	int i;
    char msg [maxnamelen + 100];

	f = fopen (name, mode);
	for (i = 0; i < n_include_dirs && f == (FILE *) NULL; i++)
	{	(void) sprintf (fname, "%s/%s", include_dirs [i], name);
		f = fopen (fname, mode);
	}
	if (f == (FILE *) NULL)
	{	sprintf (msg, "can't open file name %s", name);
		syntax_error (msg);
	}
	return (f);
}

void add_include_dir (char *s)
{	char msg [maxnamelen + 100];
	char badname [max_include_name_len + 1];

	if (strlen (s) >= max_include_name_len)
	{	strncpy (badname, s, max_include_name_len);
		badname [max_include_name_len] = '\0';
		sprintf (msg, "include name %s is too long", badname);
		syntax_error (msg);
	}
	else if (n_include_dirs == max_include_dirs)
		syntax_error ("too many included directories");
	else
		strcpy (include_dirs [n_include_dirs++], s);
}

void get_token (void)
{	char c;
	int base;
	int i;
	int incomment;
	int iw;
	int haveinclude;
	symptr filesym;
	int unconditional_include;
	static char charbuf [maxtokenlength + 1];
	logic_value_word cin;
	logic_value_word w, wl, wh;
	char msg [maxnamelen + 100];

	incomment = 0;
	haveinclude = 1;
	while (haveinclude)
	{	haveinclude = 0;
		while ((c = getc (input_file)) == ' ' || c == '\t' || c == '\n' || incomment || c == '@')
		{	linebuf [char_in_line++] = c;
			if (c == '\n')
			{	linenum++;
				update_src_context ();
				char_in_line = 0;
				incomment = 0;
			}
			if (c == '@')
				incomment = 1;
		}
		if (c == '%' && char_in_line == 0)
		{
			c = getc (input_file);
			{
				if (c != '?')
				{	unconditional_include = 1;
					ungetc (c, input_file);
				}
				else
					unconditional_include = 0;
				if (file_depth < max_file_depth)
				{	/* next line knows what maxtokenlength is, but too lazy to fix it */
					if (fscanf (input_file, (char *)" %200s", charbuf) == 1)
					{

						filesym = sym_hash_tbl (charbuf, filesymboltable, filesymtablesize, true);
						if (unconditional_include || !filesym->file_included)
#ifdef use_fancy_hash_table
						if (unconditional_include || !file_included_hash.contains (charbuf))
#endif
						{
							filesym->file_included = true;
#ifdef use_fancy_hash_table

							if (!file_included_hash.contains (charbuf))
								file_included_hash.add (charbuf, false);
#endif
							file_stack [file_depth] = input_file;
							if ((input_file = ifopen (charbuf, (char *)"r")) == (FILE *) NULL)
							{	sprintf (msg, "unabled to open included file name %s", charbuf);
								syntax_error (msg);
								input_file = file_stack [file_depth];
							}
							else
							{	input_file_name_stack [file_depth] = input_file_name;
								input_file_name_sym_stack [file_depth] = input_file_name_sym;
								input_file_name_sym = sym_hash (charbuf);
								input_file_name_sym->is_filename = true;
								input_file_name = input_file_name_sym->name;
								linenum_stack [file_depth++] = linenum;
								linenum = 1;
								update_src_context ();
							}
						}
					}
					else
					{	syntax_error ("did not find a valid file name in include");
					}
					c = '\n';
				}
				else
				{	syntax_error ("include depth overflow\n");
					exit (2);
				}
				haveinclude = 1;
			}
		}
		if (feof (input_file) && file_depth > 0)
		{	fclose (input_file);
			file_depth--;
			linenum = linenum_stack [file_depth];
			input_file_name = input_file_name_stack [file_depth];
			input_file_name_sym = input_file_name_sym_stack [file_depth];
			input_file = file_stack [file_depth];
			update_src_context ();
			haveinclude = 1;
		}
	}
	linebuf [char_in_line++] = c;
	if (isalpha (c) || c == '_' || c == '!')
	{	i = 0;
		while (isalpha (c) || isdigit (c) || c == '_' || c == '!')
		{	charbuf [i++] = c;
			c = getc (input_file);
			linebuf [char_in_line++] = c;
		}
		if (c == '=' && i == 1 && charbuf [0] == '!')
		{	next_token = token_neq;
		}
		else
		{	charbuf [i] = '\0';
			ungetc (c, input_file);
			char_in_line--;
			next_token = token_symbol;
			next_token_sym = sym_hash (charbuf);
		}
	}
	else if (isdigit (c))
	{	base = 10;
		for (iw = 0; iw < max_node_words; iw++) {
			next_token_const_val.val_logic [iw] = 0;
		}
		if (c == '0')
		{	base = 8;
			c = getc (input_file);
			linebuf [char_in_line++] = c;
			if (c == 'x')
			{	base = 16;
				c = getc (input_file);
				linebuf [char_in_line++] = c;
			}
			else if (c == 'b')
			{	base = 2;
				c = getc (input_file);
				linebuf [char_in_line++] = c;
			}
		}
		while (isdigit (c) || (c >= 'a' && c <= 'f'))
		{	if (isdigit (c))
				cin = c - '0';
			else
				cin = c + 10 - 'a';
			c = getc (input_file);
			linebuf [char_in_line++] = c;
			for (iw = 0; iw < max_node_words; iw++)
			{   w = next_token_const_val.val_logic [iw];
				wl = w & halfword_all_bit_mask;
				wh = (w >> bits_per_halfword) & halfword_all_bit_mask;
				wl *= base;
				wl += cin;
				cin = wl >> bits_per_halfword;
				wl &= halfword_all_bit_mask;
				wh *= base;
				wh += cin;
				cin = wh >> bits_per_halfword;
				next_token_const_val.val_logic [iw] = (wh << bits_per_halfword) | wl;
			}
		}
		ungetc (c, input_file);
		char_in_line--;
		next_token_num = next_token_const_val.val_logic [0];
		next_token = token_num;
	}
	else if (feof (input_file))
	{	char_in_line--;
		next_token = token_EOF;
	}
	else switch (c)
	{	case '(':
			next_token = token_lpar;
			break;

		case ')':
			next_token = token_rpar;
			break;

		case '[':
			next_token = token_lsquare;
			break;

		case ']':
			next_token = token_rsquare;
			break;

		case '-':
			next_token = token_minus;
			break;

		case '#':
			next_token = token_pound;
			break;


		case '>':
			c = getc (input_file);
			linebuf [char_in_line++] = c;
			if (c == '=')
				next_token = token_geq;
			else if (c == '>')
				next_token = token_rsh;
			else
			{	ungetc (c, input_file);
				char_in_line--;
				next_token = token_greater;
			}
			break;
 
		case '<':
			c = getc (input_file);
			linebuf [char_in_line++] = c;
			if (c == '=')
				next_token = token_leq;
			else if (c == '<')
				next_token = token_lsh;
			else
			{	ungetc (c, input_file);
				char_in_line--;
				next_token = token_less;
			}
			break;
 
		case '+':
			next_token = token_plus;
			break;
 
		case '*':
			next_token = token_star;
			break;
 
		case '|':
			c = getc (input_file);
			linebuf [char_in_line++] = c;
			if (c == '|')
				next_token = token_concat;
			else
			{	ungetc (c, input_file);
				char_in_line--;
				next_token = token_or;
			}
			break;
 
		case '&':
			next_token = token_and;
			break;

		case '/':
			next_token = token_slash;
			break;

		case '^':
			next_token = token_up;
			break;
 
		case '{':
			next_token = token_lsquig;
			break;

		case '}':
			next_token = token_rsquig;
			break;
 
		case ';':
			next_token = token_semicolon;
			break;

		case '?':
			next_token = token_question;
			break;

		case ',':
			next_token = token_comma;
			break;

		case '=':
			c = getc (input_file);
			linebuf [char_in_line++] = c;
			if (c == '=')
				next_token = token_eqeq;
			else
			{	next_token = token_equal;
				ungetc (c, input_file);
				char_in_line--;
			}
			break;

		case '.':
			c = getc (input_file);
			linebuf [char_in_line++] = c;
			if (c == '.')
				next_token = token_dotdot;
			else
				syntax_error ("bad token: found a . but no second .");
			break;

		case ':':
			c = getc (input_file);
			linebuf [char_in_line++] = c;
			if (c == '+')
			{	c = getc (input_file);
				linebuf [char_in_line++] = c;
				if (c == ':')
					next_token = token_xor;
				else
					syntax_error ("bad token: looks like a :+: but not can has");
			}
			else if (c == '=')
			{	next_token = token_assign;
			}
			else
			{	ungetc (c, input_file);
				char_in_line--;
				next_token = token_colon;
			}
			break;
	}
	if (debug_parser)
		printf ("token %d\n", next_token);
}

void match_token (int tok)
{	char msg [100];

	if (tok != next_token)
	{	sprintf (msg, "expected a %s but found a %s", token_strings [tok], token_strings [next_token]);
		syntax_error (msg);
	}
	else
		get_token ();
}

namelistptr parse_namelist (int endtok)
{	namelistptr nl;
	namelistptr res;
	exprsynptr tmp_subscripts [max_subscripts];
	int idim;
	char msg [100];


	res = (namelistptr) NULL;
	while (next_token == token_symbol)
	{	if (res == NULL)
		{	nl = (namelistptr) smalloc (sizeof (t_namelist));
			res = nl;
		}
		else
		{	nl->next = (namelistptr) smalloc (sizeof (t_namelist));
			nl = nl->next;
		}
		nl->name = next_token_sym;
		nl->next = NULL;
		get_token ();
		nl->n_dims = 0;
		while (next_token == token_lsquare && nl->n_dims < max_subscripts)
		{	get_token ();
			tmp_subscripts [nl->n_dims] = parse_expr ();
			nl->n_dims++;
			match_token (token_rsquare);
		}
		if (nl->n_dims > 0)
		{	nl->subscripts = (exprsynptr *) smalloc (nl->n_dims * sizeof (exprsynptr));
			for (idim = 0; idim < nl->n_dims; idim++)
			{	nl->subscripts [idim] = tmp_subscripts [idim];
			}
		}
	}
	
	if (next_token != endtok)
	{	sprintf (msg, "expecting a %s but got a %s", token_strings [endtok], token_strings [next_token]);
		syntax_error (msg);
		get_token ();
	}
	return (res);
}

symptr parse_name (void)
{	symptr n;
	char msg [100];

	n = next_token_sym;
	if (next_token != token_symbol)
	{	sprintf (msg, "expected a name but found a %s", token_strings [next_token]);
		syntax_error (msg);
		return (sym_hash ("????"));
	}
	else
	{	get_token ();
		return (n);
	}
}


exprlistptr parse_exprlist (int end_token)
{	exprlistptr e;

	e = (exprlistptr) smalloc (sizeof (t_exprlist));
	e->expr = parse_expr ();
	e->next = (exprlistptr) NULL;
	err_ret (exprlistptr);
	if (next_token == end_token)
		get_token ();
	else
	{	if (next_token == token_comma)
			get_token ();
		if (! statement_syntax_error_found)
			e->next = parse_exprlist (end_token);
	}
	return (e);
}


exprsynptr parse_expr(void);

expr5synptr parse_expr5 (void)
{	expr5synptr e;
	expr5synptr e1;
	int num1, num2;
	int iw;
	exprsynptr esubs [max_subscripts];
	int i_sub;

	e = (expr5synptr) smalloc (sizeof (struct expr5syntax));
	e->src_context = current_src_context;

	if (next_token == token_minus)
	{	get_token ();
		e->expr = parse_expr ();
		e->kind = expr5_kind_neg;
	}
	else if (next_token == token_lpar)
	{	get_token ();
		e->expr = parse_expr ();
		e->kind = expr5_kind_expr;
		match_token (token_rpar);
	}
	else if (next_token == token_pound)
	{	get_token ();
		e->expr5 = parse_expr5 ();
		e->kind = expr5_kind_const;
	} else if (next_token == token_num)
	{	e->kind = expr5_kind_number;
		e->const_num = new_const_val_canonic (next_token_const_val.val_logic, max_words_per_logicval);
		get_token ();
	} else
	{	e->name = parse_name ();
		e->kind = expr5_kind_name;
		e->nsubscripts = 0;
		while (next_token == token_lsquare)
		{	get_token ();
			esubs [e->nsubscripts] = parse_expr ();
			match_token (token_rsquare);
			e->nsubscripts++;
		}
		if (e->nsubscripts > 0)
		{	e->subscript_expr = (exprsynptr *) smalloc (e->nsubscripts * sizeof (exprsynptr));
			for (i_sub = 0; i_sub < e->nsubscripts; i_sub++)
			{	e->subscript_expr [i_sub] = esubs [i_sub];
			}
		}
	}

	while (next_token == token_lsquig && !statement_syntax_error_found)
	{	e1 = (expr5synptr) smalloc (sizeof (t_expr5syntax));
		e1->src_context = current_src_context;
		e1->expr5 = e;
		e = e1;
		get_token ();
		e->sel_low_exp = parse_expr ();
		if (next_token == token_dotdot)
		{	get_token ();
			e->sel_high_exp = parse_expr ();
			e->kind = expr5_kind_expr_selrange;
		} else
		{   e->kind = expr5_kind_expr_selbit;
		}
		match_token (token_rsquig);
	}

	return (e);
}
expr4synptr parse_expr4 (void)
{	expr4synptr e;

	e = (expr4synptr) smalloc (sizeof (t_expr4syntax));
	e->src_context = current_src_context;

	if (next_token == token_slash)
	{	e->kind = expr4_kind_not;
		get_token ();
		e->expr4 = parse_expr4 ();
	}
	else
	{	e->kind = expr4_kind_expr5;
		e->expr5 = parse_expr5 ();
		err_ret (expr4synptr);
		if (next_token == token_star)
		{   e->kind = expr4_kind_star;
			get_token ();
			e->expr4 = parse_expr4 ();
		}
		else if (next_token == token_symbol && next_token_sym == sym_mod)
		{   e->kind = expr4_kind_mod;
			get_token ();
			e->expr4 = parse_expr4 ();
		}
		else if (next_token == token_symbol && next_token_sym == sym_div)
		{   e->kind = expr4_kind_div;
			get_token ();
			e->expr4 = parse_expr4 ();
		}
		else if (next_token == token_lpar && e->expr5->kind == expr5_kind_name)
		{	e->kind = expr4_kind_call;
			get_token ();
			e->exprlist = parse_exprlist (token_rpar);
		}
	}
	return (e);
}

expr3synptr parse_expr3 (void)
{	expr3synptr e;

	/* syntax is hard to handle, so parse an expr1, and convert to
	 * fun call if see a (
	 */
	e = (expr3synptr) smalloc (sizeof (t_expr3syntax));
	e->src_context = current_src_context;
	e->expr4 = parse_expr4 ();
	err_ret (expr3synptr);
	if (next_token == token_and)
	{	get_token ();
		e->kind = expr3_kind_and;
		e->expr3 = parse_expr3 ();
	}
	else
		e->kind = expr3_kind_expr4;
	return (e);
}

expr2synptr parse_expr2 (void)
{	expr2synptr e;

	e = (expr2synptr) smalloc (sizeof (t_expr2syntax));
	e->src_context = current_src_context;

	e->expr3 = parse_expr3 ();
	err_ret (expr2synptr);
	if (next_token == token_or)
	{	e->kind = expr2_kind_or;
		get_token ();
		e->expr2 = parse_expr2 ();
		return (e);
	}
	else if (next_token == token_xor)
	{	e->kind = expr2_kind_xor;
		get_token ();
		e->expr2 = parse_expr2 ();
		return (e);
	}
	else if (next_token == token_eqeq)
	{	e->kind = expr2_kind_eqeq;
		get_token ();
		e->expr2 = parse_expr2 ();
		return (e);
	}
	else if (next_token == token_lsh)
	{	e->kind = expr2_kind_lsh;
		get_token ();
		e->expr2 = parse_expr2 ();
		return (e);
	}
	else if (next_token == token_rsh)
	{	e->kind = expr2_kind_rsh;
		get_token ();
		e->expr2 = parse_expr2 ();
		return (e);
	}
	else if (next_token == token_concat)
	{	get_token ();
		if (next_token == token_lsquare)
		{	get_token ();
			e->sub_start = parse_expr ();
			match_token (token_dotdot);
			e->sub_end = parse_expr ();
			match_token (token_rsquare);
			e->kind = expr2_kind_concat_range;
		}
		else
		{	e->expr2 = parse_expr2 ();
			e->kind = expr2_kind_concat;
		}
		return (e);
	}
	else
	{	e->kind = expr2_kind_expr3;
		return (e);
	}
}

expr1synptr parse_expr1 (void)
{	expr1synptr e;
	expr1synptr enew;

	e = (expr1synptr) smalloc (sizeof (t_expr1syntax));
	e->src_context = current_src_context;
	e->expr2 = parse_expr2 ();
	e->kind = expr1_kind_expr2;
	while (next_token == token_plus || next_token == token_minus)
	{   enew = (expr1synptr) smalloc (sizeof (t_expr1syntax));
		enew->src_context = current_src_context;
		enew->expr1 = e;
		e = enew;
		if (next_token == token_plus)
		{	e->kind = expr1_kind_plus;
			get_token ();
			e->expr2 = parse_expr2 ();
		}
		else if (next_token == token_minus)
		{	e->kind = expr1_kind_minus;
			get_token ();
			e->expr2 = parse_expr2 ();
		}
	}
	return (e);
}

expr0synptr parse_expr0 (void)
{	expr0synptr e;

	e = (expr0synptr) smalloc (sizeof (t_expr0syntax));
	e->src_context = current_src_context;
	e->expr1a = parse_expr1 ();
	err_ret (expr0synptr);
	if (next_token == token_greater)
	{	e->kind = expr0_kind_greater;
		get_token ();
		e->expr1b = parse_expr1 ();
		return (e);
	}
	else if (next_token == token_less)
	{	e->kind = expr0_kind_less;
		get_token ();
		e->expr1b = parse_expr1 ();
		return (e);
	}
	else if (next_token == token_leq)
	{	e->kind = expr0_kind_leq;
		get_token ();
		e->expr1b = parse_expr1 ();
		return (e);
	}
	else if (next_token == token_geq)
	{	e->kind = expr0_kind_geq;
		get_token ();
		e->expr1b = parse_expr1 ();
		return (e);
	}
	else if (next_token == token_neq)
	{	e->kind = expr0_kind_neq;
		get_token ();
		e->expr1b = parse_expr1 ();
		return (e);
	}
	else if (next_token == token_equal)
	{	e->kind = expr0_kind_equal;
		get_token ();
		e->expr1b = parse_expr1 ();
		return (e);
	}
	else
	{	e->kind = expr0_kind_expr1;
		return (e);
	}
}

exprsynptr parse_expr_cond (bool allow_latch)
{	exprsynptr e;

	e = (exprsynptr) smalloc (sizeof (t_expr0syntax));
	e->src_context = current_src_context;
	e->expr0 = parse_expr0 ();
	err_ret (exprsynptr);
	if (next_token == token_up)
	{	get_token ();
		e->expr = parse_expr ();
		e->kind = expr_kind_clocked;
	}
	else if (next_token == token_question)
	{	get_token ();
		e->expr = parse_expr_cond (false);
		e->kind = expr_kind_3state;
		if (next_token == token_colon)
		{	get_token ();
			e->exprelse = parse_expr_cond (true);
			e->kind = expr_kind_cond;
		}
	}
	else if (allow_latch && next_token == token_colon)
	{	get_token ();
		e->expr = parse_expr_cond (true);
		e->kind = expr_kind_latched;
	}
	else
		e->kind = expr_kind_expr0;
	return (e);
}

exprsynptr parse_expr (void)
{	return parse_expr_cond (true);
}

progsynptr parse_prog (int end_delim, int rsquig_delim);

void parse_assign (defsynptr d)
{	d->lhs = parse_expr5 ();
	d->clk_expr = (expr2synptr) NULL;
	d->latch_expr = (expr2synptr) NULL;
	d->buf_expr = (expr2synptr) NULL;
	if (next_token == token_question)
	{	get_token ();
		d->buf_expr = parse_expr2 ();
	}
	if (next_token == token_up)
	{	get_token ();
		d->clk_expr = parse_expr2 ();
	}
	else if (next_token == token_colon)
	{	get_token ();
		d->latch_expr = parse_expr2 ();
	}
	if (next_token == token_equal)
	{	d->kind = def_kind_eqn;
		get_token ();
	}
	else if (next_token == token_assign)
	{   d->kind = def_kind_assign;
		get_token ();
	}
	else
	{	syntax_error ("expecting assignment operator");
	}

	d->expr = parse_expr ();
}

defsynptr parse_def (void)
{	defsynptr d;
	char msg [100];
	exprsynptr esubs [max_subscripts];
	int i_sub;

	d = (defsynptr) smalloc (sizeof (t_defsyntax));
	d->src_context = current_src_context;
	statement_syntax_error_found = 0;
	if (next_token == token_symbol && (next_token_sym == sym_def || next_token_sym == sym_macro))
	{	if (next_token_sym == sym_def)
			d->kind = def_kind_def;
		else
			d->kind = def_kind_macro;
		d->ninstances = 0;
		get_token ();
		d->name = parse_name ();
		match_token (token_lsquare);
		d->namelist = parse_namelist (token_rsquare);
		get_token ();
		if (next_token == token_greater)
		{	get_token ();
			d->returns = parse_name ();
		}
		else
			d->returns = NULL;
		d->body = parse_prog (1, 0);
		match_token (token_symbol);
		match_token (token_semicolon);
	}
	else if (next_token == token_symbol && (next_token_sym == sym_node || next_token_sym == sym_ionode))
	{	if (next_token_sym == sym_node)
			d->kind = def_kind_node;
		else
			d->kind = def_kind_ionode;
		get_token ();
		d->width_expr = parse_expr ();
		d->namelist = parse_namelist (token_semicolon);
		get_token ();
	}
	else if (next_token == token_symbol && next_token_sym == sym_logicval)
	{	d->kind = def_kind_logicvar;
		get_token ();
		d->namelist = parse_namelist (token_semicolon);
		get_token ();
	}
	else if (next_token == token_symbol && next_token_sym == sym_block)
	{	get_token ();
		d->kind = def_kind_block;
		d->nsubscripts = 0;
		d->name = parse_name ();
		while (next_token == token_lsquare)
		{	get_token ();
			esubs [d->nsubscripts] = parse_expr ();
			match_token (token_rsquare);
			d->nsubscripts++;
		}
		if (d->nsubscripts > 0)
		{	d->subscript_expr = (exprsynptr *) smalloc (d->nsubscripts * sizeof (exprsynptr));
			for (i_sub = 0; i_sub < d->nsubscripts; i_sub++)
			{	d->subscript_expr [i_sub] = esubs [i_sub];
			}
		}
		d->type = parse_name ();
		d->exprlist = parse_exprlist (token_semicolon);
	}
	else if (next_token == token_symbol && next_token_sym == sym_blk)
	{	get_token ();
		d->kind = def_kind_block;
		d->name = new_temp ();
		d->type = parse_name ();
		d->exprlist = parse_exprlist (token_semicolon);
	}
	else if (next_token == token_symbol && next_token_sym == sym_for)
	{	get_token ();
		d->kind = def_kind_for;
		if (next_token == token_lpar)
		{	get_token ();
			d->stmt1 = (defsynptr) smalloc (sizeof (t_defsyntax));
			parse_assign (d->stmt1);
			match_token (token_semicolon);
			d->expr = parse_expr ();
			match_token (token_semicolon);
			d->stmt2 = (defsynptr) smalloc (sizeof (t_defsyntax));
			parse_assign (d->stmt2);
			match_token (token_rpar);
			d->loop_body = parse_def ();
		}
	}
	else if (next_token == token_symbol && next_token_sym == sym_while)
	{	get_token ();
		d->kind = def_kind_while;
		if (next_token == token_lpar)
		{	get_token ();
			d->expr = parse_expr ();
			match_token (token_rpar);
			d->loop_body = parse_def ();
		}
	}
	else if (next_token == token_symbol && next_token_sym == sym_if)
	{	get_token ();
		d->kind = def_kind_if;
		if (next_token == token_lpar)
		{	get_token ();
			d->expr = parse_expr ();
			match_token (token_rpar);
			d->stmt1 = parse_def ();
			if (next_token == token_symbol && next_token_sym == sym_else)
			{	get_token ();
				d->stmt2 = parse_def ();
			}
			else
			{	d->stmt2 = NULL;
			}
		}
	}
	else if (next_token == token_lsquig)
	{	get_token ();
		d->kind = def_kind_compound;
		d->body = parse_prog (0, 1);
		match_token (token_rsquig);
	}
	else if (next_token == token_symbol)
	{	parse_assign (d);
		match_token (token_semicolon);
	}
	else
	{	sprintf (msg, "cannot parse the def, it starts with a %s", token_strings [next_token]);
		syntax_error (msg);
	}
	if (statement_syntax_error_found)
	{	while (next_token != token_EOF && next_token != token_semicolon)
			get_token ();
		get_token ();
	}
	return (d);
}

progsynptr parse_prog (int end_delim, int rsquig_delim)	/* end token or rsquig token terminates program */
{	progsynptr p, p1;

	p = NULL;
	while (next_token != token_EOF
				&& (! end_delim || next_token != token_symbol || next_token_sym != sym_end)
				&& (! rsquig_delim || next_token != token_rsquig))
	{	if (p == NULL)
		{	p = (progsynptr) smalloc (sizeof (t_progsyntax));
			p1 = p;
		}
		else
		{	p1->next = (progsynptr) smalloc (sizeof (t_progsyntax));
			p1 = p1->next;
		}
		p1->next = NULL;
		p1->def = parse_def ();
		if (syntax_error_found > max_syntax_errs)
		{	syntax_error ("too many errors");
			return (NULL);
		}
	}
	return (p);
}

void init_sym_table (void)
{	int i;

	for (i = 0; i < symtablesize; i++)
		symboltable [i] = NULL;


	input_file_name_sym = sym_hash ("<no file>");
	input_file_name = input_file_name_sym->name;

}

progsynptr scan_parse (char *s)
{	progsynptr p;
	int i;

	if ((input_file = ifopen (s, (char *)"r")) == NULL)
		return (NULL);

	init_token_strings ();

	input_file_name_sym = sym_hash (s);
	input_file_name_sym->is_filename = true;
	input_file_name = input_file_name_sym->name;
	linenum = 1;
	update_src_context ();


	/* init next_token_const_val and preallocate val_logic for it */

	next_token_const_val.n_val_bits = 0;
	next_token_const_val.sign_bit = 0;
	next_token_const_val.n_logicval_words = max_node_words;
	next_token_const_val.val_logic = (logic_value_word *) smalloc (max_node_words * sizeof (logic_value_word));


	get_token ();
	sym_def = sym_hash ("def");
	sym_node = sym_hash ("node");
	sym_block = sym_hash ("block");
	sym_blk = sym_hash ("blk");
	sym_end = sym_hash ("end");
	sym_ionode = sym_hash ("ionode");
	sym_macro = sym_hash ("macro");
	sym_for = sym_hash ("for");
	sym_while = sym_hash ("while");
	sym_if = sym_hash ("if");
	sym_else = sym_hash ("else");
	sym_logicval = sym_hash ("logicval");
	sym_div = sym_hash ("div");
	sym_mod = sym_hash ("mod");
	p = parse_prog (0, 0);
	fflush (stderr);
	return (p);
}
