#include <stdint.h>
#include <mach/mach.h>
#include <dlfcn.h>
#include <pthread.h>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <unistd.h>

#include <libtakeover/libtakeover.hpp>
#include <libgeneral/macros.h>

using namespace tihmstar;

#define MAXCOMLEN       16              /* max command name remembered */
#define PROC_PIDTASKALLINFO             2

struct proc_bsdinfo {
    uint32_t                pbi_flags;              /* 64bit; emulated etc */
    uint32_t                pbi_status;
    uint32_t                pbi_xstatus;
    uint32_t                pbi_pid;
    uint32_t                pbi_ppid;
    uid_t                   pbi_uid;
    gid_t                   pbi_gid;
    uid_t                   pbi_ruid;
    gid_t                   pbi_rgid;
    uid_t                   pbi_svuid;
    gid_t                   pbi_svgid;
    uint32_t                rfu_1;                  /* reserved */
    char                    pbi_comm[MAXCOMLEN];
    char                    pbi_name[2 * MAXCOMLEN];  /* empty if no name is registered */
    uint32_t                pbi_nfiles;
    uint32_t                pbi_pgid;
    uint32_t                pbi_pjobc;
    uint32_t                e_tdev;                 /* controlling tty dev */
    uint32_t                e_tpgid;                /* tty process group id */
    int32_t                 pbi_nice;
    uint64_t                pbi_start_tvsec;
    uint64_t                pbi_start_tvusec;
};
struct proc_taskinfo {
    uint64_t                pti_virtual_size;       /* virtual memory size (bytes) */
    uint64_t                pti_resident_size;      /* resident memory size (bytes) */
    uint64_t                pti_total_user;         /* total time */
    uint64_t                pti_total_system;
    uint64_t                pti_threads_user;       /* existing threads only */
    uint64_t                pti_threads_system;
    int32_t                 pti_policy;             /* default policy for new threads */
    int32_t                 pti_faults;             /* number of page faults */
    int32_t                 pti_pageins;            /* number of actual pageins */
    int32_t                 pti_cow_faults;         /* number of copy-on-write faults */
    int32_t                 pti_messages_sent;      /* number of messages sent */
    int32_t                 pti_messages_received;  /* number of messages received */
    int32_t                 pti_syscalls_mach;      /* number of mach system calls */
    int32_t                 pti_syscalls_unix;      /* number of unix system calls */
    int32_t                 pti_csw;                /* number of context switches */
    int32_t                 pti_threadnum;          /* number of threads in the task */
    int32_t                 pti_numrunning;         /* number of running threads */
    int32_t                 pti_priority;           /* task priority*/
};
struct proc_taskallinfo {
    struct proc_bsdinfo     pbsd;
    struct proc_taskinfo    ptinfo;
};

int (*proc_listallpids)(void * buffer, int buffersize);
int (*proc_pidinfo)(int pid, int flavor, uint64_t arg, void *buffer, int buffersize);



int inject_command(int pid, const char *command){
    cpuword_t rt = 0;
    kern_return_t ret = 0;
    void *straddr = NULL;


    task_t remoteTask = MACH_PORT_NULL;
    doassure(!(ret = task_for_pid(mach_task_self(), pid, &remoteTask)),[pid]{
        printf("Failed to get task for pid %u!\n", pid);
    }());

    printf("Remote task: 0x%x\n", remoteTask);

    printf("Initing takeover...");
    tihmstar::takeover mytk(remoteTask);
    printf("ok!\n");

    printf("kidnapping remote thread...");
    mytk.kidnapThread();
    printf("ok!\n");

    assure(straddr = mytk.allocMem(0x100 + strlen(command) + 1));
    printf("straddr: 0x%llx\n", 0x100 + (cpuword_t)straddr);

    mytk.writeMem((void *)(0x100 + (cpuword_t)straddr), command, strlen(command) + 1);

    printf("Calling system...\n");
    rt = mytk.callfunc((void *)dlsym(RTLD_NEXT, "system"), {0x100 + (cpuword_t)straddr});

    mytk.deallocMem(straddr, 0x100 + strlen(command) + 1);

    printf("system returned 0x%08llx\n",rt);
    
    return rt;
}


int main(int argc, const char * argv[]) {
    bool doWait = false;
    int err = -1;
    
    if (argc > 1 && strcmp(argv[1],"-w") == 0) {
        printf("Wait flag detected, waiting for process to spawn\n");
        argv++;
        argc--;
        doWait = true;
    }
    
    if (argc < 3){
        printf("Make a different process run a shell command\n");
        printf("Usage: %s [-w] <process> <command>\n",argv[0]);
        return 0;
    }
    
    assure(proc_listallpids = (int (*)(void *, int))dlsym(RTLD_NEXT, "proc_listallpids"));
    assure(proc_pidinfo = (int (*)(int, int, uint64_t, void *, int))dlsym(RTLD_NEXT, "proc_pidinfo"));

    const char *processname = argv[1];
    const char *command = argv[2];
    
    printf("looking for process=%s\n",processname);
    printf("command to inject='%s'\n",command);

    if (doWait) {
        pid_t p = 0;
        if ((p = fork()) == -1) {
            printf("failed to fork!\n");
            exit(1);
        }
        if (p != 0) {
            //parent
            exit(0);
        }else{
            //child
            printf("daemonized!\n");
        }
    }
    
    do{
        int num_pids = 0;
        pid_t *pids = NULL;

        num_pids = proc_listallpids(NULL, 0);
        pids = (pid_t *)calloc(num_pids, sizeof(pid_t));
        num_pids = proc_listallpids(pids, num_pids * sizeof(pids));

        for (int i = 0; i < num_pids; i++) {
            struct proc_taskallinfo pidinfo = {};
            proc_pidinfo(pids[i], PROC_PIDTASKALLINFO, 0,  &pidinfo, sizeof(pidinfo));
            printf("checking pid=%d process=%s\n",pids[i],pidinfo.pbsd.pbi_comm);
            if (strcmp(processname, pidinfo.pbsd.pbi_comm) == 0) {
                printf("found process with pid=%d injecting...\n",pids[i]);
                try {
                    err = inject_command(pids[i],command);
                    printf("Injection succeeded!\n");
                } catch (TKexception &e) {
                    printf("[TKexception]:\n");
                    printf("what=%s\n",e.what());
                    printf("code=%d\n",e.code());
                    printf("commit count=%s:\n",e.build_commit_count().c_str());
                    printf("commit sha  =%s:\n",e.build_commit_sha().c_str());
                    printf("\n");
                    printf("moreCode=%llu:\n",e.moreCode());

                    err = e.code();
                    printf("Injection failed!\n");
                }
                doWait = false;
                break;
            }
        }
        free(pids);
        sleep(1);
    }while (doWait);
    
    
    
    if (err) {
        printf("failed!\n");
    }else{
        printf("success!\n");
    }
    
    return err;
}
