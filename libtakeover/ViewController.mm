//
//  ViewController.m
//  libtakeover
//
//  Created by tihmstar on 16.02.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#import "ViewController.h"
#include "libtakeover.hpp"

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
    
    printf("self=%p\n",mach_thread_self());
    
    printf("lol done\n");
    return 0x41414141;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    tihmstar::takeover mytk(mach_task_self());
    
    mytk.callfunc((void*)&lol, {0x11,0x22,0x33});
    mytk.callfunc((void*)&lol, {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99});
    
    printf("done");
}


@end
