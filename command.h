#ifndef __COMMAND_H
#define __COMMAND_H

#include "tortle_types.h"

void command_loop (void);

extern int pleasestop;
extern int outputbaseshift;
extern unsigned outputbase;
extern bool outputbasebinary;

extern FILE *nodevalfile;


#endif