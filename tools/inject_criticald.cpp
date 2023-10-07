#include <stdio.h>
#include <stdint.h>
#include <mach/mach.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <string>
#include <spawn.h>

#include <libtakeover/libtakeover.hpp>
#include <libgeneral/macros.h>

using namespace tihmstar;

int posix_spawnattr_set_ptrauth_task_port_np(posix_spawnattr_t * __restrict attr,mach_port_t port);

/* Set platform binary flag */
#define FLAG_PLATFORMIZE (1 << 1)
void platformizeme() {
    void* handle = NULL;
    const char *dlsym_error = NULL;
    
    assure(handle = dlopen("/usr/lib/libjailbreak.dylib", RTLD_LAZY));
    
    // Reset errors
    dlerror();
    typedef void (*fix_entitle_prt_t)(pid_t pid, uint32_t what);
    fix_entitle_prt_t ptr = (fix_entitle_prt_t)dlsym(handle, "jb_oneshot_entitle_now");
    
    if ((dlsym_error = dlerror())) {
        reterror("failed to link \"jb_oneshot_entitle_now\" with error: %s",dlsym_error);
    }
    
    ptr(getpid(), FLAG_PLATFORMIZE);
}

void inject(uint32_t pid, const char *dylib){
    task_t remoteTask = MACH_PORT_NULL;
    kern_return_t ret = 0;
    cpuword_t rt = 0;
    void *dylibPathAddr = NULL;
    
    doassure(!(ret = task_for_pid(mach_task_self(), pid, &remoteTask)),[pid]{
        printf("Failed to get task for pid %u!\n", pid);
    }());

    printf("Remote task: 0x%x\n", remoteTask);

    printf("platformizing myself...");
    try {
        platformizeme();
        printf("ok!\n");
    } catch (tihmstar::exception &e) {
        printf("fail!\n");
        e.dump();
        printf("platformizing failed! Continuing without platformization (injection to platform processes may fail!)\n");
    }
    
    printf("Initing takeover...");
    tihmstar::takeover mytk(remoteTask);
    printf("ok!\n");

    printf("kidnapping remote thread...");
    mytk.kidnapThread();
    printf("ok!\n");

    assure(dylibPathAddr = mytk.allocMem(0x100 + strlen(dylib) + 1));
    printf("Dylib Path Addr: 0x%llx\n", 0x100 + (cpuword_t)dylibPathAddr);

    mytk.writeMem((void *)(0x100 + (cpuword_t)dylibPathAddr), dylib, strlen(dylib) + 1);

    printf("Calling dlopen...\n");
    rt = mytk.callfunc((void *)&dlopen, {0x100 + (cpuword_t)dylibPathAddr, 2});

    mytk.deallocMem(dylibPathAddr, 0x100 + strlen(dylib) + 1);

    printf("dlopen returned 0x%08llx\n",rt);
    
    if (cpuword_t error = mytk.callfunc((void *)&dlerror, {})){
        
        cpuword_t len = mytk.callfunc((void *)&strlen, {error});
        char *local_cstring = (char *)malloc(len + 1);
        
        mytk.readMem((void *)error, local_cstring, len + 1);
        
        printf("Error is %s\n", local_cstring);
        free(local_cstring);
        reterror("Failed to inject!");
    }
    
    printf("No error occurred!\n");
}


MAINFUNCTION
int main_r(int argc, const char* argv[]){
    uint32_t pid = 0;
    const char *dylib = NULL;
    printf("inject_criticald version=%s commit=%s\n",takeover::build_commit_count().c_str(),takeover::build_commit_sha().c_str());
    if (argc < 3){
        printf("Usage: inject_criticald <pid> <dylib>\n");
        return 0;
    }
    
    pid = atoi(argv[1]);
    dylib = argv[2];
    
    printf("inject(%d,%s)\n",pid,dylib);
    
    inject(pid,dylib);
    printf("Injection succeeded!\n");

    return 0;
}
