# task_runner
A simple task runner.

## what is a task?

a `task` is a struct, which wraps a function pointer(`void (*)(void*)`) and its argument(`void*`).

```
typedef void (*job)(void*);

typedef struct{
    job fn;
    void*arg;
    int wait;
    int id;
    queue q;
}task;
```

`task` may be waitable, it could be blocked by
* give up its cpu;
* wait on the `runner`'s event fd.

`task` is postted as `runner`'s queue entry.

## what is a runner?

a `runner` is a thread, do an infinite loop to wait for `task` arrival, then execute the `task`, again and again.

## run test

```
# x64 & linux
mkdir -p build
cd build && rm * -rf && cmake .. && make
./runner
```

## todo

1. task wait return its result(`void*`);
2. timer runner impl.;
3. complete test.
