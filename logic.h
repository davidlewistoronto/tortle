#ifndef __LOGIC_H
#define __LOGIC_H


void constfn (bptr b);
void nandfn (bptr b);
void andfn (bptr b);
void orfn (bptr b);
void norfn (bptr b);
void xorfn (bptr b);
void eqeqfn (bptr b);
void regfn (bptr b);
void rsregfn (bptr b);
void latchfn (bptr b);
void buf3sfn (bptr b);
void adderfn (bptr b);
void subfn (bptr b);
void greaterfn (bptr b);
void lessfn (bptr b);
void geqfn (bptr b);
void leqfn (bptr b);
void eqfn (bptr b);
void neqfn (bptr b);
void lshfn (bptr b);
void rshfn (bptr b);
void concatfn (bptr b);
void muxfn (bptr b);
void demuxfn (bptr b);
void ramfn (bptr b);
void romfn (bptr b);
void memfn (bptr b);
void expandfn (bptr b);
void alufn(bptr b);
void priorityfn (bptr b);
void undefinedfn(bptr b);

void fastnandfn (bptr  b);
void fastnand2fn (bptr b);
void fastnand1fn (bptr b);
void fastandfn (bptr  b);
void fastand2fn (bptr  b);
void fastorfn (bptr  b);
void fastxorfn (bptr  b);
void fastxor2fn (bptr  b);

extern t_fast_logic_desc fast_block_list [];

#define state_word_ram_depth	0
#define state_word_ram_width	1
#define state_word_ram_words_per_state_word	2
#define state_word_ram_state_words_per_word	3
#define state_word_ram_nports	4
#define state_word_ram_state_data	5


#endif
