
#CCFLAGS = -Dcountbits -Dblock_debugging -Dsim_debugging
#CCFLAGS = -Dcountbits -Wno-write-strings -Duse_wide_drivers
CCFLAGS = -Wno-write-strings -O -fpermissive
CCCRUNFLAGS = -Wno-write-strings -Dcompiled_runtime -O
tortleg: 
	gcc -otortleg $(CCFLAGS) *.cpp -lpcre

tortlegcrun: 
	gcc -otortlegcrun $(CCCRUNFLAGS) *.cpp -lpcre

SHELL=/bin/bash

#CCFLAGS = -Dinline_drive_node

LIBFLAGS = -lpcre -lstdc++

all: install binaries


tortlegg: sim.o mem_access.o logic.o scan_parse.o compile_net.o make_net.o nethelp.o make_blocks.o \
			blocktypes.o command.o describe.o trace.o utils.o codegen.o
	gcc -g $(CCFLAGS) -o tortlegg sim.o mem_access.o logic.o scan_parse.o compile_net.o make_net.o nethelp.o \
			make_blocks.o blocktypes.o command.o describe.o trace.o utils.o codegen.o $(LIBFLAGS)

nethelp.o: nethelp.cpp 
	gcc -g -c $(CCFLAGS) nethelp.cpp

mem_access.o: mem_access.cpp config.h parser.h tortle_types.h fileparam.h
	gcc -g -c $(CCFLAGS) mem_access.cpp

sim.o: sim.cpp config.h parser.h tortle_types.h fileparam.h
	gcc -g -c $(CCFLAGS) sim.cpp

trace.o: trace.cpp config.h parser.h tortle_types.h fileparam.h
	gcc -g -c $(CCFLAGS) trace.cpp

describe.o: describe.cpp config.h parser.h tortle_types.h fileparam.h
	gcc -g -c $(CCFLAGS) describe.cpp

utils.o: utils.cpp config.h parser.h tortle_types.h fileparam.h
	gcc -g -c $(CCFLAGS) utils.cpp

command.o: command.cpp config.h parser.h tortle_types.h 
	gcc -g -c $(CCFLAGS) command.cpp

logic.o: logic.cpp config.h parser.h tortle_types.h
	gcc -g -c $(CCFLAGS) logic.cpp

make_net.o: make_net.cpp config.h parser.h tortle_types.h blocktypes.h
	gcc -g -c $(CCFLAGS) make_net.cpp

compile_net.o: compile_net.cpp config.h parser.h tortle_types.h blocktypes.h
	gcc -g -c $(CCFLAGS) compile_net.cpp

codegen.o: codegen.cpp config.h parser.h tortle_types.h blocktypes.h
	gcc -g -c $(CCFLAGS) codegen.cpp

scan_parse.o: config.h parser.h tortle_types.h scan_parse.cpp
	gcc -g -c $(CCFLAGS) scan_parse.cpp

make_blocks.o: make_blocks.cpp tortle_types.h config.h parser.h fileparam.h
	gcc -g -c $(CCFLAGS) make_blocks.cpp

blocktypes.o: blocktypes.cpp tortle_types.h blocktypes.h parser.h
	gcc -g -c $(CCFLAGS) blocktypes.cpp

