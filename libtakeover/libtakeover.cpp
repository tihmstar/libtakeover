//
//  libtakeover.cpp
//  libtakeover
//
//  Created by tihmstar on 24.02.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#include "libtakeover.hpp"
#include "TKexception.hpp"
#include <stdlib.h>
#include <dlfcn.h>
#include <pthread/pthread.h>
#include <stdlib.h>

#include <unistd.h>

#if defined (__arm64__)

#   define MY_THREAD_STATE ARM_THREAD_STATE64
#   define MY_THREAD_STATE_COUNT ARM_THREAD_STATE64_COUNT
#   define my_thread_state_t arm_thread_state64_t

#elif defined (__arm__)

#   define MY_THREAD_STATE ARM_THREAD_STATE
#   define MY_THREAD_STATE_COUNT ARM_THREAD_STATE_COUNT
#   define my_thread_state_t arm_thread_state_t

#endif


using namespace tihmstar;

extern "C"{
kern_return_t mach_vm_allocate(vm_map_t target, mach_vm_address_t *address, mach_vm_size_t size, int flags);
kern_return_t mach_vm_protect(vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size, boolean_t set_maximum, vm_prot_t new_protection);
kern_return_t mach_vm_read_overwrite(vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size, mach_vm_address_t data, mach_vm_size_t *outsize);
kern_return_t mach_vm_write(vm_map_t target_task, mach_vm_address_t address, vm_offset_t data, mach_msg_type_number_t dataCnt);
kern_return_t mach_vm_deallocate(mach_port_name_t target, mach_vm_address_t address, mach_vm_size_t size);
};

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

typedef struct {
    mach_msg_header_t Head;
    NDR_record_t NDR;
    kern_return_t RetCode;
    int flavor;
    mach_msg_type_number_t new_stateCnt;
    natural_t new_state[614];
} exception_raise_state_reply;
#pragma pack()

takeover::takeover()
    /* init member vars */
    :_remoteStack(0),_marionetteThread(MACH_PORT_NULL),_exceptionHandler(MACH_PORT_NULL),_remoteSelf(MACH_PORT_NULL),_emsg({}),
        _isFakeThread(true), _remoteScratchSpace(NULL), _remoteScratchSpaceSize(0)
{
    
}

