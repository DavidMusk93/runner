#pragma once

enum {
#define __field(t, x) STATE_##t=(1L<<x)
    STATE_NOTHINGNESS,
    __field(INITIALIZED, 0),
    __field(TRANSFERRED, 1),
    __field(IDLE, 2),
    __field(WORK, 3),
    __field(TERMINATED, 4),
    STATE_COMPOUND = STATE_IDLE | STATE_WORK,
#undef __field
};
