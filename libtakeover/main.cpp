//
//  main.cpp
//  libtakeover
//
//  Created by tihmstar on 04.07.23.
//

#include <libgeneral/macros.h>
#include "libtakeover.hpp"
#include <dlfcn.h>
#include <pthread/pthread.h>
#include "TKexception.hpp"
#include <unistd.h>

int lol(uint64_t a1,uint64_t a2,uint64_t a3,uint64_t a4,uint64_t a5,uint64_t a6,uint64_t a7,uint64_t a8){
    printf("lol\n");
    printf("a1=%p\n",(void*)a1);
    printf("a2=%p\n",(void*)a2);
    printf("a3=%p\n",(void*)a3);
    printf("a4=%p\n",(void*)a4);
    printf("a5=%p\n",(void*)a5);
    printf("a6=%p\n",(void*)a6);
    printf("a6=%p\n",(void*)a7);
    printf("a7=%p\n",(void*)a8);
    printf("lol done\n");
    return 0x41414141;
}

void *loop(void*a){
    printf("thread=%p\n",(void*)(uint64_t)mach_thread_self());
    while (1);
}

int main(int argc, const char * argv[]) {
    pthread_t dsa;
    pthread_t *asd = &dsa;

//    pthread_create(asd, NULL, loop, (void*)0x61718191);
    
    printf("pid=%d\n",getpid());

    {
        tihmstar::takeover mytk(mach_task_self());


        mytk.kidnapThread();
        
        mytk.callfunc((void*)lol, {0x10101010,0x20202020,0x30303030,0x40404040,0x50505050,0x60606060,0x70707070,0x80808080});

        char str[] = "/usr/lib/libjailbreak.dylib";
        void *strptr = mytk.allocMem(sizeof(str));
        mytk.writeMem(strptr, str, sizeof(str));
        
        void *dsa = (void*)mytk.callfunc(dlsym(RTLD_NEXT, "dlopen"), {(cpuword_t)strptr,(uint64_t)RTLD_NOW});
        printf("dlopen ok\n");
        
        
        void *sdlopen = dlsym(RTLD_NEXT, "dlopen");
        void *rdlopen = (void*)&dlopen;
    }
    
    printf("done");
    return 0;
}