takeover::takeover(mach_port_t target)
    /* init member vars */
    :_target(target),_remoteStack(0),_marionetteThread(MACH_PORT_NULL),_exceptionHandler(MACH_PORT_NULL),_remoteSelf(MACH_PORT_NULL),_emsg({}),
        _isFakeThread(true), _remoteScratchSpace(NULL), _remoteScratchSpaceSize(0)
{
    
    /* setup local variables */
    cpuword_t *localStack = NULL;
    size_t stackpointer = 0;
    mach_msg_type_number_t count = MY_THREAD_STATE_COUNT;
    my_thread_state_t state = {0};
    
    /* setup cleanups for normal/emergency case */
    auto finalClean =[&]{ //always clean up
        safeFree(localStack);
    };
    
    auto clean =[&]{ //cleanup only if something goes wrong
        auto err = deinit();
        if(err.first){
            error("[takeover] deinit failed on line %d with code %d",err.first,err.second);
        }
        
        finalClean(); //also do regular cleanup
    };
    
    /* actually construct object */
    
    //aquire send right to targer
    {
        kern_return_t err = 0;
        doassure(!(err = mach_port_insert_right(mach_task_self(),_target, _target, MACH_MSG_TYPE_COPY_SEND)), [&](void)->void{
            //if this step fails, make sure not to drop a send right to the target on cleanup!
            _target = MACH_PORT_NULL;
            assureMachclean(err);
        }());
    }
    
    //allocate remote stack
    assureMachclean(mach_vm_allocate(_target, &_remoteStack, _remoteStackSize, VM_FLAGS_ANYWHERE));
    assureMachclean(mach_vm_protect(_target, _remoteStack, _remoteStackSize, 1, VM_PROT_READ | VM_PROT_WRITE));


    //setup stack
    assureclean(localStack = (cpuword_t *)malloc(_remoteStackSize));
    stackpointer = ((_remoteStackSize/2) / sizeof(cpuword_t))-1; //leave enough space for stack args
#if defined (__arm64__)
    localStack[stackpointer--] = 0x4142434445464748; //magic end (x86 legacy, we don't need this for ARM64, do we?)
#elif defined (__arm__)
    localStack[stackpointer--] = 0x41424344; //magic end (x86 legacy, we don't need this for ARM64, do we?)
#endif

    
    //spawn new thread
    assureMachclean(thread_create(_target, &_marionetteThread));

#if defined (__arm64__)
    localStack[0xf8/8] = (cpuword_t)_marionetteThread;   //thread port
    localStack[0xe0/8] = (cpuword_t)_remoteStack;        //ptr to remotePthreadBuf (lies at the beginning of _remoteStack)
    localStack[0x88/8] = (cpuword_t)0x5152535455565758;  //pc (do a soft crash)
    localStack[0x98/8] = 0x1337;                        //pthread arg1
#elif defined (__arm__)
    localStack[0x4c/4] = (cpuword_t)_remoteStack+0x100;
#endif

    
    //write localStack to remote stack
    assureMachclean(mach_vm_write(_target, _remoteStack, (vm_offset_t)localStack, (mach_msg_type_number_t)_remoteStackSize));
    
    //setup thread state
    assureMachclean(thread_get_state(_marionetteThread, MY_THREAD_STATE, (thread_state_t)&state, &count));
    
#if defined (__arm64__)
    state.__x[0] = (cpuword_t)_remoteStack;
    arm_thread_state64_set_lr_fptr(state,(void*)0x7171717171717171);
    arm_thread_state64_set_pc_fptr(state,dlsym(RTLD_NEXT, "thread_start"));
    assureclean(arm_thread_state64_get_pc(state));
    arm_thread_state64_set_sp(state,_remoteStack + stackpointer*sizeof(cpuword_t));
#elif defined (__arm__)
    state.__r[0] = (cpuword_t)_remoteStack;
    state.__r[1] = (cpuword_t)_marionetteThread;
    state.__r[2] = (cpuword_t)0x51515151;
    state.__r[3] = (cpuword_t)0x1337; //pthread arg1
    state.__r[4] = (cpuword_t)0x00080000; //idk
    state.__r[5] = (cpuword_t)0x080008ff; //idk
    state.__lr = 0x71717171;        //actual magic end
    assureclean(state.__pc = (cpuword_t)dlsym(RTLD_NEXT, "thread_start"));
    state.__sp = (cpuword_t)(_remoteStack + stackpointer*sizeof(cpuword_t));
#endif
    
    if (dlsym(RTLD_NEXT, "pthread_create_from_mach_thread")){
#if defined (__arm64__)
        arm_thread_state64_set_pc_fptr(state,(void*)0x4141414141414141);
#elif defined (__arm__)
        state.__pc = 0x41414141;
#endif
    }

    assureMachclean(thread_set_state(_marionetteThread, MY_THREAD_STATE, (thread_state_t)&state, MY_THREAD_STATE_COUNT));

    //create exception port
    assureMachclean(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &_exceptionHandler));
    assureMachclean(mach_port_insert_right(mach_task_self(),_exceptionHandler, _exceptionHandler, MACH_MSG_TYPE_MAKE_SEND));
    
    //set our new port
    assureMachclean(thread_set_exception_ports(_marionetteThread, EXC_MASK_ALL & ~(EXC_MASK_BREAKPOINT | EXC_MASK_MACH_SYSCALL | EXC_MASK_SYSCALL | EXC_MASK_RPC_ALERT), _exceptionHandler, EXCEPTION_STATE | MACH_EXCEPTION_CODES, MY_THREAD_STATE));

    //initialize our remote thread
    assureMachclean(thread_resume(_marionetteThread));
    
    //wait for exception
    assureMachclean(mach_msg(&_emsg.head, MACH_RCV_MSG|MACH_RCV_LARGE, 0, sizeof(_emsg), _exceptionHandler, 0, MACH_PORT_NULL));
    
    finalClean();
    
    //if we have pthread_create_from_mach_thread, we can do things much cleaner
    if (dlsym(RTLD_NEXT, "pthread_create_from_mach_thread")){
        kidnapThread();
    }
}

takeover::takeover(takeover &&tk)
    :_target(tk._target),_remoteStack(tk._remoteStack),_marionetteThread(tk._marionetteThread),_exceptionHandler(tk._exceptionHandler),_emsg(tk._emsg),
        _isFakeThread(tk._isFakeThread)
{
    tk._remoteStack = 0;
    tk._target = MACH_PORT_NULL;
    tk._exceptionHandler = MACH_PORT_NULL;
    tk._marionetteThread = MACH_PORT_NULL;
}

takeover takeover::takeoverWithExceptionHandler(mach_port_t exceptionHandler){
    takeover tk;
    tk._exceptionHandler = exceptionHandler;
    tk._isFakeThread = false;
    
    assureMach(mach_msg(&tk._emsg.head, MACH_RCV_MSG|MACH_RCV_LARGE, 0, sizeof(tk._emsg), tk._exceptionHandler, 0, MACH_PORT_NULL));
    
    tk._marionetteThread = tk._emsg.head.msgh_remote_port;
    
    return tk;
}

