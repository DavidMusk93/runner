#pragma once

#define MAIN() int main()
#define MAIN_EX(c,v) int main(int argc,char*argv[])

#define __log(fp,fmt,...) fprintf(fp,fmt "\n",##__VA_ARGS__)
#define log1(...) __log(stdout,__VA_ARGS__)
#define log2(...) __log(stderr,__VA_ARGS__)

#define infinite_loop() for(;;)
#define dimension_of(x) sizeof(x)/sizeof(*(x))

#define __attr(...) __attribute__((__VA_ARGS__))
#define __scoped_guard(fn) __attr(__cleanup__(fn))
#define __deprecated __attr(deprecated,unused)

#define __args_17(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,...) _16
#define __args_len(...) __args_17(__VA_ARGS__,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)

#define __concat(x,y) x##y
#define CONCAT(x,y) __concat(x,y)

#define __op_list_1(op,x) op(x)
#define __op_list_2(op,x,...) __op_list_1(op,x),__op_list_1(op,__VA_ARGS__)
#define __op_list_3(op,x,...) __op_list_1(op,x),__op_list_2(op,__VA_ARGS__)
#define __op_list_4(op,x,...) __op_list_1(op,x),__op_list_3(op,__VA_ARGS__)
#define __op_list_5(op,x,...) __op_list_1(op,x),__op_list_4(op,__VA_ARGS__)
#define __op_list_6(op,x,...) __op_list_1(op,x),__op_list_5(op,__VA_ARGS__)
#define __op_list_7(op,x,...) __op_list_1(op,x),__op_list_6(op,__VA_ARGS__)
#define __op_list_8(op,x,...) __op_list_1(op,x),__op_list_7(op,__VA_ARGS__)
#define __op_list_9(op,x,...) __op_list_1(op,x),__op_list_8(op,__VA_ARGS__)

#define __op_list_r_2(op,x,...) __op_list_1(op,__VA_ARGS__),__op_list_1(op,x)
#define __op_list_r_3(op,x,...) __op_list_r_2(op,__VA_ARGS__),__op_list_1(op,x)
#define __op_list_r_4(op,x,...) __op_list_r_3(op,__VA_ARGS__),__op_list_1(op,x)
#define __op_list_r_5(op,x,...) __op_list_r_4(op,__VA_ARGS__),__op_list_1(op,x)
#define __op_list_r_6(op,x,...) __op_list_r_5(op,__VA_ARGS__),__op_list_1(op,x)
#define __op_list_r_7(op,x,...) __op_list_r_6(op,__VA_ARGS__),__op_list_1(op,x)
#define __op_list_r_8(op,x,...) __op_list_r_7(op,__VA_ARGS__),__op_list_1(op,x)
#define __op_list_r_9(op,x,...) __op_list_r_8(op,__VA_ARGS__),__op_list_1(op,x)

#define OP_LIST(op,...) CONCAT(__op_list_,__args_len(__VA_ARGS__))(op,__VA_ARGS__)
#define OP_LIST_R(op,...) CONCAT(__op_list_r_,__args_len(__VA_ARGS__))(op,__VA_ARGS__)
