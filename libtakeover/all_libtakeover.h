//
//  all_libtakeover.h
//  libtakeover
//
//  Created by tihmstar on 24.02.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#ifndef all_libtakeover_h
#define all_libtakeover_h

#ifdef DEBUG
#define TAKEOVER_VERSION_COMMIT_COUNT "Debug"
#define TAKEOVER_VERSION_COMMIT_SHA "Build: " __DATE__ " " __TIME__
#endif

#define log(a ...) ({printf(a),printf("\n");})
#define info(a ...) ({printf(a),printf("\n");})
#define warning(a ...) ({printf("[WARNING] line %d: ",__LINE__), printf(a),printf("\n");})
#define error(a ...) ({printf("[Error] "),printf(a),printf("\n");})

#define safeFree(ptr) ({if (ptr) free(ptr),ptr=NULL;})

#define reterror(err) throw tihmstar::TKexception(__LINE__, err, LOCAL_FILENAME)
#define retcustomerror(err,except) throw tihmstar::except(__LINE__, err, LOCAL_FILENAME)
#define assure(cond) if ((cond) == 0) throw tihmstar::TKexception(__LINE__, "assure failed", LOCAL_FILENAME)
#define assureMach(kernRet) if (kernRet) throw tihmstar::TKexception(__LINE__, "assure failed", LOCAL_FILENAME,kernRet)
#define doassure(cond,code) do {if (!(cond)){(code);assure(cond);}} while(0)
#define retassure(cond, err) if ((cond) == 0) throw tihmstar::TKexception(__LINE__,err,LOCAL_FILENAME)
#define assureclean(cond) do {if (!(cond)){clean();assure(cond);}} while(0)
#define assureMachclean(kernRet) do {if (kernRet){clean();assureMach(kernRet);}} while(0)



#endif /* all_libtakeover_h */