cpuword_t takeover::callfunc(void *addr, const std::vector<cpuword_t> &x){
    my_thread_state_t *state = (my_thread_state_t*)(_emsg.data+0x24);
    cpuword_t lrmagic = 0x71717171;
    
#if defined (__arm64__)
    arm_thread_state64_set_pc_fptr(*state,(void*)addr);
    arm_thread_state64_set_lr_fptr(*state,(void*)lrmagic);
#else
    state->__lr = (void *)lrmagic;
    state->__pc = (cpuword_t)addr;
#endif
    
    
#if defined (__arm64__)
    retassure(x.size() <= 29,"only up to 29 arguments allowed");
    for (int i=0; i<29; i++) {
        state->__x[i] = (i<x.size()) ? x[i] : 0;
    }
#elif defined (__arm__)
    if (x.size() <= 4) {
        retassure(x.size() <= 12,"only up to 12 arguments allowed");
        for (int i=0; i<12; i++) {
            state->__r[i] = (i<x.size()) ? x[i] : 0;
        }
    }else{
        //wtf is this??
        state->__r[0] = x.at(0);
        state->__r[2] = x.at(1);
        //pass args by stack
        for (int i=2; i<x.size(); i++) {
            uint64_t arg = x.at(i);
            writeMem(((uint64_t*)state->__sp)+(i-2), &arg, sizeof(arg));
        }
    }
    
    state->__cpsr = (state->__cpsr & ~(1<<5)) | ((state->__pc & 1) << 5); //set ARM/THUMB mode properly
#endif
    
    exception_raise_state_reply reply = {};
    exception_raise_request* req = (exception_raise_request*)&_emsg;

    reply.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(_emsg.head.msgh_bits), 0);
    reply.new_stateCnt = MY_THREAD_STATE_COUNT;
    reply.Head.msgh_remote_port = _emsg.head.msgh_remote_port;
    reply.Head.msgh_local_port = MACH_PORT_NULL;
    reply.Head.msgh_id = _emsg.head.msgh_id + 100;

    reply.NDR = req->NDR;
    reply.RetCode = KERN_SUCCESS;
    reply.flavor = MY_THREAD_STATE;
    memcpy(reply.new_state, state, sizeof(my_thread_state_t));
    
    reply.Head.msgh_size = (mach_msg_size_t)(sizeof(exception_raise_state_reply) - 2456) + (((4 * reply.new_stateCnt))); //straight from MIG
    
    //resume
    assureMach(mach_msg(&reply.Head, MACH_SEND_MSG|MACH_MSG_OPTION_NONE, (mach_msg_size_t)reply.Head.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL));
    
    //wait until end of function
    assureMach(mach_msg(&_emsg.head, MACH_RCV_MSG|MACH_RCV_LARGE, 0, sizeof(_emsg), _exceptionHandler, 0, MACH_PORT_NULL));
    
    //get result (implicit through receiving mach_msg)
    
#if defined (__arm64__)
    assure(((cpuword_t)arm_thread_state64_get_pc(*state) & 0x1FFFFFFFF) == lrmagic);
#else
    assure((state->__pc & (~1)) == (lrmagic & (~1)));
#endif
    
#if defined (__arm64__)
    return state->__x[0];
#elif defined (__arm__)
    return state->__r[0];
#endif
}

