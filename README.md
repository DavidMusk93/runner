# task_runner

A simple task runner.

## what is a (defer)task?

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

typedef struct{
    job fn;
    void*arg;
    long run_at;
    int id;
    unsigned duration:31;
    unsigned repeated:1;
}defer_task;
```

`task` may be waitable, it could be blocked by

* give up its cpu;
* wait on the `runner`'s event fd.

`defer_task` is cancelable, which is marked by its raw pointer.

`task` is postted as `runner`'s queue entry, `defer_task` is organized as heap(minimal) entry in `defer_runner`.

## what is a (defer)runner?

a `runner` is a thread, do an infinite loop to wait for `task` arrival, then execute the `task`, again and again.

`defer_runner` is similar to `runner`, but execute task based on its timeout.

## run test

```
# x64 & linux
mkdir -p build
cd build && rm * -rf && cmake .. && make && ./runner
```

## todo

1. task wait return its result(`void*`);
2. reorganize `task` struct(make it opaque?);
3. complete test.
