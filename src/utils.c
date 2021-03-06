/*
Copyright (c) 2015, Ștefan Talpalaru <stefantalpalaru@yahoo.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include "golib.h"

#define ROUND(x, n) (((x)+(n)-1)&~(uintptr)((n)-1))
enum
{
	// flags to malloc
	FlagNoScan	= 1<<0,	// GC doesn't have to scan object
	FlagNoProfiling	= 1<<1,	// must not profile
	FlagNoGC	= 1<<2,	// must not free or scan for pointers
	FlagNoZero	= 1<<3, // don't zero memory
	FlagNoInvokeGC	= 1<<4, // don't invoke GC
};

// libgo symbols
extern void runtime_check();
extern void runtime_args(int, char **);
extern void runtime_osinit();
extern void runtime_schedinit();
extern void runtime_main();
extern void runtime_mstart(void *);
// no_split_stack is the key to avoid crashing !!! [uWSGI comment, I don't see crashes with gcc-4.9.2]
/*void* runtime_m() __attribute__ ((noinline, no_split_stack));*/
extern void* runtime_m();
extern void* runtime_mallocgc(uintptr size, uintptr typ, uint32 flag);

// our golib.go symbols
extern void golib_init() __asm__ ("main.Golib_init");

// have the GC scan the BSS
extern char edata, end;
struct root_list {
	struct root_list *next;
	struct root {
		void *decl;
		size_t size;
	} roots[];
};
extern void __go_register_gc_roots (struct root_list* r);
static struct root_list bss_roots = {
    NULL,
    { { NULL, 0 },
      { NULL, 0 } },
};

void golib_main(int argc, char **argv) {
    runtime_check();
    /*printf("edata=%p, end=%p, end-edata=%d\n", &edata, &end, &end - &edata);*/
    bss_roots.roots[0].decl = &edata;
    bss_roots.roots[0].size = &end - &edata;
    __go_register_gc_roots(&bss_roots);
    runtime_args(argc, argv);
    runtime_osinit();
    runtime_schedinit();
    golib_init();
    __go_go((void (*)(void *))runtime_main, NULL);
    runtime_mstart(runtime_m());
    abort();
}

void* go_malloc(uintptr size) {
    return runtime_mallocgc(ROUND(size, sizeof(void*)), 0, FlagNoZero);
}

void* go_malloc0(uintptr size) {
    return runtime_mallocgc(ROUND(size, sizeof(void*)), 0, 0);
}

void go_run_finalizer(void (*f)(void *), void *obj) {
    f(obj);
}
