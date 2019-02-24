//
//  libtakeover.c
//  libtakeover
//
//  Created by tihmstar on 16.02.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#include "libtakeover_test.h"
#include <stdlib.h>
#include <mach/mach.h>
#include <dlfcn.h>
#include <pthread/pthread.h>
#include <unistd.h>


#define debug(a ...)        (fprintf(stdout,"[DEBUG] "),fprintf(stdout,a),fprintf(stdout,"\n"))
#define log(a ...)          (fprintf(stdout,"[LOG  ] "),fprintf(stdout,a),fprintf(stdout,"\n"))
#define error(a ...)        (fprintf(stderr,"[ERROR] %s: ",__FUNCTION__),fprintf(stderr,a),fprintf(stderr,"\n"))


#define safeFree(buf) do{ if (buf) free(buf), buf = NULL; }while(0)
#define assure(a) do{ if ((a) == 0){err=-(__LINE__); goto error;} }while(0)
#define retassure(cond,errstr ...) do{ if (!(cond)){error(errstr); err=-(__LINE__); goto error;} }while(0)
#define reterror(errstr ...) do{error(errstr); err=-(__LINE__); goto error; }while(0)

kern_return_t mach_vm_allocate(vm_map_t target, mach_vm_address_t *address, mach_vm_size_t size, int flags);
kern_return_t mach_vm_protect(vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size, boolean_t set_maximum, vm_prot_t new_protection);
kern_return_t mach_vm_write(vm_map_t target_task, mach_vm_address_t address, vm_offset_t data, mach_msg_type_number_t dataCnt);

#pragma pack(4)
typedef struct {
    mach_msg_header_t Head;
    mach_msg_body_t msgh_body;
    mach_msg_port_descriptor_t thread;
    mach_msg_port_descriptor_t task;
    NDR_record_t NDR;
} exception_raise_request; // the bits we need at least

typedef struct {
    mach_msg_header_t Head;
    NDR_record_t NDR;
    kern_return_t RetCode;
} exception_raise_reply;
#pragma pack()


