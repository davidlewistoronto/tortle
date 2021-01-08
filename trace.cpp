/* copyright (c) David M. Lewis 1987 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>

#include "config.h"
#include "parser.h"
#include "tortle_types.h"
#include "sim.h"
#include "scan_parse.h"
#include "utils.h"
#include "describe.h"
#include "command.h"

#include "debug.h"

#define state_magic 0x127983fc

int make_tracefile;
FILE *tracefile;
FILE *tracenamefile;

int nfiletraced;
symptr *filetracednodes;

int trace_sample = 1;	/* how often to write trace */



void write_trace (void)
{	int i;
	nptr n;
	symptr s;
	unsigned *tptr;
	unsigned *tptr_end;
	logicval v;
	int iw;
	int k;
	int jw;

	iw = (current_tick / trace_sample) % bits_per_word;
	jw = ((current_tick / trace_sample) / bits_per_word) % wordsperclick;
	for (i = 0; i < nfiletraced; i++)
	{
		s = filetracednodes [i];
		n = s->nodedef;
		v = n->value;
		k = bitrange_width (n->node_size);
		tptr = s->tracebits + jw;
		tptr_end = tptr + k * wordsperclick;
		while (tptr != tptr_end)
		{	*tptr |= (v & 1) << iw;
			v >>= 1;
			tptr += wordsperclick;
		}
		if (iw == bits_per_word - 1 && jw == wordsperclick - 1)
		{	k = k * wordsperclick;
			if (fwrite ((char *) s->tracebits, sizeof (unsigned), k, tracefile) != (size_t) k)
			{	fprintf (stderr, "can't write trace file\n");
				exit (2);
			}
		}
	}
	if (iw == bits_per_word - 1 && jw == wordsperclick - 1)
	{
		for (i = 0; i < nfiletraced; i++)
		{
			s = filetracednodes [i];
			k = bitrange_width (s->nodedef->node_size) * wordsperclick;
			tptr = s->tracebits;
			tptr_end = tptr + k;
			while (tptr != tptr_end)
			*tptr++ = 0;
		}
		i = ftell (tracefile);
		fflush (tracefile);
		fseek (tracefile, (long) sizeof (int), 0);
		putw ((current_tick / trace_sample) + 1, tracefile);
		fflush (tracefile);
		fseek (tracefile, (long) i, 0);
	}
}



void flush_trace (void)
{	int i;
	symptr s;

	int k;
	int where;

	where = ftell (tracefile);
	if ((current_tick / trace_sample) % bits_per_word != 0 || ((current_tick / trace_sample) / bits_per_word) % wordsperclick)
	{	
		for (i = 0; i < nfiletraced; i++)
		{
			s = filetracednodes [i];
			k = bitrange_width (s->nodedef->node_size) * wordsperclick;
			if ((int) fwrite ((char *) s->tracebits, sizeof (unsigned), k, tracefile) != k)
			{	fprintf (stderr, "can't write trace file\n");
				exit (2);
			}
		}
		fflush (tracefile);
		fseek (tracefile, (long) sizeof (int), 0);
		putw ((current_tick / trace_sample) + 1, tracefile);
		fflush (tracefile);
		fseek (tracefile, (long) where, 0);
	}
}

void make_tracenames (int traceall)
{	int i;
	int j;
	int k;
	symptr s;
	int nsig;
	nptr n;
	symptr *ss;

	for (nfiletraced = 0, nsig = 0, i = 0; i < symtablesize; i++)
	{	for (s = symboltable [i]; s != (symptr) NULL; s = s->next)
		{	if ((n = s->nodedef) != (nptr) NULL && (traceall || s->nodedef->save_trace))
			{	nsig += bitrange_width (s->nodedef->node_size);
				nfiletraced++;
			}
		}
	}
	filetracednodes = (symptr *) smalloc (nfiletraced * sizeof (symptr));
	ss = filetracednodes;
	fprintf (tracenamefile, "%d binary\n", nsig);
	for (i = 0; i < symtablesize; i++)
	{	for (s = symboltable [i]; s != (symptr) NULL; s = s->next)
		{	if ((n = s->nodedef) != (nptr) NULL && (traceall || s->nodedef->save_trace))
			{	k = bitrange_width (n->node_size);
				if (k > 1)
				{	for (j = 0; j < k; j++)
					fprintf (tracenamefile, "%s#%d\n", s->name, j);
				}
				else
					fprintf (tracenamefile, "%s\n", s->name);
				s->tracebits = (unsigned *) smalloc (k * sizeof (unsigned));
				for (j = 0; j < k; j++)
					s->tracebits [j] = 0;
				*ss++ = s;
			}
		}
	}
}




