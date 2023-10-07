//
//  tkexception.hpp
//  libtakeover
//
//  Created by tihmstar on 24.02.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#ifndef tkexception_hpp
#define tkexception_hpp

#include <libgeneral/macros.h>
#include <libgeneral/exception.hpp>
#include <stdint.h>

namespace tihmstar {
    class TKexception : public tihmstar::exception{
        using tihmstar::exception::exception;
    };

    class TKexception_Bad_PC_Magic : public tihmstar::TKexception{
        using TKexception::TKexception;
    };

};


#endif /* tkexception_hpp */