void takeover::kidnapThread(){
    if (!_isFakeThread)
        return;
    void *mem_mutex           = NULL;
    void *mem_thread          = NULL;
    void *func_mutex_init     = NULL;
    void *func_mutex_destroy  = NULL;
    void *func_mutex_lock     = NULL;
    void *func_mutex_unlock   = NULL;
    void *func_pthread_create = NULL;
    void *func_pthread_exit   = NULL;
    void *pt_from_mt = NULL;
    cpuword_t ret = 0;
    bool lockIsInited = false;
    thread_array_t threadList = 0;
    mach_msg_type_number_t threadCount = 0;
    mach_port_t kidnapped_thread = 0;
    my_thread_state_t state = {0};
    mach_msg_type_number_t count = MY_THREAD_STATE_COUNT;
    mach_port_t newExceptionHandler = 0;
    
    auto clean =[&](bool isInException = true){ //cleanup only code
        if (kidnapped_thread) {
            //idk if this leaks resources, ideally we shouldn't even get here
            assureNoDoublethrow(assureMach(thread_terminate(kidnapped_thread)));
            kidnapped_thread = MACH_PORT_NULL;
        }
        
        if (lockIsInited) {
            assureNoDoublethrow(callfunc(func_mutex_destroy, {(cpuword_t)mem_mutex}));
            lockIsInited = false;
        }
        if (mem_mutex) {
            assureNoDoublethrow(deallocMem(mem_mutex,sizeof(pthread_mutex_t)));
            mem_mutex = NULL;
        }
        if (mem_thread) {
            assureNoDoublethrow(deallocMem(mem_thread,sizeof(pthread_t)));
            mem_thread = NULL;
        }
        if (newExceptionHandler) {
            //ideally we don't get here
            assureNoDoublethrow(assureMach(mach_port_destroy(mach_task_self(), newExceptionHandler)));
            newExceptionHandler = MACH_PORT_NULL;
        }
    };
    
    assureCatchClean(mem_mutex = allocMem(sizeof(pthread_mutex_t)));
    assureCatchClean(mem_thread = allocMem(sizeof(pthread_t)));

    assureclean(func_mutex_init     = dlsym(RTLD_NEXT, "pthread_mutex_init"));
    assureclean(func_mutex_destroy  = dlsym(RTLD_NEXT, "pthread_mutex_destroy"));
    assureclean(func_mutex_lock     = dlsym(RTLD_NEXT, "pthread_mutex_lock"));
    assureclean(func_mutex_unlock   = dlsym(RTLD_NEXT, "pthread_mutex_unlock"));
    assureclean(func_pthread_create = dlsym(RTLD_NEXT, "pthread_create"));
    assureclean(func_pthread_exit   = dlsym(RTLD_NEXT, "pthread_exit"));
    
    assureCatchClean(ret = callfunc(func_mutex_init, {(cpuword_t)mem_mutex,0}));
    assureclean(!ret);
    lockIsInited = true;
    
    if ((pt_from_mt = dlsym(RTLD_NEXT, "pthread_create_from_mach_thread"))) {
        //this spawns thread which locks mutex and finishes
        assureCatchClean(ret = callfunc(pt_from_mt, {(cpuword_t)_remoteStack, NULL, (cpuword_t)func_mutex_lock, (cpuword_t)mem_mutex}));
        assureclean(!ret);
        
        usleep(420); //wait for new thread to spawn

        //this spawns thread which locks mutex and stays, we will kidnap this one
        assureCatchClean(ret = callfunc(pt_from_mt, {(cpuword_t)mem_thread, NULL, (cpuword_t)func_mutex_lock, (cpuword_t)mem_mutex}));
        assureclean(!ret);
    }else{
        assureCatchClean(ret = callfunc(func_mutex_lock, {(cpuword_t)mem_mutex}));
        assureclean(!ret);

        assureCatchClean(ret = callfunc(func_pthread_create, {(cpuword_t)mem_thread,0,(cpuword_t)func_mutex_lock,(cpuword_t)mem_mutex}));
        assureclean(!ret);
    }

    usleep(420); //wait for new thread to spawn
    //find new thread
    assureMachclean(task_threads(_target, &threadList, &threadCount));
    
    for (int i=0; i<threadCount; i++) {
        assureMachclean(thread_get_state(threadList[i], MY_THREAD_STATE, (thread_state_t)&state, &count));
        
        
#if defined (__arm64__)
            if (state.__x[0] == (cpuword_t)mem_mutex) {
#elif defined (__arm__)
            if (state.__r[0] == (cpuword_t)mem_mutex) {
#endif
            //found to-kidnap-thread!
            kidnapped_thread = threadList[i];
            break;
        }
    }
    assureclean(kidnapped_thread);
    
    //create exception port
    assureMachclean(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &newExceptionHandler));
    assureMachclean(mach_port_insert_right(mach_task_self(),newExceptionHandler, newExceptionHandler, MACH_MSG_TYPE_MAKE_SEND));
    
    //set our new port
    assureMachclean(thread_set_exception_ports(kidnapped_thread, EXC_MASK_ALL & ~(EXC_MASK_BREAKPOINT | EXC_MASK_MACH_SYSCALL | EXC_MASK_SYSCALL | EXC_MASK_RPC_ALERT), newExceptionHandler, EXCEPTION_STATE | MACH_EXCEPTION_CODES, MY_THREAD_STATE));

    
#if defined (__arm64__)
    arm_thread_state64_set_lr_fptr(state,(void*)0x6161616161616161);
#elif defined (__arm__)
    state.__lr = 0x61616161; //magic end
#endif
        
    
    //set magic end
    assureMachclean(thread_set_state(kidnapped_thread, MY_THREAD_STATE, (thread_state_t)&state, MY_THREAD_STATE_COUNT));

    if (pt_from_mt) {
        //this new thread will finish again
        assureCatchClean(ret = callfunc(pt_from_mt, {(cpuword_t)_remoteStack, NULL, (cpuword_t)func_mutex_unlock, (cpuword_t)mem_mutex}));
        assureclean(!ret);
        
        usleep(420); //wait for new thread to spawn
    }else{
        //unlock mutex and let thread fall in magic
        assureCatchClean(ret = callfunc(func_mutex_unlock, {(cpuword_t)mem_mutex}));
    }
        
    
    //kill marionettThread
    deinit(true);
    
    //restore globals
    _exceptionHandler = newExceptionHandler;
    newExceptionHandler = MACH_PORT_NULL;
    
    _marionetteThread = kidnapped_thread;
    kidnapped_thread = MACH_PORT_NULL;
    
    //receive exception
    assureMachclean(mach_msg(&_emsg.head, MACH_RCV_MSG|MACH_RCV_LARGE, 0, sizeof(_emsg), _exceptionHandler, 0, MACH_PORT_NULL));
    
    clean(false);
    _isFakeThread = false;
}