int remoteProcCallFunction(mach_port_t client, void *func, int argc, uint64_t *argv){
    int err = 0;
    mach_port_t exceptionPort = MACH_PORT_NULL;
    thread_t mythread = {0};
    mach_vm_address_t stack = 0;
    mach_vm_size_t stackSize = PAGE_SIZE/sizeof(uint64_t);
    mach_vm_size_t stackpointer = stackSize-1;
    arm_thread_state64_t state = {0};
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    uint64_t *localStack = NULL;
    pthread_t remotePthread = 0;
    uint64_t result = 0;
    
    //allocate remote stack
    assure(!(err = mach_vm_allocate(client, &stack, stackSize*sizeof(uint64_t), VM_FLAGS_ANYWHERE)));
    assure(!(err = mach_vm_protect(client, stack, stackSize*sizeof(uint64_t), 1, VM_PROT_READ | VM_PROT_WRITE)));

    //allocate remote thread struct
    assure(!(err = mach_vm_allocate(client, (mach_vm_address_t*)&remotePthread, 0x1000, VM_FLAGS_ANYWHERE)));
    
    
    //setup stack
    assure(localStack = (uint64_t *)malloc(stackSize));
    localStack[stackpointer--] = 0x4142434445464748; //magic end
    
    printf("stack at=%p\n",(void*)stack);
    for (int i=0; i<argc && i < 10; i++) {
        state.__x[i] = argv[i];
    }
    
    //write localStack to remote stack
    assure(!(err = mach_vm_write(client, stack, (vm_offset_t)localStack, (mach_msg_type_number_t)stackSize*sizeof(uint64_t))));
    
    //thread
    assure(!(err = thread_create(client, &mythread)));
    
    {
        uint8_t buf[sizeof(struct _opaque_pthread_t)] = {};
        *(uint64_t*)&buf[0xf8] = (uint64_t)mythread; //thread port
        *(uint64_t*)&buf[0xe0] = (uint64_t)remotePthread; //ptr to remotePthread (self)

        *(uint64_t*)&buf[0x88] = (uint64_t)0x4142434445464748; //pc (do a soft crash)

        
//        *(uint64_t*)&buf[0x88] = (uint64_t)func; //pc
        *(uint64_t*)&buf[0x98] = 0x1337; //arg1

        assure(!(err = mach_vm_write(client, (mach_vm_address_t)remotePthread, (vm_offset_t)buf, (mach_msg_type_number_t)sizeof(buf))));
    }
    
    assure(!(err = thread_get_state(mythread, ARM_THREAD_STATE64, (thread_state_t)&state, &count)));
    
    
    state.__x[0] = (uint64_t)remotePthread;
    state.__lr = 0x7171717171717171;
    state.__pc = (uint64_t)dlsym(RTLD_NEXT, "thread_start");
    state.__sp = (uint64_t)(stack + stackpointer*sizeof(uint64_t));
    
    assure(!(err = thread_set_state(mythread, ARM_THREAD_STATE64, (thread_state_t)&state, ARM_THREAD_STATE64_COUNT)));
    
    //set up exception port
    assure(!(err = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &exceptionPort)));
    assure(!(err = mach_port_insert_right(mach_task_self(),exceptionPort, exceptionPort, MACH_MSG_TYPE_MAKE_SEND)));
    
    // set our new port
    
    assure(!(err = thread_set_exception_ports(mythread, EXC_MASK_ALL & ~(EXC_MASK_MACH_SYSCALL | EXC_MASK_SYSCALL | EXC_MASK_RPC_ALERT), exceptionPort, EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, ARM_THREAD_STATE64)));
    
    //fire!
    assure(!(err = thread_resume(mythread)));
    
    
    //wait for exception
    struct {
        mach_msg_header_t head;
        mach_msg_body_t msgh_body;
        char data[1024];
    } msg = {0};
    assure(!(err = mach_msg(&msg.head, MACH_RCV_MSG|MACH_RCV_LARGE, 0, sizeof(msg), exceptionPort, 0, MACH_PORT_NULL)));
    
    exception_raise_request* req = (exception_raise_request*)&msg;
    
    mach_port_t thread_port = req->thread.name;
    mach_port_t task_port = req->task.name;

    uint64_t origlr = 0;
    uint64_t origx19 = 0;

    assure(!(err = thread_get_state(mythread, ARM_THREAD_STATE64, (thread_state_t)&state, &count)));
    
    origlr = state.__lr;
    origx19 = state.__x[19];
    
    state.__lr = 0x7171717171717171;
    state.__pc = (uint64_t)func;

    for (int i=0; i<29 && i<argc; i++) {
        state.__x[i] = argv[i];
    }
    
    assure(!(err = thread_set_state(mythread, ARM_THREAD_STATE64, (thread_state_t)&state, ARM_THREAD_STATE64_COUNT)));

    exception_raise_reply reply = {0};
    
    reply.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(req->Head.msgh_bits), 0);
    reply.Head.msgh_size = sizeof(reply);
    reply.Head.msgh_remote_port = req->Head.msgh_remote_port;
    reply.Head.msgh_local_port = MACH_PORT_NULL;
    reply.Head.msgh_id = req->Head.msgh_id + 100;
    
    reply.NDR = req->NDR;
    reply.RetCode = KERN_SUCCESS;
    
    assure(!(err = mach_msg(&reply.Head, MACH_SEND_MSG|MACH_MSG_OPTION_NONE, (mach_msg_size_t)sizeof(reply), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL)));
    
    assure(!(err = mach_msg(&msg.head, MACH_RCV_MSG|MACH_RCV_LARGE, 0, sizeof(msg), exceptionPort, 0, MACH_PORT_NULL)));
    
    assure(!(err = thread_get_state(mythread, ARM_THREAD_STATE64, (thread_state_t)&state, &count)));

    //terminate thread
    result = state.__x[0];
    
    for (int i=0; i<29; i++) {
        state.__x[i] = 0x1337;
    }
    assure(!(err = thread_set_state(mythread, ARM_THREAD_STATE64, (thread_state_t)&state, ARM_THREAD_STATE64_COUNT)));

    assure(!(err = thread_terminate(mythread)));
    
    
error:
    printf("err=%d %s\n",err,mach_error_string(err));
    
    
    safeFree(localStack);
    if (exceptionPort) {
        mach_port_deallocate(mach_task_self(), exceptionPort);
    }
    return err ? -err : result;
}


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

int run(){
    int err = 0;
    printf("dorun\n");
    uint64_t argv[] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
    
//    pthread_t f;
//    pthread_create(&f, NULL, &lol, 0);
    
    int ret = remoteProcCallFunction(mach_task_self(), &lol, sizeof(argv), argv);

    printf("ret = %p\n",ret);
    
error:
    printf("done run\n");
    return err;
}
