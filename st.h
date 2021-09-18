#pragma once

typedef int(*compare)(const void*,const void*);

#define SWAP_POINTER(x,y) \
do{\
    __auto_type __x=&(x);/*avoid side effect*/\
    __auto_type __y=&(y);\
    if(__x==__y)break;\
    __auto_type __t=*__x;\
    *__x=*__y;\
    *__y=__t;\
}while(0)
