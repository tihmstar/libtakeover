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
#include <iostream>

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

void (*start_thread)(void);

void *testprint(char *arg){
	printf("Got: %s\n", arg);
	return NULL;
}

- (void)viewDidLoad {
    [super viewDidLoad];
	
	start_thread = (void (*)(void))dlsym(RTLD_NEXT, "thread_start");
    
    printf("pid=%d\n",getpid());

	//stringToPrint = strdup("test");

	try {
		tihmstar::takeover mytk(mach_task_self());
		mytk.kidnapThread();
	
		char *str = "/electra/helloworld.dylib";
		void *strptr = mytk.allocMem(0x100 + strlen(str) + 1);
		mytk.writeMem((void *)(0x100 + (uint64_t)strptr), strlen(str) + 1, str);

		void *dsa = (void*)mytk.callfunc(dlsym(RTLD_NEXT, "dlopen"), {0x100 + (uint64_t)strptr,(uint64_t)RTLD_NOW});
		printf("dlopen ok\n");
		
		mytk.callfunc((void *)&testprint, {0x100 + (uint64_t)strptr});
	
		/*mytk.callfunc((void *)&pthread_create, {(uint64_t)NULL, (uint64_t)NULL, (uint64_t)&testprint, 0x100 + (uint64_t)strptr});*/

		sleep(1);


		/*void *sdlopen = dlsym(RTLD_NEXT, "dlopen");
		void *rdlopen = (void*)&dlopen;*/
	} catch (tihmstar::exception &e){
		std::cerr << "Exception: " << e.what() << "; Code: " << (e.code() & 0xff) << "; Filelength: " << (e.code() >> 16) << std::endl;
	} catch (const std::exception &e){
		std::cerr << "Exception: " << e.what() << std::endl;
	}
    
    
    printf("done");
}


@end
