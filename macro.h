#pragma once

#define MAIN() int main()
#define MAIN_EX(c,v) int main(int argc,char*argv[])

#define __log(fp,fmt,...) fprintf(fp,fmt "\n",##__VA_ARGS__)
#define log1(...) __log(stdout,__VA_ARGS__)
#define log2(...) __log(stderr,__VA_ARGS__)

#define infinite_loop() for(;;)