void takeover::primitiveWrite(void *remote, void *inAddr, size_t size){
    for (int i=0; i<size; i++) {
        callfunc((void*)memset,{(cpuword_t)remote+i,(cpuword_t)((uint8_t*)inAddr)[i],1});
    }
}

void takeover::primitiveRead(void *remote, void *outAddr, size_t size){
    uint8_t ab = *(uint8_t*)memcmp;
    for (int i=0; i<size; i++) {
        ((uint8_t*)outAddr)[i] = (uint8_t)callfunc((void*)memcmp,{(cpuword_t)remote+i,(cpuword_t)memcmp,1}) + ab;
    }
}



void takeover::overtakeMe(){
    if (_remoteSelf)
        return;
    kern_return_t ret = 0;
    mach_port_t localSenderPort = MACH_PORT_NULL;
    mach_port_t remoteListenerPort = MACH_PORT_NULL;
    mach_port_t remoteExcetionHandlerPort = MACH_PORT_NULL;
    auto clean =[&](bool isInException = true){ //cleanup only code
        if (localSenderPort) {
            assureNoDoublethrow(assureMach(mach_port_deallocate(mach_task_self(), localSenderPort)));
        }
        if (remoteListenerPort) {
            assureNoDoublethrow(assureMach(callfunc((void*)mach_port_deallocate,{(cpuword_t)mach_task_self_, (cpuword_t)remoteListenerPort})));
        }
    };
    //no cleanup vars
    void *curScratchSpace = NULL;
    size_t curScratchSpaceSize = 0;
    mach_port_t remoteThreadPort = MACH_PORT_NULL;
    mach_port_t *remoteListener = NULL;
    exception_mask_array_t masks = NULL;
    mach_msg_type_number_t *masksCnt = NULL;
    exception_handler_array_t old_handlers = NULL;
    exception_behavior_array_t old_behaviors = NULL;
    exception_flavor_array_t old_flavors = NULL;
    exception_raise_request msg = {};
    exception_raise_request *remoteMsg = NULL;
    
#define allocScratchSpace(size) ((void*)(curScratchSpace = (uint8_t *)curScratchSpace-size,({assure(curScratchSpaceSize>=size);}),curScratchSpaceSize-=size,(uint8_t *)curScratchSpace-size))
    
    curScratchSpaceSize = 0x1000;
    {
        //alloc mem chicken & egg problem
        //you need to have mem, to alloc mem ._.
        
        //alloc temp mem
        void *tempScratchSpace = (void*)callfunc((void*)malloc, {0x10});
        _remoteScratchSpaceSize = 0x10;
        _remoteScratchSpace = tempScratchSpace;
        
        //do real allocation
        curScratchSpace  = allocMem(curScratchSpaceSize);
        _remoteScratchSpace = curScratchSpace;
        _remoteScratchSpaceSize = curScratchSpaceSize;
        
        //free temp mem
        callfunc((void*)free, {(cpuword_t)tempScratchSpace});
    }
    allocScratchSpace(0x100);    //reserve first few bytes for possible internal stuff

    remoteListener = (mach_port_t*)allocScratchSpace(sizeof(mach_port_t));
    
    /*
     remote: create remote port with rcv right
     */
    //get remote thread port
    assure(remoteThreadPort = (mach_port_t)callfunc((void*)mach_thread_self, {}));
    
    //get remote exception handler port (which a a port we have a recv right to)
    masks = (exception_mask_array_t)allocScratchSpace(sizeof(exception_mask_array_t));
    masksCnt = (mach_msg_type_number_t *)allocScratchSpace(sizeof(mach_msg_type_number_t *));
    old_handlers = (exception_handler_array_t)allocScratchSpace(sizeof(exception_handler_array_t));
    old_behaviors = (exception_behavior_array_t)allocScratchSpace(sizeof(exception_behavior_array_t));
    old_flavors = (exception_flavor_array_t)allocScratchSpace(sizeof(exception_flavor_array_t));
    remoteMsg = (exception_raise_request*)allocScratchSpace(sizeof(exception_raise_request));
    
    //construct mach msg locally
    msg.Head.msgh_bits = MACH_MSGH_BITS_SET(MACH_MSG_TYPE_COPY_SEND, 0, 0, 0);
    msg.Head.msgh_id = 1336;
    
    msg.Head.msgh_remote_port = 0; //this one gets overwritten with exception port!
    
    msg.Head.msgh_local_port = MACH_PORT_NULL;
    msg.Head.msgh_size = sizeof(msg); //a bit larger than really neccessary, but who cares
    msg.msgh_body.msgh_descriptor_count = 1;
    
    msg.thread.name = 0; //this will be overwritten with a newly allocated port
    msg.thread.disposition = MACH_MSG_TYPE_COPY_SEND;
    msg.thread.type = MACH_MSG_PORT_DESCRIPTOR;
    
    //move over msg
    primitiveWrite(remoteMsg,&msg,sizeof(msg));
    
    //fill remote_port with remote tasks exception port
    assureMachclean(callfunc((void*)thread_get_exception_ports,{(cpuword_t)remoteThreadPort, (cpuword_t)EXC_MASK_ALL, (cpuword_t)masks, (cpuword_t)masksCnt, (cpuword_t)&remoteMsg->Head.msgh_remote_port, (cpuword_t)old_behaviors, (cpuword_t)old_flavors}));
    
    primitiveRead(&remoteMsg->Head.msgh_remote_port, &remoteExcetionHandlerPort, sizeof(remoteExcetionHandlerPort));

    if (!remoteExcetionHandlerPort) {
        //thread exception handler not set, it's probably the task exception handler then
        //i was told mach_task_self_ is 0x103 in every process
        assureMachclean(callfunc((void*)task_get_exception_ports,{(cpuword_t)mach_task_self_, (cpuword_t)EXC_MASK_ALL, (cpuword_t)masks, (cpuword_t)masksCnt, (cpuword_t)&remoteMsg->Head.msgh_remote_port, (cpuword_t)old_behaviors, (cpuword_t)old_flavors}));
        primitiveRead(&remoteMsg->Head.msgh_remote_port, &remoteExcetionHandlerPort, sizeof(remoteExcetionHandlerPort));
    }
    assure(remoteExcetionHandlerPort);
    
    //make sure fileds weren't overwritten by last call
    primitiveWrite(&remoteMsg->Head.msgh_local_port,&msg.Head.msgh_local_port,sizeof(msg)-(sizeof(mach_msg_bits_t)+sizeof(mach_msg_size_t)+sizeof(mach_port_t)));
    
    //i was told mach_task_self_ is 0x103 in every process
    assureMachclean(callfunc((void*)mach_port_allocate,{(cpuword_t)mach_task_self_, (cpuword_t)MACH_PORT_RIGHT_RECEIVE, (cpuword_t)&remoteMsg->thread.name}));

    
    //send mach message!
    
    //this call will throw, because we will first receive our mach message and afterwards the exception message will be queued
    try {
        ret = (kern_return_t)callfunc((void*)mach_msg, {(cpuword_t)remoteMsg,MACH_SEND_MSG|MACH_MSG_OPTION_NONE,msg.Head.msgh_size, 0, MACH_PORT_NULL,MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL});
    } catch (TKexception &e) {
        //all good, this was expected
    }
    
    //if this is assure fails, it means we failed to send our mach message :(
    assure(_emsg.head.msgh_id == 1336);
    
    //this is the remote name of the remote port we allocated
    remoteListenerPort = ((exception_raise_request*)&_emsg)->thread.name;
    
    //fix up our call primitive by receiving the pending exception message
    assureMach(mach_msg(&_emsg.head, MACH_RCV_MSG|MACH_RCV_LARGE, 0, sizeof(_emsg), _exceptionHandler, 0, MACH_PORT_NULL));

    
    /*
     remote: add send right
     */
    assureMachclean(callfunc((void*)mach_port_insert_right,{(cpuword_t)mach_task_self_, (cpuword_t)remoteListenerPort, (cpuword_t)remoteListenerPort, (cpuword_t)MACH_MSG_TYPE_MAKE_SEND}));

    /*
     remote: send port send right here
     */
    
    //construct mach msg locally (this time we send a send right to our task)
    msg.Head.msgh_bits = MACH_MSGH_BITS_SET(MACH_MSG_TYPE_COPY_SEND, 0, 0, MACH_MSGH_BITS_COMPLEX);
    msg.Head.msgh_id = 1337;
    
    msg.Head.msgh_remote_port = remoteExcetionHandlerPort;
    
    msg.Head.msgh_local_port = MACH_PORT_NULL;
    msg.Head.msgh_size = sizeof(msg); //a bit larger than really neccessary, but who cares
    msg.msgh_body.msgh_descriptor_count = 1;
    
    msg.thread.name = remoteListenerPort; //send over a port where we can send from local and receive on remote
    msg.thread.disposition = MACH_MSG_TYPE_COPY_SEND;
    msg.thread.type = MACH_MSG_PORT_DESCRIPTOR;
    
    //move over msg
    primitiveWrite(remoteMsg,&msg,sizeof(msg));
    
    //send mach message!
    //this call will throw, because we will first receive our mach message and afterwards the exception message will be queued
    try {
        ret = (kern_return_t)callfunc((void*)mach_msg, {(cpuword_t)remoteMsg,MACH_SEND_MSG|MACH_MSG_OPTION_NONE,msg.Head.msgh_size, 0, MACH_PORT_NULL,MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL});
    } catch (TKexception &e) {
        //all good, this was expected
    }
    
    //if this is assure fails, it means we failed to send our mach message :(
    assure(_emsg.head.msgh_id == 1337);

    
    //safe the port we will send our task port to
    localSenderPort = ((exception_raise_request*)&_emsg)->thread.name;
    
    //fix up our call primitive by receiving the pending exception message
    assureMach(mach_msg(&_emsg.head, MACH_RCV_MSG|MACH_RCV_LARGE, 0, sizeof(_emsg), _exceptionHandler, 0, MACH_PORT_NULL));

    /*
     //local: send over own task port
     */
    
    //construct another mach msg
    msg.Head.msgh_bits = MACH_MSGH_BITS_SET(MACH_MSG_TYPE_COPY_SEND, 0, 0, MACH_MSGH_BITS_COMPLEX);
    msg.Head.msgh_id = 1338;
    
    msg.Head.msgh_remote_port = localSenderPort; //this time we send to the remote process
    msg.Head.msgh_local_port = MACH_PORT_NULL;
    msg.Head.msgh_size = sizeof(msg); //a bit larger than really neccessary, but who cares
    msg.msgh_body.msgh_descriptor_count = 1;
    
    msg.thread.name = mach_task_self(); //let the remote process overtake me
    msg.thread.disposition = MACH_MSG_TYPE_COPY_SEND;
    msg.thread.type = MACH_MSG_PORT_DESCRIPTOR;
    
    //send over our task port
    assureMach(mach_msg((mach_msg_header_t*)&msg,MACH_SEND_MSG|MACH_MSG_OPTION_NONE, sizeof(msg), 0, MACH_PORT_NULL,MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL));

    
    /*
     //remote: receive msg on port
     */
    
    //no clue what's up with mach_msg and the size field o.O
    assureMach(callfunc((void*)mach_msg, {(cpuword_t)remoteMsg, MACH_RCV_MSG|MACH_RCV_LARGE, 0, sizeof(msg)+sizeof(msg.Head), remoteListenerPort, 0, MACH_PORT_NULL}));

    /*
     //remote: send back remote name for local task
     //local: store remote portname of local task locally
     */
    
    primitiveRead(&remoteMsg->thread.name, &_remoteSelf, sizeof(_remoteSelf));
        
    clean(false);
#undef allocScratchSpace
}



