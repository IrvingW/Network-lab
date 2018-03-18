# README
Implementation of a GO BACK N Reliable Data Transfer protocal

## 目录
[TOC] 

## 新添加文件说明
### frame.h/.cc
我所设计的包的结构如下所示
```	
	frame structure
		（total 128 byte）

	checksum ================= 2 byte
	paylaod size   =========== 1 byte(8 bit)    // 2**8=256
	seq_or_ack	========== 1 byte
		|
		|----- 2 bit ack mark(00 for normal package, 01 for ack, 10 for nak)
		|----- 6 bit for seq numebr
        payload ================== 124 bytes
```
* payload size： 是有效载荷的大小，之所以有这一项是因为有的时候messge的零头并不能填满一个* packet，这一项就记录了有效信息的长度。
* checksum： 这一项是校验和，我采用的是比较简答的Internet checksum，所以需要16 bit也就是 2个bytes
* seq_ack: 为了提高有效载荷， 这一项将两个变量拼接在一起组成。分别是 sequence number（6 bits） 和 ACK mark （2 bits）。这两个值的不同组合所产生的意义不相同。在第二节我会详细介绍。


在原有代码的基础上，我新添加了两个文件，分别是frame.h 和 frame.cc。其中在头文件中我定义了一下一些宏和结构

    ```
    #define HEADER_SIZE 4       // packet中除去有效载荷的部分，包含了包的基本信息
                                // 以下是四种ACK mark的值
    #define NORMAL 0            
    #define ACK 1
    #define NAK 2
    #define REQ 3

    #define MAX_SEQ 20          // 为了解决乱序的问题，所以将最大的sequence number
    #define WINDOW_SIZE 10      // 设置成 window size * 2 + 1


    // 这个功能用的很多，为了提高性能所以设成了宏
    #define inc(k) if(k == MAX_SEQ) k = 0; else k = k+1;
    
    typedef unsigned int seq_nr; 

    // frame 结构
    struct frame{
        short checksum;			// 16 bit
        char payload_size;		// 8 bit
        char seq_ack;			// 8 bit
        char payload[RDT_PKTSIZE - HEADER_SIZE];
    };
    ```

另外frame.h 中还定义了三种方法
```
// pack a seq_ack, ack_mark(00 for normal, 01 for ack, 10 for nak)
char pack_seq_ack(int ack_mark, seq_nr seq);

/* fill with Internet checksum*/
void fill_checksum(struct frame *f);

/* verify the checksum*/
bool verify_checksum(struct frame *f);
```

### MakeFile
我对MakeFile也进行了修改，使他可以编译我新添加的文件

```
rdt_sender.o: 	rdt_struct.h rdt_sender.h frame.h

rdt_receiver.o:	rdt_struct.h rdt_receiver.h frame.h

frame.o: frame.h

rdt_sim.o: 	rdt_struct.h

rdt_sim: rdt_sim.o rdt_sender.o rdt_receiver.o frame.o
```


## 设计细节

### Sender
#### msg_buffer
为了解决上层传递消息到RDT层太快的问题，我定义了这个链表式的结构
```
struct msg_buffer{
	message * msg;
	msg_buffer *next;
};
```
这个结构的主要作用是用来缓存没来得及发送的message，两个全局指针分别指向这个链表的头部和尾部
```
struct msg_buffer * msg_buffer_head = NULL;		// used to fetch from buffer
struct msg_buffer * msg_buffer_tail = NULL;		// used to store into buffer
```
#### packet_buffer
```
struct packet * packet_buffer[MAX_SEQ + 1];			// frame buffer
```
这个结构是用来对已经发送但是还没有收到ACK的包进行缓存的地方，如果超时的话需要从这个结构中按索引取出缓存的包重新发送。

