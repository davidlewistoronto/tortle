#ifndef __MEM_ACCESS_H
#define __MEM_ACCESS_H

void dumpram (obj_ptr ob, int s, int e);
void examineram (obj_ptr ob, int *nn);
void changeram (obj_ptr ob, int *n, char *s);
void romload (obj_ptr ob, char *fs);

#endif

 