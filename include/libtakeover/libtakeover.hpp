//
//  libtakeover.hpp
//  libtakeover
//
//  Created by tihmstar on 24.02.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#ifndef libtakeover_hpp
#define libtakeover_hpp

#include <mach/mach.h>
#include <vector>
#include <libtakeover/TKexception.hpp>
#include <functional>

#if defined (__arm64__)
    typedef uint64_t cpuword_t;
#elif defined (__arm__)
    typedef uint32_t cpuword_t;
#endif

namespace tihmstar {
    typedef struct {
        mach_msg_header_t head;
        mach_msg_body_t msgh_body;
        char data[1024];
    } exceptmsg_t;
    class takeover{
        exceptmsg_t _emsg;
        mach_port_t _target;
        mach_port_t _marionetteThread;
        mach_port_t _exceptionHandler;
        mach_vm_address_t _remoteStack;
        const mach_vm_size_t _remoteStackSize = 0x4000;
        bool _isFakeThread;
        bool _isRemotePACed;
        
        mach_port_t _remoteSelf;
        void *_remoteScratchSpace;
        size_t _remoteScratchSpaceSize;
        std::function<cpuword_t(cpuword_t ptr)> _signptr_cb;
        
        
        std::pair<int, kern_return_t>  deinit(bool noDrop = false);
        takeover();
        void primitiveWrite(void *remote, void *inAddr, size_t size);
        void primitiveRead(void *remote, void *outAddr, size_t size);

    public:
        takeover(const mach_port_t target, std::function<cpuword_t(cpuword_t ptr)> signptr_cb = nullptr);
        takeover(const takeover &) = delete; //no copy constructor
        takeover(takeover &&); //move constructor
        
        static takeover takeoverWithExceptionHandler(mach_port_t exceptionHandler);
        
        void setSignptrCB(std::function<cpuword_t(cpuword_t ptr)> signptr_cb);
        
        cpuword_t callfunc(void *addr, const std::vector<cpuword_t> &xtr);
        
        void kidnapThread();
        void overtakeMe();

        void *getRemoteSym(const char *sym);
        void readMem(void *remote, void *outAddr, size_t size);
        void writeMem(void *remote, const void *inAddr, size_t size);
        void *allocMem(size_t size);
        void deallocMem(void *remote,size_t size);

        
        ~takeover();
        
        static std::string build_commit_count();
        static std::string build_commit_sha();
        
        static bool targetIsPACed(const mach_port_t target);
    };
    
};


#endif /* libtakeover_hpp */
