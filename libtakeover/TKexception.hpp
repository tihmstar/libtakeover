//
//  tkexception.hpp
//  libtakeover
//
//  Created by tihmstar on 24.02.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#ifndef tkexception_hpp
#define tkexception_hpp

#include <liboffsetfinder64/exception.hpp>
#include "all_libtakeover.h"
#include <stdint.h>

namespace tihmstar {
    class TKexception : public tihmstar::exception{
        uint64_t _moreCode;
    public:
        TKexception(int code, std::string err, std::string filename, uint64_t moreCode = 0)
            :exception(code,err,filename),_moreCode(moreCode){
                error("initing exception with build: count=%s sha=%s",build_commit_count().c_str(),build_commit_sha().c_str());
            }
        
        uint64_t moreCode() const{
            return _moreCode;
        }

        std::string build_commit_count() const override{
            return LIBTAKEOVER_VERSION_COMMIT_COUNT;
        };
        
        std::string build_commit_sha() const override{
            return LIBTAKEOVER_VERSION_COMMIT_SHA;
        };
        
    };
};


#endif /* tkexception_hpp */
