/* copyright (c) David M. Lewis 1987 */

#define ndebugs 50

extern int debug [ndebugs];
extern FILE *debugfile;
extern bool have_debugfile;

#define debug_parser				debug [0]
#define debug_hash 					debug [1]
#define debug_nodes					debug [2]
#define debug_makeblocks			debug [3]
#define debug_logiceval				debug [4]
#define debug_traceall				debug [5]
#define debug_defs					debug [6]
#define debug_echo_cmd				debug [7]
#define debug_subnodes				debug [8]
#define debug_compiled_cmds			debug [9]
#define debug_compiled_comments		debug [10]
#define debug_compiled_exec			debug [11]
#define debug_ascii_descr			debug [12]
#define debug_bin_descr				debug [13]
#define debug_codegen_clock_domains	debug [14]
#define debug_block_fanout_graph	debug [15]
#define debug_hash_table_smalloc    debug [16]
#define debug_wide_node_drive       debug [17]
#define debug_node_drive_count      debug [18]
#define debug_disable_fast_blocks	debug [19]