#### timer
由于只有一个计时器，所以我用另一种方法来对放在缓存中的包存放在buffer中的时间进行计时
```
int buffer_timer[MAX_SEQ + 1];		// timer for packets have not been acknowledge
 					// timer is initialized with -1
```
buffer_timmer这个结构会里面存储的是MIN_TIME的倍数，所以是一个int类型，当达到TIME_OUT时就需要重发这个包
```
#define MIN_TIME 0.05

#define SLEEP_TIME_MAX 3 
```
MIN_TIME这个变量是我定义的时间的最小粒度。
在Sender的Init方法中我开启了一个计时器，时间设置位MIN_TIME，当TIME_OUT的时候，就表示过了一个时间粒度，根据这个对buffer_timer进行操作。最后再重新启动一个同样的时钟

这个时钟如果一直被启动的话，模拟器就不会终止。一开始没有看sim文件中代码的我并不知道这一点。所以我设置了sleep time这个变量，这个变量的作用就是如果检测到一段时间(SLEEP_TIME)内上层没有动作，而且msg buffer中的message都已经被发送完成，并且收到了ACK。就自动将时钟停止退出模拟器。

#### window
我主要用了几个“指针”分别记录window的两端的移动情况
```
seq_nr ack_expected;	// the oldest frame has not been ack
seq_nr next_to_send; 	// the seq number of next frame to send
```
ack expected指向window的头部
next to send 指向window的尾部
另外还有一个buffered cnt 变量用来记录window打开的大小。

在window设计上我考虑到会有乱序带来的问题， 如果新的有效序号和老的序号有重叠，那么旧的序号过来就会错误的ack掉新的序号指向的包。所以我的窗口的大小是10， 而序号滑动的范围是

#### Sender_FromUpperLayer
在这个函数中，当RDT层接收到上层发来的message，首先要做的是将message复制下来保存到msg buffer中去，然后会调用一个deal message 函数，在这个函数中会检查当前窗口是否满了，如果满了那么不发送新的packet，否则会将message切分成packet进行发送。

#### Sender_Timeout
没过一个最小粒度时间都会调用这个函数，这个函数主要做以下几个工作
1. 遍历buffer timer更新timers的时间
2. 检查window头部的packet是否在buffer中timeout了，如果是那么重新发送一遍窗口中的所有包
2. 决定是否退出模拟器

#### Sender_FromLowerLayer
这个函数主要用来处理Receiver端发送回来的ACK包，并且做相应的处理
发过来的包有以下几种情况：
1. ACK
表示Receiver端正确接受到了一个包，Sender如果发现返回来的序号落在了窗口之中，将窗口向前滑动至相应位置

2. NAK
表示Receiver段收到了一个corrupted的包，Sender检查序号，如果在窗口之中进行相应的滑动，然后重新发送packet buffer中从毁坏掉的包开始后面的所有包

3. REQ
这种情况是我后来添加的，因为我想到一种极端的情况：如果在丢包率特别大的情况下，一个窗口的所有ACK包都在中途被丢掉了，之后Sender即使多次发送window中的所有包，都不会有用处，程序就会卡死。所以我让Receiver在收到不是当前想要的包时返回Sender端一个sequence nuber，表示Receiver端当前想要的包，如果Sender发现这个值和自己的window的尾端的值相同，那么说明所有ACK包在中途都被丢掉了，Sender清除掉缓存的包，接着发送新的包。

### Receiver

#### Receiver_FromLowerLayer
这端我给它设置的窗口大小是一，也就是没有缓存，只有唯一一个包是当前可以接受的包
进来之后首先检查是否是当前需要的包，如果不是丢掉，并且返回一个REQ包告诉Sender当前需要的包的序号。
然后检查checksum值是否正确，如果不正确也丢掉，并且返回一个NAK包。
检查都通过，那么就接受这个包，这里我没有做什么修改。
接收完成后返回Sender一个ACK信息。

