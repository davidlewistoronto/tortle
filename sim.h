#ifndef __SIM_H
#define __SIM_H

#include "tortle_types.h"

void write_trace (void);

extern int flatten_net;
extern bool not_silent;
extern nptr *activenodes;
extern bptr *activeblocks;
extern int nactivenodes;
extern int nblocks, nnodes;
extern int nbitblocks, nbitnodes;
extern nptr allnodelist;
extern blistptr allblocklist;
extern double nnodeevents;
extern double nnodebitevents;
extern double nblockevals;
extern double nblockbitevals;
extern int nmultidrivenodes;
extern int net_read;

extern symptr block_false, block_true, node_const_input;


extern int current_tick;

extern int plotspice_pid;

extern int pleasestop;

void drive_node (nptr n, logicval v, unsigned mask, int deltadrive);
void drive_and_set_node (obj_ptr s, logicval v, int deltadrive);
void print_io_nodes (void);
void init_driver_counts (void);
void check_output_next (bptr blk, int iout, int delta);
void sched_block_output (bptr blk, int iout, logicval nv, int delta);
void sched_block_output_next (bptr blk, int iout, int delta);
void drive_and_set_node_value (obj_ptr s, logic_value v, int deltadrive);
void insert_shifted_bits (logic_value dst, int n_logic_value_words, logic_value v, int lsh, t_bitrange range);


void init_net (char *s);
void simulate (unsigned n);

#endif

