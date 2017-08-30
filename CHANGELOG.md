**[2017/4/3]**: All the comments that I think is worthy to change in the lists below have been solved directly, or been solved when solving another issue, indirectly.

```
[PERF]    75 <- fork/join
[PERF]    26 <- yield
[TEST] thread creation/join/scheduling
[PERF]   110 <- snd+rcv (buffer size 0)
[PERF]   113 <- snd+rcv (buffer size 1)
[PERF]   110 <- snd+rcv (buffer size 2)
[PERF]    34 <- asynchronous snd->rcv (buffer size 100)
[TEST] multisend (channel buffer size 0)
[TEST] multisend (channel buffer size 100)
[TEST] ring buffer...SUCCESS
[TEST] multi wait (group size 30)...SUCCESS
```


**[2017/3/2]**: Todo List or change plan:

Important:

1. Adjust pool for allocate/deallocate memory of any size. So this pool takes charge for all memory allocation later. (I'm working on it, maybe it's done when you are reviewing my code, or might not.)
2. Try to simplify DS of channel for msg pool is done.
3. When recycle a thread from pool, making sure wipe off all the data. (The logic is right, but I memset it to 0, the perf reduces quit a lot.)
4. If trying to tab align make sure that smart tabs aren't enabled so that indentation doesn't look off. (This is important to me, my code always seems strange even if I use tabs, I'll look into settings of my IDE.)

Others:

1. `lwt_chan_deref()`, thread should be removed from channel when the channel is deref()-ed.
2. Go through codes again to make sure all one-line code has braces.
3. Use low-case characters for `LWT_INFO_*` stuff, make it an enum. (?)
4. function name can be shorter. (??)
5. Use a single pointer but not char array in DS definition. (? Not sure it's wise.)
6. More error checking code. (? I'll look into it.)
7. Optimize `lwt_yield()` logic (Done.)
8. Make a function to get the next active thread in `__lwt_schedule()`. (? I don't think so. These codes only used in this function, once.)


**[2017/2/27]**: HW2 commit before due.
Except contents from HW2, things updated:

1. Optimize `lwt_join()` logic，avoid issue that thread have been directed yielded to before dying;
2. Modify `lwt_state` from #define to enum;
3. Add braces to all one-line blocks;
4. Correct the function name `__lwt_schedule()`;
5. Use `<sys/types.h>`;
6. Remove dispatch.o;
7. Rewrite `lwt_info()`, use integers to collect informations;
8. Make stack size configurable;
9. Optimize `lwt_yield()`, `lwt_current()`
10. Use `<sys/queue.h>` for thread lists, pool and channel;
11. Adjust some variable names;

The output for HW2 is:
```
[PERF]    76 <- fork/join
[PERF]    47 <- yield
[TEST] thread creation/join/scheduling
[PERF]   138 <- snd+rcv (buffer size 0)
```

**[2017/2/6]**: Final commit before due. Tried to optimize `__lwt_dispatch()`, get limited progress. Seems performance can't be improved unless the logic is changed. Anyway, so far the performance of fork/join can be controlled around **100**, more or less than 20; performance of yield can be controlled around **50**, more or less than 10. 
```
[PERF]   108 <- fork/join
[PERF]    33 <- yield
[TEST] thread creation/join/scheduling
```

**[2017/2/6]**: Bug fixed, current output:
```
[PERF]    88 <- fork/join
[PERF]    27 <- yield
[TEST] thread creation/join/scheduling
```

**[2017/2/6]**: Add some optimazations, current output:
```
[PERF]   203 <- fork/join
[PERF]    23 <- yield
[TEST] thread creation/join/scheduling
```

**[2017/2/5]**: Current output:
```
[PERF]   449 <- fork/join
[PERF]    21 <- yield
[TEST] thread creation/join/scheduling
```