void takeover::readMem(void *remote, void *outAddr, size_t size){
    mach_vm_size_t out = size;
    if (_target) {
        assureMach(mach_vm_read_overwrite(_target, (mach_vm_address_t)remote , (mach_vm_size_t)size, (mach_vm_address_t) outAddr, &out));
    }else{
        assure(_remoteSelf);
        assureMach(callfunc((void*)mach_vm_write,{(cpuword_t)_remoteSelf, (cpuword_t)outAddr, (cpuword_t)remote, (cpuword_t)size}));
    }
}
void takeover::writeMem(void *remote, const void *inAddr, size_t size){
    if (_target) {
        assureMach(mach_vm_write(_target, (mach_vm_address_t)remote, (vm_offset_t)inAddr, (mach_msg_type_number_t)size));
    }else{
        assure(_remoteSelf);
        assure(_remoteScratchSpace && _remoteScratchSpaceSize > 8);
        assureMach(callfunc((void*)mach_vm_read_overwrite,{(cpuword_t)_remoteSelf, (cpuword_t)inAddr, (cpuword_t)size, (cpuword_t)remote, (cpuword_t)_remoteScratchSpace}));
    }
}
void *takeover::allocMem(size_t size){
    if (_target) {
        void *ret = 0;
        assureMach(mach_vm_allocate(_target, (mach_vm_address_t*)&ret, size, VM_FLAGS_ANYWHERE));
        assureMach(mach_vm_protect(_target, (mach_vm_address_t)ret, size, 1, VM_PROT_READ | VM_PROT_WRITE));
        return ret;
    }else{
        //overtakeme style
        void *ret = 0;
        assure(_remoteScratchSpace && _remoteScratchSpaceSize > 8);
        
        //i was told mach_task_self_ is 0x103 in every process
        assureMach(callfunc((void*)mach_vm_allocate, {(cpuword_t)mach_task_self_,(cpuword_t)_remoteScratchSpace,(cpuword_t)size,(cpuword_t)VM_FLAGS_ANYWHERE}));
        primitiveRead(_remoteScratchSpace, &ret, sizeof(ret));
        assureMach(callfunc((void*)mach_vm_protect,{(cpuword_t)mach_task_self_, (cpuword_t)ret, (cpuword_t)size, (cpuword_t)1, (cpuword_t)(VM_PROT_READ | VM_PROT_WRITE)}));
        return ret;
    }
}
void takeover::deallocMem(void *remote, size_t size){
    if (_target) {
        assureMach(mach_vm_deallocate(_target, (mach_vm_address_t)remote, (mach_vm_size_t)size));
    }else{
        //overtakeme style
        //i was told mach_task_self_ is 0x103 in every process
        assureMach(callfunc((void*)mach_vm_deallocate,{(cpuword_t)mach_task_self_, (cpuword_t)remote, (cpuword_t)size}));
    }
}

