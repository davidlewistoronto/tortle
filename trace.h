#ifndef __TRACE_H
#define __TRACE_H

extern int trace_sample;
extern int make_tracefile;
extern FILE *tracefile;
extern FILE *tracenamefile;

void make_tracenames (int traceall);
void flush_trace (void);



#endif
 