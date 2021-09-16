#pragma once

#define DATETIME_BUF_LEN 64

typedef const char*(*datetime_format)(char buf[DATETIME_BUF_LEN],int sec,int us);

const char*now_string(char buf[DATETIME_BUF_LEN],datetime_format format);
long now_ms();

int __pid();
int __tid();

int nap(int ms);