//noDrop means don't drop send right if we could
std::pair<int, kern_return_t> takeover::deinit(bool noDrop){
    kern_return_t err = 0;
    std::pair<int, kern_return_t> gerr = {0,0};
    if (_remoteScratchSpace) {
        try{
            deallocMem(_remoteScratchSpace, _remoteScratchSpaceSize);
        }catch(TKexception &e){
            error("deinit: dealloc exceptio: line=%d code=%lld what=%s",__LINE__,(cpuword_t)e.code(),e.what());
        }
        _remoteScratchSpace = NULL;
        _remoteScratchSpaceSize = 0;
    }
    
    if (_marionetteThread) {
        void *func_pthread_exit = NULL;
        my_thread_state_t *state = (my_thread_state_t*)(_emsg.data+0x24);
        kern_return_t ret = 0;
        
        func_pthread_exit  = dlsym(RTLD_NEXT, "pthread_exit");
        
        if (func_pthread_exit && !ret && !_isFakeThread) {
#if defined (__arm64__)
            state->__x[0] = 0;
            arm_thread_state64_set_pc_fptr(*state,func_pthread_exit);
#elif defined (__arm__)
            state->__r[0] = 0;
            state->__pc = (cpuword_t)func_pthread_exit;
#endif
            
            //clean terminate of kidnapped thread and resume
            exception_raise_state_reply reply = {};
            exception_raise_request* req = (exception_raise_request*)&_emsg;
            
            reply.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(_emsg.head.msgh_bits), 0);
            reply.new_stateCnt = MY_THREAD_STATE_COUNT;
            reply.Head.msgh_remote_port = _emsg.head.msgh_remote_port;
            reply.Head.msgh_local_port = MACH_PORT_NULL;
            reply.Head.msgh_id = _emsg.head.msgh_id + 100;
            
            reply.NDR = req->NDR;
            reply.RetCode = KERN_SUCCESS;
            reply.flavor = MY_THREAD_STATE;
            memcpy(reply.new_state, state, sizeof(my_thread_state_t));
            
            reply.Head.msgh_size = (mach_msg_size_t)(sizeof(exception_raise_state_reply) - 2456) + (((4 * reply.new_stateCnt))); //straight from MIG
            
            //resume
            err = mach_msg(&reply.Head, MACH_SEND_MSG|MACH_MSG_OPTION_NONE, (mach_msg_size_t)reply.Head.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
            if (err) {
                error("deinit: line=%d err=%llx s=%s",__LINE__,(cpuword_t)err,mach_error_string(err));
                if (!gerr.first) {gerr.first=__LINE__; gerr.second = err;}
            }
            _marionetteThread = MACH_PORT_NULL;
        }else{
            //kill it with fire
            err = thread_terminate(_marionetteThread);
            _marionetteThread = MACH_PORT_NULL;
            if (err) {
                error("deinit: line=%d err=%llx s=%s",__LINE__,(cpuword_t)err,mach_error_string(err));
                if (!gerr.first) {gerr.first=__LINE__; gerr.second = err;}
            }
        }
    }
    
    /* DO NOT CLEAN UP _remoteStack !!!
     
     Once the thread was alive, there is no going back.
     The memory is lost forever.
     
     New threads spawned by the system will access the mapping and crash the process if
     we remove it. Thus we just leave it there.
     */
    
    
    if (_exceptionHandler) {
        err = mach_port_destroy(mach_task_self(), _exceptionHandler);
        _exceptionHandler = MACH_PORT_NULL;
        if (err) {
            error("deinit: line=%d err=%llx s=%s",__LINE__,(cpuword_t)err,mach_error_string(err));
            if (!gerr.first) {gerr.first=__LINE__; gerr.second = err;}
        }
    }
    
    if (_target && !noDrop) {
        //drop one send right
        err = mach_port_deallocate(mach_task_self(), _target);
        _target = MACH_PORT_NULL;
        if (err) {
            error("deinit: line=%d err=%llx s=%s",__LINE__,(cpuword_t)err,mach_error_string(err));
            if (!gerr.first) {gerr.first=__LINE__; gerr.second = err;}
        }
    }
    
    return gerr;
}


takeover::~takeover(){
    auto err = deinit();
    if(err.first){
        error("[~takeover] deinit failed on line %d with code %d",err.first,err.second);
    }
}

std::string takeover::build_commit_count(){
    return VERSION_COMMIT_COUNT;
};

std::string takeover::build_commit_sha(){
    return VERSION_COMMIT_SHA;
};
