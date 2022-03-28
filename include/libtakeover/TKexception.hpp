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
#include <stdint.h>

namespace tihmstar {
    class TKexception : public tihmstar::exception{
        using tihmstar::exception::exception;
    };
};


#endif /* tkexception_hpp */
