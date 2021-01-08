/* copyright (c) David M. Lewis 1987 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <io.h>

#ifndef __BORLANDC__
#include <unistd.h>
#endif

#include "config.h"
#include "parser.h"
#include "tortle_types.h"
#include "logic.h"
#include "debug.h"
#include "utils.h"
#include "sim.h"
#include "trace.h"
#include "scan_parse.h"
#include "describe.h"
#include "mem_access.h"
#include "command.h"
#include "codegen.h"
#include "nethelp.h"
#include "compiled_runtime.h"
#include "codegen_netlist.h"



int node_wid_table [max_node_width + 1];

FILE *nodevalfile;

obj vector_node_table [max_vector_nodes];
int n_vector_node_table = 0;



typedef struct binder *bindptr;

typedef struct binder {
	bindptr next;
	char *name;
	char *def;
	} t_binding;


bindptr bindings = NULL;


int outputbaseshift = 4;
unsigned outputbase = 16;
bool outputbasebinary = true;

int pleasestop;

unsigned sim_interval = 100;

obj_ptr ram_obj;
int haveram = 0;
int ramstart = 0;
int ramend = 0;

int n_sims, n_sim_calls, n_activations, n_fanout_calls;


void makebinding (char *name, char *def)
{	bindptr b;

	b = (bindptr) smalloc (sizeof (t_binding));
	b->next = bindings;
	bindings = b;
	b->name = newstring (name);
	b->def = newstring (def);
}

void helpcmd (const char *cmd)
{	FILE *f;
	char hdrline [wordlen + 10];
	char *s;
	char line [1000];

	if ((s = getenv (envname)) == (char *) NULL)
	{	fprintf (stderr, "no environment variable %s\n", envname);
		return;
	}
	sprintf (line, helpfile, s);
	sprintf (hdrline, "###%s\n", cmd);
	if ((f = fopen (line, "r")) == NULL)
	{	printf ("can't open help file\n");
		return;
	}
	while (fgets (line, 1000, f) != NULL && strcmp (hdrline, line) && !pleasestop)
		;
	while (fgets (line, 1000, f) != NULL && strncmp (line, "###", 3) && !pleasestop)
		fputs (line, stdout);
	fclose (f);
}

void find_fanout_stats (void)
{	int i;
	int j;
	int fanout_count [100];
	int fanin_count [100];
	blistptr bp;
	blistptr bp1;
	for (i = 0; i < 100; i++)
	{	fanout_count [i] = 0;
		fanin_count [i] = 0;
	}
	for (bp = allblocklist; bp != (blistptr) NULL; bp = bp->next)
	{	/* only looks at fanout of output pin 0 */
		bp1 = bp->b->outcons[0] [0].node->uses;
		for (j = 0; j < 99 && bp1 != (blistptr) NULL; bp1 = bp1->next)
			j++;
		fanout_count [j]++;
		if (bp->b->ninputs > 99)
		{	fanin_count [99]++;
		} else
		{	fanin_count [bp->b->ninputs]++;
		}
	}
	printf ("fanout count\n");
	for (i = 0; i < 100; i++)
	{	if (fanout_count [i] > 0)
			printf ("%d: %d\n", i, fanout_count [i]);
	}
	printf ("fanin count\n");
	for (i = 0; i < 100; i++)
	{	if (fanin_count [i] > 0)
			printf ("%d: %d\n", i, fanin_count [i]);
	}
}

void command_loop_parser (char *input_string, FILE *input_file, int is_file, int inputnotfromtty, int prompt, int level);


void print_node_array (symptr s, bool print_detail, bool print_brief, bool print_events)
{	int iidx;
	nptr n;

	if (s->n_array_elems > 0)
	{	for (iidx = s->n_array_elems - 1; iidx >= 0 ; iidx--)
		{	print_node_array (s->array_elems [iidx], print_detail, print_brief, print_events);
		}
	}
	else
	{	if (s->nodedef != NULL)
		{	if (print_detail)
			{	describe_node (s);
			}
			else
			{
				n = s->nodedef;
				if (!print_brief || print_events)
					fprintf (nodevalfile, "%s =", s->name);
				fprintf (nodevalfile, " %s", print_logic_value_base (n->node_value, n->node_size.bit_high + 1));
				if (print_events)
				{	fprintf (nodevalfile, " %d", n->n_events);
				}
				if (!print_brief || print_events)
					fprintf (nodevalfile, "\n");
			}
		}
	}
}

