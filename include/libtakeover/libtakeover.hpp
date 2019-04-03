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
        
        std::pair<int, kern_return_t>  deinit(bool noDrop = false);
        
    public:
        takeover(mach_port_t target);
        
        uint64_t callfunc(void *addr, const std::vector<uint64_t> &x);
        
        bool kidnapThread();
        
        void readMem(void *remote, size_t size, void *outAddr);
        void writeMem(void *remote, size_t size, void *inAddr);
        void *allocMem(size_t size);
        void deallocMem(void *remote,size_t size);

        ~takeover();

    };
    
};


#endif /* libtakeover_hpp */
