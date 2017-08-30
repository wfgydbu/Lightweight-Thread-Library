# Trade-offs and implementation
## Lwt isolation
lwt -> channel -> group -> kthd. Each kthd will have one lwt thread run on it when it's created. So each kthd will maintain a runqueue that contains all runnable lwt thread. lwt thread can be sent or received among kthds, so it's possible that a kthd has no runnable thread after some certain migration, in this case ,the kthd will be blocked, and it will be woke up when another kthd sent a lwt thread to it.

## Inter-kthd Comunication
My previous implementation already support both local and remote data transmission. If it's local, it's same as how we use channel before; however, a downside here is when the receiver is in an remote kthd, there's nothing current lwt thread can do except waiting for kthd's scheduler.

## Synchronization
To guarantee synchronization, I need to modify code to make sure at all points in time, shared objects can only be modified by one lwt on one kthd. I used critical section in the first version. later, I changed it into PS_CAS instruction, these objects include thread id counter, global threads' counter, and ringbuf in each channel.

## Test
A bunch of test cases.


# LMbench 
## Install  
On ubuntu:

```
apt-get build-dep lmbench
apt-get source lmbench
cd lmbench-3.0-a9/
tar xzvf lmbench-3.0-a9.tgz
cd lmbench-3.0-a9
make
```

If `make` works, you will find a bunch of binary files under `bin` directory and their description under `doc`. Here i will use four of them: `lat_ctx`, `lat_pipe`, `lat_proc` and `lat_select`.

+ `lat_ctx` measures context switching time for any reasonable number of  processes of any reasonable size. Since `lwt_yield` doesn't support any data buffer, so here I choose 0 kb data  with 1000 processes. `./lat_ctx 0 -s 1000`.
+ `lat_pipe` uses two processes communicating through a Unix pipe to measure interprocess communication latencies. The reported time is in microseconds per round trip inclues other time like context switch overhead. `./lat_pipe`.
+ `lat_proc` has three types of operation: `fork+exit`, `fork+execve` and `fork+/bin/sh -c`. Here only the second one makes sense to lwt, which measures the time it takes to create a new process and have the new process run a new program.`./lat_proc exec`.
+ `lat_select` measures the time to do a select on n file descriptors. Here I set n = 30. `./lat_select -n 30 file`.

## Convert
All results come from LMbench are using unit `ms`, they need to be converted to cpu cycles. 
### Get CPU frequency
```
cat /proc/cpuinfo
```
Check the content with label model name:
```
model name	: Intel(R) Core(TM) i7-6700HQ CPU @ 2.60GHz
```
This means my cpu has frequency of 2.6G Hz, so to convert the time from previous sections to CPU cycles, simply do a multiplication (use standard unit like s, Hz).

## result 

| No. |                System&Param                |      context_switch      | pipe/channel | proc/thd_create | event_notification |
|-----|:------------------------------------------:|:------------------------:|:------------:|:---------------:|:------------------:|
|     |                                            | 0kB data with 1000 procs |    2 procs   |     2 proces    |    30 group size   |
| 1   |           linux/lmbench(time:ms)           |           3.27           |    6.3019    |     104.0823    |       0.4720       |
| 2   | linux/lmbench(cycles: time x cpu frequency |          6,540,000         |   15,124,560   |    249,600,000    |       1,132,800      |
| 3   |                lwt_local hw3               |           40,290          |      296     |       465       |         219        |
| 4   |                  lwt_local                 |          1,328,138         |     7,255     |       1,338      |        5,126        |
| 5   |                 lwt_remote                 |         22,497,007         |     87,937    |      30,550      |        12,119       |

## A bit explanation about test cases
1. Line #1 is the results that lmbench run on a ubuntu VM, and instructions are listed above.
2. Line #2 is #1 multiples 2.4x10^6.
3. Line #3 and #4 are test cases run on ubuntu and Composite, test file is test_lmbench_compare.c.
4. Line #4 are test case run on Composite, test file is unilt_schedlib.c.
5. Some test functions must be run separately. 

# Golang
## Goroutines and Scheduler
ALl goroutimes will run under a traditional system process, and a goroutine won't share cpu with other goroutines until it's blocked.

To create a goroutine, simply add a "go" before the function. As same as lwt, go uses 4K stack, a stack pointer and instruction pointer.

The scheduler starts with a special goroutine named "sysmon", in my implementation, there exists similar thread, I call it idle thread.

There are three important concepts: M, P and G. M is OS process, P is goroutime, G is context switch space. Each P maintains a runnable queue, so as lwt.

More than lwt, go has more APIs for goroutine schedule. And go also has a feature know as stealing work.This makes go more complicated than lwt.

## Channels for communication and Multi-wait
Channel is a core part of Golang. And it's very similar with lwt. At least they shared the same design concept.

Each channel has a capacity represents a cached buffer for communication, (as chsz in lwt), if the capacity is zero, which means no cache,that's synchrony, otherwise it asynchrony.

Unlike lwt, channel in go uses a FIFO queue, to guarantee the order of rcv/snd data. (I use ringbuf in lwt).

Channel in golang can also use select to select a range of data, this is a bit similar to channel group, but not much. For example, the size of select statement is fixed.

Besides, A timer can be used to monitor a channel, for example, it will block that channel when the channel has nothing to select, but in lwt, I direclty block that thread, when a channel has nothing to receive.

## Cocurrency
In golang scheduler, we have three components, which are M, P, G. M,P,G represent kernel thread, context, and goroutine respectively. The number of P controls the degree of concurrency. Each P maintains a goroutine queue. And the G runs on the M. If the M0 does a syscall and is blocked, The P of M0 will move to another M like M1, which could be an idle M or just be created.  When M0 woke up, it will try to get a P from other M. If it doesn’t get the P, it will put its goroutine in a global runqueue, and then get back to the thread pool. P will periodically check the global runqueue to make sure these goroutines can be executed.

Decision about preemption can be made by the sysmon() background thread. This function will periodically do epoll operation and check if each P runs for too long. It it find a P has been in Psyscal state for more a sysmon time period and there is runnable task, it will switch P. If it find a P in Prunning state running for more than 10ms, it will set the stackguard of current G of P StackPreempt to let the G know it should give up the running opportunity.


# References
1. [LMbench](http://blog.yufeng.info/archives/753).
2. LMbench Man Page.
3. [The Go scheduler](http://morsmachine.dk/go-scheduler).