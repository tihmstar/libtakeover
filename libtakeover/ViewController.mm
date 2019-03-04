//
//  ViewController.m
//  libtakeover
//
//  Created by tihmstar on 16.02.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#import "ViewController.h"
#include "libtakeover.hpp"
#include <dlfcn.h>
#include <pthread/pthread.h>
#include "TKexception.hpp"

@interface ViewController ()

@end

@implementation ViewController

int lol(uint64_t a1,uint64_t a2,uint64_t a3,uint64_t a4,uint64_t a5,uint64_t a6,uint64_t a7,uint64_t a8){
    printf("lol\n");
    printf("a1=%p\n",a1);
    printf("a2=%p\n",a2);
    printf("a3=%p\n",a3);
    printf("a4=%p\n",a4);
    printf("a5=%p\n",a5);
    printf("a6=%p\n",a6);
    printf("a6=%p\n",a7);
    printf("a7=%p\n",a8);    
    printf("lol done\n");
    return 0x41414141;
}

void *loop(void*a){
    printf("thread=%p\n",mach_thread_self());
    while (1);
}

- (void)viewDidLoad {
    [super viewDidLoad];

    
    tihmstar::takeover mytk(mach_task_self());
    mytk.kidnapThread();
    char str[] = "/usr/lib/libjailbreak.dylib";
    void *strptr = mytk.allocMem(sizeof(str));
    mytk.writeMem(strptr, sizeof(str), str);

    void *dsa = (void*)mytk.callfunc(dlsym(RTLD_NEXT, "dlopen"), {(uint64_t)strptr,(uint64_t)RTLD_NOW});
    printf("dlopen ok\n");

    sleep(1);


    void *sdlopen = dlsym(RTLD_NEXT, "dlopen");
    void *rdlopen = (void*)&dlopen;
    
    
    printf("done");
}


@end
