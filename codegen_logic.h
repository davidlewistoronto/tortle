#ifndef __CODEGEN_LOGIC_H
#define __CODEGEN_LOGIC_H


void codegen_constfn (FILE *cf, bptr b);
void codegen_nandfn (FILE *cf, bptr b);
void codegen_andfn (FILE *cf, bptr b);
void codegen_orfn (FILE *cf, bptr b);
void codegen_norfn (FILE *cf, bptr b);
void codegen_xorfn (FILE *cf, bptr b);
void codegen_eqeqfn (FILE *cf, bptr b);
void codegen_regfn (FILE *cf, bptr b);
void codegen_rsregfn (FILE *cf, bptr b);
void codegen_latchfn (FILE *cf, bptr b);
void codegen_buf3sfn (FILE *cf, bptr b);
void codegen_adderfn (FILE *cf, bptr b);
void codegen_subfn (FILE *cf, bptr b);
void codegen_greaterfn (FILE *cf, bptr b);
void codegen_lessfn (FILE *cf, bptr b);
void codegen_geqfn (FILE *cf, bptr b);
void codegen_leqfn (FILE *cf, bptr b);
void codegen_eqfn (FILE *cf, bptr b);
void codegen_neqfn (FILE *cf, bptr b);
void codegen_lshfn (FILE *cf, bptr b);
void codegen_rshfn (FILE *cf, bptr b);
void codegen_concatfn (FILE *cf, bptr b);
void codegen_muxfn (FILE *cf, bptr b);
void codegen_demuxfn (FILE *cf, bptr b);
void codegen_ramfn (FILE *cf, bptr b);
void codegen_romfn (FILE *cf, bptr b);
void codegen_memfn (FILE *cf, bptr b);
void codegen_expandfn (FILE *cf, bptr b);
void codegen_priorityfn (FILE *cf, bptr b);
void codegen_undefinedfn (FILE *cf, bptr b);

#endif

