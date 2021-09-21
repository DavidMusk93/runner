#pragma once

typedef int(*compare)(const void*,const void*);

#define SWAP_POINTER(x,y) \
do{\
    void**__x=&(x);/*avoid side effect*/\
    void**__y=&(y);\
    if(__x==__y)break;\
    void*__t=*__x;\
    *__x=*__y;\
    *__y=__t;\
}while(0)