void drive_and_set_node_value_array (symptr s, logic_value v, int deltadrive)
{	int iidx;
	nptr n;

	if (s->n_array_elems > 0)
	{	for (iidx = 0; iidx < s->n_array_elems; iidx++)
		{	drive_and_set_node_value_array (s->array_elems [iidx], v, deltadrive);
		}
	}
	else
	{	if (s->nodedef != NULL)
		{	drive_and_set_node_value (s, v, deltadrive);
		}
	}
}

void set_node_tracebit_array (symptr s, bool tracebit)
{	int iidx;
	nptr n;

	if (s->n_array_elems > 0)
	{	for (iidx = 0; iidx < s->n_array_elems; iidx++)
		{	set_node_tracebit_array (s->array_elems [iidx], tracebit);
		}
	}
	else
	{	if (s->nodedef != NULL)
		{	s->nodedef->traced = tracebit;
			if (tracebit && strcmp (s->name, s->nodedef->name))
				printf ("note: node %s is aliased to %s\n", s->name, s->nodedef->name);
		}
	}
}


void do_command (char *line, int level)
{	int i;
	int j;
	int k;
	nptr n;
	obj_ptr s;
	obj_ptr s1;
	bindptr bp;
	bptr b;
	int nwords;
	char tmpline [max_line_len];
	char words [maxwords] [wordlen];
	char *plotspice_argv [maxwords];
	FILE *f;
	union {
		float f;
		unsigned i;
	} x;
	FILE *vectorfile;
	char *cp;
	char *rcp;
	unsigned v;
	unsigned vmask;
	bool vec_mismatch;
	int iw;
	logic_value_word vw [max_node_words];
	int vflag;
	int istart;
	int fmt_width;
	int fmt_shift;
	char fmt_char;
	int n_vec_errs;
	bool match_name_exact;
	bool match_name_start;
	symptr sym_match_list;
	bool tracebit;


	if (debug_echo_cmd)
		printf ("about to exec %s\n", line);
	switch (line [0])
	{	case '\'':
			getwords (line + 1, words, &nwords, maxwords);
			for (i = 0; i < nwords; i++)
			{	for (bp = bindings; bp != NULL && strcmp (bp->name, words [i]); bp = bp->next)
					;
				printf ("%%%s = ", words [i]);
				if (bp != NULL)
					printf ("%s", bp->def);
				printf ("\n");
			}
			break;

		case 'b':
			match_name_exact = true;
			match_name_start = false;
			if (line [1] == '*' || line [1] == '?')
			{	getwords (line + 2, words, &nwords, maxwords);
				match_name_exact = false;
				if (line [1] == '?')
					match_name_start = true;
			}
			else
			{	getwords (line + 1, words, &nwords, maxwords);
			}
			for (i = 0; i < nwords; i++)
			{	s = get_sym_matches (words [i], match_name_exact, match_name_start);
				if (match_name_exact && (s == NULL || s->blockdef == NULL))
				{	printf ("no such block: %s\n", words [i]);
				}
				else
				{	while (s != NULL)
					{	if (s->blockdef != NULL)
						{	describe_block (s);
						}
						s = s->next_match;
					}
				}
			}
			break;

		case 'B':
			i = atoi (line + 1);
			outputbase = i;
			outputbasebinary = ((i | (i - 1)) == (i + i - 1));
			for (outputbaseshift = 1; i > 2 && i <= 16; i >>= 1, outputbaseshift++)
				;
			init_print_tables ();
			break;

		case 'c':
			getwords (line + 1, words, &nwords, maxwords);
			if (nwords == 2)
				ramstart = abtoi (words [0]);
			if (haveram)
				s = ram_obj;
			else
			{	printf ("c command needs a block\n");
				break;
			}
			if (ramstart < 0)
				ramstart = 0;
			changeram (s, &ramstart, words [nwords - 1]);
			break;

		case 'C':
			if (net_read)
			{	codegen_net ();
			}
			else
			{	printf ("read the net first\n");
			}
			break;

		case 'd':
			getwords (line + 1, words, &nwords, maxwords);
			if (nwords >= 1)
				ramstart = abtoi (words [0]);
			if (nwords >= 2)
				ramend = abtoi (words [1]);
			else
				ramend = ramstart + 1;
			if (nwords >= 3)
			{	s = find_obj (words [2]);
				if (s == NULL || ! obj_has_block (s))
				{	printf ("no such block %s\n", words [2]);
					break;
				}
			}
			else if (haveram)
				s = ram_obj;
			else
			{	printf ("d command needs a block\n");
				break;
			}
			if (ramstart < 0)
				ramstart = 0;
			printf ("ram %s\n", s->name);
			dumpram (s, ramstart, ramend);
			haveram = 1;
			ram_obj = s;
			break;


		case 'D':
			getwords (line + 1, words, &nwords, maxwords);
			for (i = 0; i < nwords; i++)
				add_include_dir (words [i]);
			break;


		case 'e':
			getwords (line + 1, words, &nwords, maxwords);
			if (nwords >= 1)
				ramstart = abtoi (words [0]);
			if (nwords >= 2)
			{	s = find_obj (words [1]);
				if (s == NULL || ! obj_has_block (s))
				{	printf ("no such block: %s\n", words [1]);
					break;
				}
			}
			else if (haveram)
				s = ram_obj;
			else
			{	printf ("e command needs a block\n");
				break;
			}
			if (ramstart < 0)
				ramstart = 0;
			examineram (s, &ramstart);
			haveram = 1;
			ram_obj = s;
			break;

		case 'E':
#ifdef countbits
			printf ("%g node events, %g block evals\n", nnodeevents, nblockevals);
#endif
#ifdef countbitwidths
			printf ("%g node bit events, %g block bit evals\n", nnodebitevents, nblockbitevals);
#endif
			match_name_exact = true;
			match_name_start = false;
			if (line [1] == '*' || line [1] == '?')
			{	getwords (line + 2, words, &nwords, maxwords);
				match_name_exact = false;
				if (line [1] == '?')
					match_name_start = true;
			}
			else
			{	getwords (line + 1, words, &nwords, maxwords);
			}
			for (i = 0; i < nwords; i++)
			{	s = get_sym_matches (words [i], match_name_exact, match_name_start);
				if (match_name_exact && (s == NULL || sym_array_elem_0 (s)->nodedef == NULL))
				{	printf ("no such node: %s\n", words [i]);
				}
				else
				{	while (s != NULL)
					{	if (sym_array_elem_0 (s)->nodedef != NULL)
						{	print_node_array (s, false, true, true);
						}
						s = s->next_match;
					}
				}
			}

			break;

		case 'f':
			if (make_tracefile)
				fprintf (stderr, "tracing already on\n");
			else
			{	match_name_exact = true;
				match_name_start = false;
				if (line [1] == '*' || line [1] == '?')
				{	getwords (line + 2, words, &nwords, maxwords);
					match_name_exact = false;
					if (line [1] == '?')
						match_name_start = true;
				}
				else
				{	getwords (line + 1, words, &nwords, maxwords);
				}
				for (i = 0; i < nwords; i++)
				{	s = get_sym_matches (words [i], match_name_exact, match_name_start);
					if (match_name_exact && (s == NULL || s->nodedef == NULL))
					{	printf ("no such node: %s\n", words [i]);
					}
					while (s != NULL)
					{	if (s->nodedef != NULL)
						{	s->nodedef->save_trace = true;
							if (strcmp (s->name, s->nodedef->name))
								printf ("note: node %s is aliased to %s\n", s->name, s->nodedef->name);
						}
						s = s->next_match;
					}
				}
			}
			break;


		case 'g':
			getwords (line + 1, words, &nwords, maxwords);
#ifdef compiled_runtime
			if (nwords == 0)
			{	simulate_domain (0);
			} else
			{	s = find_obj (words [0]);
				if (s == NULL || !obj_has_node (s) || s->nodedef->clock_id == -1)
					printf ("no such clock domain %s\n", words [0]);
				else
					simulate_domain (s->nodedef->clock_id);
			}
#else
			if (nwords == 1)
				simulate (axtoi (words [0]));
			else
				simulate (sim_interval);
#endif
			break;

		case 'i':
			sim_interval = axtoi (line + 1);
			break;


		case 'I':
			print_io_nodes ();
			break;

		case 'l':
			getwords (line + 1, words, &nwords, maxwords);
			if (nwords < 0 || nwords > 3)
			{	printf ("bad l\n");
				break;
			}
			if (nwords >= 2)
			{	s = find_obj (words [0]);
				if (s == NULL || ! obj_has_block (s))
				{	printf ("no such block: %s\n", words [0]);
					break;
				}
			}
			else if (haveram)
				s = ram_obj;
			else
			{	printf ("l command needs a block\n");
				break;
			}
			romload (s, words [nwords - 1]);
			haveram = true;
			ram_obj = s;
			break;

		case 'm':
			report_multi_driven_nodes ();
			break;

		case 'n':
			match_name_exact = true;
			match_name_start = false;
			if (line [1] == '*' || line [1] == '?')
			{	getwords (line + 2, words, &nwords, maxwords);
				match_name_exact = false;
				if (line [1] == '?')
					match_name_start = true;
			}
			else
			{	getwords (line + 1, words, &nwords, maxwords);
			}
			for (i = 0; i < nwords; i++)
			{	s = get_sym_matches (words [i], match_name_exact, match_name_start);
				if (match_name_exact && (s == NULL || sym_array_elem_0 (s)->nodedef == NULL))
				{	printf ("no such node: %s\n", words [i]);
				}
				else
				{	while (s != NULL)
					{	if (sym_array_elem_0 (s)->nodedef != NULL)
						{	print_node_array (s, true, false, false);
						}
						s = s->next_match;
					}
				}
			}
			break;

		case 'o':
		case 'O':
		case 'p':
			getwords (line + 1, words, &nwords, maxwords);
			if (nwords >= 1)
			{	if ((tracefile = fopen (words [0], "w")) == (FILE *) NULL)
				{	printf ("can't open file %s\n", words [0]);
					break;
				}
				make_tracefile = 1;
				putw (current_tick / trace_sample, tracefile);
				putw (current_tick / trace_sample, tracefile);
				fflush (tracefile);
				if (nwords >= 2)
				{	if ((tracenamefile = fopen (words [1], "w")) == (FILE *) NULL)
					{	printf ("can't open file %s\n", words [1]);
						break;
					}
					else
					{	make_tracenames (line [0] == 'O' || line [0] == 'p');
						fclose (tracenamefile);
					}
					if (line [0] == 'p')
					{
#ifdef plotspice
						plotspice_pid = fork ();
						if (plotspice_pid == -1)
						{	printf ("can't fork\n");
						}
						else if (plotspice_pid == 0)
						{	if ((cp = getenv (envname)) == (char *) NULL)
							{	printf ("no environment variable %s\n", envname);
								exit (1);
							}
							sprintf (words [2], "%s/spice/plotspice", cp);
							plotspice_argv [0] = words [2];
							plotspice_argv [1] = "-X";
							plotspice_argv [2] = "-W";
							plotspice_argv [3] = "80";
							plotspice_argv [4] = "50";
							plotspice_argv [5] = "-I";
							plotspice_argv [6] = "-n";
							plotspice_argv [7] = words [1];
							plotspice_argv [8] = words [0];
							f = fopen (words [1], "r");
							fgets (tmpline, max_line_len, f);
							for (i = 9; i < maxwords - 1 && fgets (words [i], max_line_len, f) != (char *) NULL; i++)
							{	plotspice_argv [i] = words [i];
								words [i] [strlen (words [i]) - 1] = '\0';
							}
							plotspice_argv [i] = (char *) NULL;
							execv (plotspice_argv [0], plotspice_argv);
							printf ("can't exec %s\n", plotspice_argv [0]);
							exit (1);
						}
#endif
					}
				}
			}
			else
				printf ("need a trace file name\n");
			break;


		case 'P':
			getwords (line + 1, words, &nwords, maxwords);
			if (nwords == 1)
				(void) scan_parse (words [0]);
			fflush (stdout);
			fflush (stderr);
			break;

		case 'q':
			exit (0);

		case 'r':
#ifndef compiled_runtime
			getwords (line + 1, words, &nwords, maxwords);
			if (nwords == 1)
				init_net (words [0]);
			fflush (stdout);
			fflush (stderr);
#endif
			break;

		case 'R':
			trace_sample = atoi (line + 1);
			break;

		case 's':
			getwords (line + 1, words, &nwords, maxwords);
			if (nwords != 1)
			{	printf ("bad s\n");
				break;
			}
			s = find_obj (words [0]);
			if (s == NULL || ! obj_has_block (s))
			{	printf ("no such block: %s\n", words [0]);
				break;
			}
			haveram = 1;
			ram_obj = s;
			printf ("set ram to %s\n", words [0]);
			break;

		case 't':
		case 'u':
			if (line [0] == 't')
				tracebit = true;
			else
				tracebit = false;
			match_name_exact = true;
			match_name_start = false;
			if (line [1] == '*' || line [1] == '?')
			{	getwords (line + 2, words, &nwords, maxwords);
				match_name_exact = false;
				if (line [1] == '?')
					match_name_start = true;
			}
			else
			{	getwords (line + 1, words, &nwords, maxwords);
			}
			for (i = 0; i < nwords; i++)
			{	s = get_sym_matches (words [i], match_name_exact, match_name_start);
				if (match_name_exact && (s == NULL || sym_array_elem_0 (s)->nodedef == NULL))
				{	printf ("no such node: %s\n", words [i]);
				}
				else
				{	while (s != NULL)
					{	if (sym_array_elem_0 (s)->nodedef != NULL)
						{	set_node_tracebit_array (s, tracebit);
						}
						s = s->next_match;
					}
				}
			}
			break;

		case 'v':
			getwords (line + 1, words, &nwords, maxwords);
			for (i = 0; i < nwords; i++)
			{	s = find_obj (words [i]);
				if (s == NULL || ! obj_has_node (s))
				{	printf ("no such node: %s\n", words [i]);
				}
				else if (n_vector_node_table == max_vector_nodes)
					printf ("too many vector nodes\n");
				else
					vector_node_table [n_vector_node_table++] = *s;

			}
			break;

		case 'V':
			getwords (line + 1, words, &nwords, maxwords);
			if (nwords < 1)
			{	printf ("bad v\n");
				break;
			}
			if ((vectorfile = fopen (words [0], "r")) == (FILE *) NULL)
			{	printf ("can't open vector file %s\n", words [0]);
				break;
			}
			if (nwords == 2)
			{	for (bp = bindings; bp != NULL && strcmp (bp->name, words [1]); bp = bp->next)
					;
				if (bp == NULL)
				{	printf ("no such macro %s\n", words [1]);
					break;
				}
			}
			n_vec_errs = 0;
			for (i = 1; n_vec_errs < 10 && fgets (line, max_line_len, vectorfile) != (char *) NULL && !pleasestop; i++)
			{	for (j = 0, cp = line; j < n_vector_node_table && *cp != '\n' && *cp != '\0'; j++)
				{	while (*cp == ' ' || *cp == '\t')
						cp++;
					if (*cp == 'v')
					{	vflag = 1;
						cp++;
					}
					else
						vflag = 0;
					rcp = tmpline;
					while ((*cp >= '0' && *cp <= '9') || (*cp >= 'a' && *cp <= 'f') || *cp == 'x' || *cp == 'X')
					{	*rcp++ = *cp++;
					}
					*rcp = '\0';
					if (*cp == ',')
						cp++;
					if (!vflag)
					{	ascii_to_logic_value (tmpline, vw);
						drive_and_set_node_value (&(vector_node_table [j]), vw, 0);
					}
				}
				if (j < n_vector_node_table)
				{	printf ("vector file line %d, not enough input values\n", i);
					n_vec_errs++;
				} else if (*cp != '\n')
				{	printf ("vector file line %d, too many input values\n", i);
					n_vec_errs++;
				}
				if (nwords == 2)
					command_loop_parser (bp->def, (FILE *) NULL, 0, 1, 0, level + 1);
				else
				{
#ifdef compiled_runtime
					simulate_domain (0);
#else
					simulate (sim_interval);
#endif
				}
				for (j = 0, cp = line; !pleasestop && j < n_vector_node_table && *cp != '\n' && *cp != '\0'; j++)
				{	while (*cp == ' ' || *cp == '\t')
						cp++;
					if (*cp == 'v')
					{	vflag = 1;
						cp++;
					}
					else
					{	vflag = 0;
					}
					rcp = tmpline;
					while ((*cp >= '0' && *cp <= '9') || (*cp >= 'a' && *cp <= 'f') || *cp == 'x' || *cp == 'X')
					{	*rcp++ = *cp++;
					}
					*rcp = '\0';
					if (*cp == ',')
						cp++;
#ifndef compiled_runtime
					if (vflag)
					{	n = vector_node_table [j].nodedef;
						ascii_to_logic_value (tmpline, vw);
						vec_mismatch = false;
						for (iw = 0; iw < n->node_size_words; iw++)
						{	if (n->node_value [iw] != vw [iw])
							{	vec_mismatch = true;
							}
						}
						if (!pleasestop && vec_mismatch)
						{	printf ("\nvector_file line %d, node %s is %s, should be %s\n",
								i, n->name, print_logic_value_base (n->node_value, n->node_size.bit_high + 1),
								print_logic_value_base (vw, n->node_size.bit_high + 1));
							n_vec_errs++;
						}
					}
#endif
				}
				if (i % 10000 == 0)
				{	printf ("...%d", i);
					if (i % 100000 == 0)
						putchar ('\n');
					fflush (stdout);
				}
			}
			putchar ('\n');
			n_vector_node_table = 0;
			fclose (vectorfile);
			break;


		case 'W':
			for (i = 0; i <= max_node_width; i++)
				node_wid_table [i] = 0;
			for (n = allnodelist; n != (nptr) NULL; n = n->hnext)
				node_wid_table [bitrange_width (n->node_size)]++;
			for (i = 0; i <= max_node_width; i++)
				printf ("%d %d\n", i, node_wid_table [i]);
			break;

		case 'y':
			for (i = 1; line [i] == ' '; i++)
				;
			printf ("%s", line + i);
			fflush (stdout);
			break;

		case 'z':
			if (line [1] == 'f')
			{	getwords (line + 2, words, &nwords, maxwords);
				debugfile = fopen (words [0], "wb");
				have_debugfile = true;
			} else {
				i = axtoi (line + 1);
				if (i > 0 && i < ndebugs)
					debug [i - 1] = 1;
				else if (i < 0 && i > -ndebugs)
					debug [1 - i] = 0;
				else
				{   for (i = 0; i < ndebugs; i++)
					{   debug [i] = 1 - debug [i];
					}
				}
			}

			break;

		case 'Z':
			getwords (line + 2, words, &nwords, maxwords);
			if (nwords >= 2)
			{	s = find_obj (words [0]);
				if (s == NULL || ! obj_has_block (s))
				{	printf ("no such block: %s\n", words [0]);
				}
				else
					s->blockdef->debugblock = (bool) axtoi (words [1]);
			}
			else
				printf ("bad Z\n");
			break;

		case '!':
			find_fanout_stats ();
			break;

		case '@':
			if (line [1] == '>')
			{	getwords (line + 2, words, &nwords, maxwords);
				if (nwords < 1)
					fprintf (stderr, "missing file name in @>\n");
				else if ((nodevalfile = fopen (words [0], "w")) == (FILE *) NULL)
					fprintf (stderr, "can't open file\n");
			}
			else
			{	k = 0;
				if (line [1] == '@')
				{	k = 1;
				}
				match_name_exact = true;
				match_name_start = false;
				if (line [k + 1] == '*' || line [k + 1] == '?')
				{	getwords (line + k + 2, words, &nwords, maxwords);
					match_name_exact = false;
					if (line [1] == '?')
						match_name_start = true;
				}
				else
				{	getwords (line + k + 1, words, &nwords, maxwords);
				}
				istart = 0;
				if (words [0] [0] == '%')
				{	istart = 1;
					fmt_width = 1;
					fmt_shift = 0;
					fmt_char = 'f';
					sscanf (words [0], "%*c%d.%d%c", &fmt_width, &fmt_shift, &fmt_char);
				}
				for (i = istart; i < nwords; i++)
				{	s = get_sym_matches (words [i], match_name_exact, match_name_start);
					if (match_name_exact && (s == NULL || sym_array_elem_0(s)->nodedef == NULL))
					{	printf ("no such node: %s\n", words [i]);
					}
					while (s != NULL)
					{	print_node_array (s, false, (bool) (k != 0), false);
						s = s->next_match;
					}
				}
#ifdef fooooo
					if (s == NULL || !obj_has_node (s))
					{	printf ("no such node: %s\n", words [i]);
					}
					else
					{
						n = s->nodedef;
						if (!k)
							fprintf (nodevalfile, "%s =", n->name);
						fprintf (nodevalfile, " %s", print_logic_value_base (n->node_value, n->node_size.bit_high + 1));
						if (istart == 1)
						{	if (fmt_char == 'u')			/* unsigned */
								printf (" = %16.10g", (double) n->node_value [0] / (double) (1 << fmt_shift));
							else if (fmt_char == 'b')		/* excess-n bias */
								printf (" = %16.10g", ((double) ((int) n->node_value [0] - ((int) 1 << (fmt_width - 1)))) / (double) (1 << fmt_shift));
							else							/* signed */
								printf (" = %16.10g", (double) ((((int) (n->node_value [0])) << (bits_per_word - fmt_width)) >>
									(bits_per_word - fmt_width)) / (double) (1 << fmt_shift));
						}
						if (!k)
							fprintf (nodevalfile, "\n");
					}
#endif
				if (k)
					fprintf (nodevalfile, "\n");
			}
			break;

		case '0':
		case '1':
		case '-':
			match_name_exact = true;
			match_name_start = false;
			if (line [1] == '*' || line [1] == '?')
			{	getwords (line + 2, words, &nwords, maxwords);
				match_name_exact = false;
				if (line [1] == '?')
					match_name_start = true;
			}
			else
			{	getwords (line + 1, words, &nwords, maxwords);
			}
			if (nwords < 1 || (nwords < 2 && line [0] == '-'))
			{	printf ("bad -\n");
				break;
			}
			if (line [0] == '-')
			{	i = 1;
				ascii_to_logic_value (words [0], vw);
			}
			else
			{	i = 0;
				if (line [0] == '0')
				{   for (iw = 0; iw < max_node_words; iw++)
					{   vw [iw] = 0;
					}
				}
				else if (line [0] == '1')
				{   for (iw = 0; iw < max_node_words; iw++)
					{   vw [iw] = ~0;
					}
				}
			}
			for (; i < nwords; i++)
			{	s = get_sym_matches (words [i], match_name_exact, match_name_start);
				if (match_name_exact && (s == NULL || sym_array_elem_0 (s)->nodedef == NULL))
				{	printf ("no such node: %s\n", words [i]);
				}
				else
				{	while (s != NULL)
					{	if (sym_array_elem_0 (s)->nodedef != NULL)
						{	drive_and_set_node_value_array (s, vw, 0);
						}
						s = s->next_match;
					}
				}
			}
			break;

		case '?':
			getwords (line + 1, words, &nwords, maxwords);
			if (nwords != 0)
				helpcmd (words [0]);
			else
				helpcmd ("general info");
			break;


		default:
			if (line [0] != '\n')
				printf ("unknown command: %s\n", line);
			break;
	}
}