### checksum
我用的是Internet checksum的算法，具体实现如下
```
    short *buf = (short *)f;	
	buf ++;						// jump over checksum part
	int times = (RDT_PKTSIZE - 2) /2;
	// calculate the sum
	unsigned long long sum = 0;
	for(int i = 0; i < times; i++){
		sum += *buf++;
	}
	// turn to 16 bit
	while(sum >> 16){
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	short checksum = ~sum;	
	sum = checksum + sum;
```
这里一开始我是选择如果packet没有填满则只对有效信息的部分进行校验求和的，但是测试时候发现这样做使得容错性有一定的下降，最后还是改回了这中低效的方式。


## 使用方法
```
> make clean
> make
> ./rdt_sim <sim_time> <mean_msg_arrivalint> <mean_msg_size> <outoforder_rate> <loss_rate> <corrupt_rate> <tracing_level>

```

## 测试情况
### 参数都是30%
做三次测试

#### 第一次
```
## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 30.00%
	average loss rate is 30.00%
	average corrupt rate is 30.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.

At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 5337.85s: sender finalizing ...
At 5337.85s: receiver finalizing ...

## Simulation completed at time 5337.85s with
	982395 characters sent
	982395 characters delivered
	226805 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.

```

#### 第二次
```
## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 30.00%
	average loss rate is 30.00%
	average corrupt rate is 30.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.

At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 5426.70s: sender finalizing ...
At 5426.70s: receiver finalizing ...

## Simulation completed at time 5426.70s with
	1000654 characters sent
	1000654 characters delivered
	229958 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.

```
#### 第三次
```
## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 30.00%
	average loss rate is 30.00%
	average corrupt rate is 30.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.

At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 5396.85s: sender finalizing ...
At 5396.85s: receiver finalizing ...

## Simulation completed at time 5396.85s with
	987657 characters sent
	987657 characters delivered
	228890 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.

```

### 参数都是15%
做三次测试

#### 第一次
```
## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 15.00%
	average loss rate is 15.00%
	average corrupt rate is 15.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.

At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 2227.80s: sender finalizing ...
At 2227.80s: receiver finalizing ...

## Simulation completed at time 2227.80s with
	1000939 characters sent
	1000939 characters delivered
	137610 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```

#### 第二次
```
## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 15.00%
	average loss rate is 15.00%
	average corrupt rate is 15.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.

At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 2188.75s: sender finalizing ...
At 2188.75s: receiver finalizing ...

## Simulation completed at time 2188.75s with
	1002410 characters sent
	1002410 characters delivered
	135547 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.

```

#### 第三次
```
## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 15.00%
	average loss rate is 15.00%
	average corrupt rate is 15.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.

At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 2176.75s: sender finalizing ...
At 2176.75s: receiver finalizing ...

## Simulation completed at time 2176.75s with
	994250 characters sent
	994250 characters delivered
	134645 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.

```

## reference
* Computer Networks (Fifth Edition) Andrew S. Tanenbaum


## 总结
这次实验我在原有代码的基础上实现了一个使用 Go Back N 策略的可靠数据传输协议，实现过程中主要遇到了几个问题：
1. 一开始并不知道这个一个单线程的程序，所以最初在Sender_FromUpperLayer中的实现是如果当前窗口是满的话，就一直while循环，这个是看了sim模块的代码才弄明白模拟器的消息机制，一开始太想当然了。
2. 调的最久的一个bug，由于我是将seq number和ack mark拼接成了一个字节，在取出的时候由于忘记了逻辑右移和算数右移的事情，导致代码里藏了一个很难发现的bug，这里我反思自己第一遍写的时候太不认真了。

最后测试的结果还可以吧，但是当我将前两个参数，也就是丢包和乱序都调到0.9的时候都不会出错，只有包的毁坏率到达50%以后就很难测试通过。我觉的Internet checksum还是不够优秀吧，之后会继续看看还有什么校验和的算法。
通过这次实验主要是锻炼了我gdb调试的技术，并且彻底弄懂了go back n的实现原理，还是非常由收获的。 