#define nextinputc() (is_file?getc(input_file):*input_string++)
#define eofinput() (is_file?feof(input_file):*(input_string-1)=='\0')

void command_loop_parser (char *input_string, FILE *input_file, int is_file, int inputnotfromtty, int prompt, int level)
{	int i;
	int ilinelen;
	int j;
	int k;
	int more;
	bindptr b;
	int somenum;
	FILE *loadfile;
	char iline [1002];
	char fname [200];
	char words [maxwords] [wordlen];
	int n_stop_nodes;
	unsigned init_node_value [maxwords];
	obj_ptr s;
	int iterate_stop;
	obj_ptr stop_nodes [maxwords];
	int printprompt;

	more = 1;
	printprompt = 1;
	while (more)
	{	if (level != 0 && pleasestop)
			return;
		if (level == max_command_loop_depth)
		{	printf ("command recursion too deep\n");
			return;
		}
		do
		{	if (prompt && printprompt && not_silent)
			{	printf (": ");
				fflush (stdout);	/* just in case not a tty */
			}
			/* this is gross */
			for (ilinelen = 0; ilinelen < 1000 && (iline [ilinelen] = nextinputc()) != '\n' &&
				!eofinput() && (iline [ilinelen] != ';' || iline [0] == '=' || iline [0] == '#' || iline [0] == '*') &&
				  iline [ilinelen] != '\0'; ilinelen++)
			{	if (ilinelen == 0 && isspace (iline [0]))	/* trash leading white space */
					ilinelen--;
			}
			if (eofinput ())
				more = 0;
		}
		while (iline [0] == '#');
		if (ilinelen == 0 && eofinput ())
		{	if (prompt && not_silent)
				printf ("\n");
			return;
		}
		if (iline [ilinelen] != '\n' && iline [ilinelen] != ';')
			iline [ilinelen] = '\n';
		if (iline [ilinelen] == '\n')
			printprompt = 1;
		else
			printprompt = 0;
		iline [ilinelen + 1] = '\0';
		if (debug_echo_cmd)
			printf ("parse command %s\n", iline);
		if (inputnotfromtty && prompt && not_silent)
		{	printf ("%s", iline);
			fflush (stdout);
		}
		if (level == 0)
			pleasestop = 0;
		if (iline [0] == '<')
		{	if (sscanf (iline + 1, " %s", fname) < 1)
				printf ("bad <\n");
			else if ((loadfile = ifopen (fname, "r")) == NULL)
				printf ("can't open %s\n", fname);
			else
			{	command_loop_parser ((char *)NULL, loadfile, 1, 1, 0, level + 1);
				fclose (loadfile);
			}
		}
		else if (iline [0] == '=')
		{	if (iline [ilinelen] == '\n')
				iline [ilinelen] = '\0';
			printprompt = 1;
			for (i = 1; iline [i] == ' '; i++)
				;
			for (j = i; iline [j] != '\0' && iline [j] != ' '; j++)
				;
			if (iline [j] == ' ')
			{	while (iline [j] == ' ')
					iline [j++] = '\0';
				makebinding (iline + i, iline + j);
			}
		}
		else if (iline [0] == '*')
		{	printprompt = 1;
			for (i = 1; iline [i] == ' '; i++)
				;
			for (j = i; isdigit (iline [j]); j++)
				;
			while (iline [j] == ' ')
				j++;
			somenum = axtoi (iline + i);
			n_stop_nodes = 0;
			if (index (iline, '{') != NULL && index (iline, '}') != NULL)
			{	j = index (iline, '}') - iline + 1;
				iline [j - 1] = '\0';
				getwords (index (iline, '{') + 1, words, &n_stop_nodes, maxwords);
				for (i = 0; i < n_stop_nodes; i++)
				{	s = find_obj (words [i]);
					if (s == NULL || !obj_has_node (s))
					{	printf ("iteration cancelled because no such node: %s\n", words [i]);
						somenum = 0;
					}
					else
					{	stop_nodes [i] = s;
						init_node_value [i] = stop_nodes [i]->nodedef->value;
					}
				}
			}
			iterate_stop = 0;
			for (i = 0; i < somenum && !iterate_stop && !pleasestop; i++)
			{	command_loop_parser (iline + j, (FILE *)NULL, 0, 1, 0, level + 1);
				for (k = 0; k < n_stop_nodes && !iterate_stop; k++)
				{	if (stop_nodes [k]->nodedef->value != init_node_value [k])
					{	printf ("terminate at iteration %d, t = %d, because node %s was %s <- %s\n",
							i, current_tick, stop_nodes [k]->name,
							print_masked_val (init_node_value [k], stop_nodes [k]->nodedef->node_size.bit_high + 1),
							print_masked_val (stop_nodes [k]->nodedef->value, stop_nodes [k]->nodedef->node_size.bit_high + 1));
						iterate_stop = 1;
					}
				}
			}
			if (pleasestop)
				printf ("*** iteration terminated at %d repetitions\n", i);
		}
		else if (iline [0] == '%')
		{	for (i = 1; i < ilinelen && !isspace (iline [i]); i++)
				;
			iline [i] = '\0';
			for (b = bindings; b != NULL && strcmp (b->name, iline + 1); b = b->next)
				;
			if (b != NULL)
				command_loop_parser (b->def, (FILE *) NULL, 0, 1, 0, level + 1);
			else
				printf ("no such macro %s\n", iline + 1);
		}
		else
		{	iline [strlen (iline) - 1] = '\n';	/* make it easy to scanf it */
			for (i = 0; iline [i] == ' '; i++)
				;
			do_command (iline + i, level);
		}
		if (level == 0 && make_tracefile)
			flush_trace ();
	}
}


void command_loop (void)
{	int inputnotfromtty;

	inputnotfromtty = (isatty (fileno (stdin)) == 0);

//	if (inputnotfromtty) {
//		printf ("no tty\n");
//	}

	command_loop_parser ((char *)NULL, stdin, 1, inputnotfromtty, 1, 0);
}


