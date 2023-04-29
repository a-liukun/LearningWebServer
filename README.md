LearningWebServer
===============
此项目为一个学习TinyWebServer项目的记录，旨在亲自实现、复刻该项目。

## 基础知识

##### 信号量：

```
sem_init函数用于初始化一个未命名的信号量
sem_destory函数用于销毁信号量
sem_wait函数将以原子操作方式将信号量减一,信号量为0时,sem_wait阻塞
sem_post函数以原子操作方式将信号量加一,信号量大于0时,唤醒调用sem_post的线程
```

> 成功返回0，失败返回errno

特点：

> 信号量是一种特殊的变量，它只能取自然数值并且只支持两种操作：等待(P)和信号(V).假设有信号量SV，对其的P、V操作如下：

```c++
P，如果SV的值大于0，则将其减一；若SV的值为0，则挂起执行
V，如果有其他进行因为等待SV而挂起，则唤醒；若没有，则将SV值加一
```



## 线程池

#### （一）基础知识：

##### 1.线程池特点

- 空间换时间,浪费服务器的硬件资源,换取运行效率.
- 池是一组资源的集合,这组资源在服务器启动之初就被完全创建好并初始化,这称为静态资源.
- 当服务器进入正式运行阶段,开始处理客户请求的时候,如果它需要相关的资源,可以直接从池中获取,无需动态分配.
- 当服务器处理完一个客户连接后,可以把相关的资源放回池中,无需执行系统调用释放资源.



##### 2.五种I/O模型

- **阻塞IO**:调用者调用了某个函数，等待这个函数返回，期间什么也不做，不停的去检查这个函数有没有返回，必须等这个函数返回才能进行下一步动作
- **非阻塞IO**:非阻塞等待，每隔一段时间就去检测IO事件是否就绪。没有就绪就可以做其他事。非阻塞I/O执行系统调用总是立即返回，不管时间是否已经发生，若时间没有发生，则返回-1，此时可以根据errno区分这两种情况，对于accept，recv和send，事件未发生时，errno通常被设置成eagain
- **信号驱动IO**:linux用套接口进行信号驱动IO，安装一个信号处理函数，进程继续运行并不阻塞，当IO时间就绪，进程收到SIGIO信号。然后处理IO事件。
- **IO复用**:linux用select/poll函数实现IO复用模型，这两个函数也会使进程阻塞，但是和阻塞IO所不同的是这两个函数可以同时阻塞多个IO操作。而且可以同时对多个读操作、写操作的IO函数进行检测。知道有数据可读或可写时，才真正调用IO操作函数
- **异步IO**:linux中，可以调用aio_read函数告诉内核描述字缓冲区指针和缓冲区的大小、文件偏移及通知的方式，然后立即返回，当内核将数据拷贝到缓冲区后，再通知应用程序。

> **注意：阻塞I/O，非阻塞I/O，信号驱动I/O和I/O复用都是同步I/O。同步I/O指内核向应用程序通知的是就绪事件，比如只通知有客户端连接，要求用户代码自行执行I/O操作，异步I/O是指内核向应用程序通知的是完成事件，比如读取客户端的数据后才通知应用程序，由内核完成I/O操作。**



##### 3.事件处理模式：

- reactor模式中，主线程(**I/O处理单元**)只负责监听文件描述符上是否有事件发生，有的话立即通知工作线程(**逻辑单元** )，读写数据、接受新连接及处理客户请求均在工作线程中完成。通常由**同步I/O**实现。
- proactor模式中，主线程和内核负责处理读写数据、接受新连接等I/O操作，工作线程仅负责业务逻辑，如处理客户请求。通常由**异步I/O**实现。



##### 4.同步I/O模拟proactor模式

同步I/O模型的工作流程如下（epoll_wait为例）：

- 主线程往epoll内核事件表注册socket上的读就绪事件。
- 主线程调用epoll_wait等待socket上有数据可读
- 当socket上有数据可读，epoll_wait通知主线程,主线程从socket循环读取数据，直到没有更多数据可读，然后将读取到的数据封装成一个请求对象并插入请求队列。
- 睡眠在请求队列上某个工作线程被唤醒，它获得请求对象并处理客户请求，然后往epoll内核事件表中注册该socket上的写就绪事件
- 主线程调用epoll_wait等待socket可写。
- 当socket上有数据可写，epoll_wait通知主线程。主线程往socket上写入服务器处理客户请求的结果。



##### 5.半同步/半反应堆工作流程（以Proactor模式为例）

- 主线程充当异步线程，负责监听所有socket上的事件
- 若有新请求到来，主线程接收之以得到新的连接socket，然后往epoll内核事件表中注册该socket上的读写事件
- 如果连接socket上有读写事件发生，主线程从socket上接收数据，并将数据封装成请求对象插入到请求队列中
- 所有工作线程睡眠在请求队列上，当有任务到来时，通过竞争（如互斥锁）获得任务的接管权



#### （二）注意点：

1.**pthread_create陷阱**

```c++
#include <pthread.h>
int pthread_create (pthread_t *thread_tid,                 //返回新生成的线程的id
                    const pthread_attr_t *attr,         //指向线程属性的指针,通常设置为NULL
                    void * (*start_routine) (void *),   //处理线程函数的地址
                    void *arg);                         //start_routine()中的参数
```

函数原型中的第三个参数，为函数指针，指向处理线程函数的地址。该函数，要求为静态函数。如果处理线程函数为类成员函数时，需要将其设置为**静态成员函数**。



2.**线程处理函数**

```c++
//线程处理函数
template<typename T>
void *threadpool<T>::worker(void *arg){
	//将参数强转为线程池类，调用成员方法
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}
```



（三）线程池框架

1. 定义线程类
2. 线程池创建与回收
3. 向请求队列添加任务
4. 线程处理函数



## http连接处理

#### 基础知识

#### （一）epoll

全称 eventpoll，是 linux 内核实现 IO 多路转接 / 复用（IO multiplexing）的一个实现。epoll 是 select 和 poll 的升级版，相较于这两个前辈，epoll 改进了工作方式，因此它更加高效。

- 对于待检测集合select和poll是基于线性方式处理的，**epoll是基于红黑树**来管理待检测集合的。

- select和poll每次都会线性扫描整个待检测集合，集合越大速度越慢，**epoll使用的是回调机制**，效率高，处理效率也不会随着检测集合的变大而下降
- select和poll工作过程中存在内核/用户空间数据的频繁拷贝问题，**在epoll中内核和用户区使用的是共享内存（**基于mmap内存映射区实现），省去了不必要的内存拷贝。
- 程序猿需要对select和poll返回的集合进行判断才能知道哪些文件描述符是就绪的，通过**epoll可以直接得到已就绪的文件描述符集合**，无需再次检测
- 使用 epoll 没有最大文件描述符的限制，仅受系统中进程能打开的最大文件数目限制
- **select和poll**都只能工作在相对低效的LT模式下，**epoll**则可以工作在ET高效模式，并且epoll还支持EPOLLONESHOT事件，该事件能进一步减少可读、可写和异常事件被触发的次数。

> 注：当多路复用的文件数量庞大、IO 流量频繁的时候，一般不太适合使用 select () 和 poll ()，这种情况下 select () 和 poll () 表现较差，推荐使用 epoll ()。



#### 2、操作函数

```cpp
#include <sys/epoll.h>
// 创建epoll实例，通过一棵红黑树管理待检测集合
int epoll_create(int size);
->创建一个指示epoll内核事件表的文件描述符，该描述符将用作其他epoll系统调用的第一个参数，size不起作用。

// 管理红黑树上的文件描述符(添加、修改、删除)
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
->epfd：为epoll_creat的句柄
->op：表示动作，用3个宏来表示：
		EPOLL_CTL_ADD (注册新的fd到epfd)，
		EPOLL_CTL_MOD (修改已经注册的fd的监听事件)，
		EPOLL_CTL_DEL (从epfd删除一个fd)；
->event：告诉内核需要监听的事件

// 检测epoll树中是否有就绪的文件描述符 返回就绪的文件描述符个数
int epoll_wait(int epfd, struct epoll_event * events, int maxevents, int timeout);
->events：用来存内核得到事件的集合，
->maxevents：告之内核这个events有多大，这个maxevents的值不能大于创建epoll_create()时的size，
->timeout：是超时时间
        -1：阻塞
        0：立即返回，非阻塞
        >0：指定毫秒
->返回值：成功返回有多少文件描述符就绪，时间到时返回0，出错返回-1
```

其中，event是epoll_event结构体指针类型，表示内核所监听的事件，具体定义如下：

```cpp
struct epoll_event {
    __uint32_t events; /* Epoll events */
    epoll_data_t data; /* User data variable */
};
```

- events描述事件类型，其中epoll事件类型有以下几种

- - EPOLLIN：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）
  - EPOLLOUT：表示对应的文件描述符可以写
  - EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）
  - EPOLLERR：表示对应的文件描述符发生错误
  - EPOLLHUP：表示对应的文件描述符被挂断；
  - EPOLLET：将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)而言的
  - EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里



#### 3、 epoll 的工作模式

- LT水平触发模式

- - epoll_wait检测到文件描述符有事件发生，则将其通知给应用程序，应用程序可以不立即处理该事件。
  - 当下一次调用epoll_wait时，epoll_wait还会再次向应用程序报告此事件，直至被处理

- ET边缘触发模式

- - epoll_wait检测到文件描述符有事件发生，则将其通知给应用程序，应用程序必须立即处理该事件
  - 必须要一次性将数据读取完，使用非阻塞I/O，读取到出现eagain

- EPOLLONESHOT

- - 一个线程读取某个socket上的数据后开始处理数据，在处理过程中该socket上又有新数据可读，此时另一个线程被唤醒读取，此时出现两个线程处理同一个socket
  - 我们期望的是一个socket连接在任一时刻都只被一个线程处理，通过epoll_ctl对该文件描述符注册epolloneshot事件，一个线程处理socket时，其他线程将无法处理，**当该线程处理完后，需要通过epoll_ctl重置epolloneshot事件**



#### 4、HTTP报文格式

HTTP报文分为请求报文和响应报文两种，每种报文必须按照特有格式生成，才能被浏览器端识别。

- 请求报文：浏览器端向服务器发送的报文
  - 种类：GET、POST
  
    > GET和POST请求报文的区别之一是有无消息体部分，GET请求没有消息体
  
  - 组成：请求行（request line）、请求头部（header）、空行和请求数据(主体)
  
- 响应报文：服务器处理后返回给浏览器端的报文。
  - 组成：状态行、消息报头、空行和响应正文。



#### 5、有限状态机

- 是一种抽象的理论模型，它能够把有限个变量描述的状态变化过程，以可构造可验证的方式呈现出来。
- 有限状态机可以通过if-else,switch-case和函数指针来实现，从软件工程的角度看，主要是为了封装逻辑。

```cpp
 STATE_MACHINE(){
 2    State cur_State = type_A;
     while(cur_State != type_C){
         Package _pack = getNewPackage();
         switch(){
             case type_A:
                 process_pkg_state_A(_pack);
                 cur_State = type_B;
                 break;
            case type_B:
                process_pkg_state_B(_pack);
                cur_State = type_C;
                break;
        }
    }
}
```

- 该状态机包含三种状态：type_A，type_B和type_C。其中，type_A是初始状态，type_C是结束状态。
- 状态机的当前状态记录在cur_State变量中，逻辑处理时，状态机先通过getNewPackage获取数据包，然后根据当前状态对数据进行处理，处理完后，状态机通过改变cur_State完成状态转移。
- 有限状态机一种逻辑单元内部的一种高效编程方法，在服务器编程中，服务器可以根据不同状态或者消息类型进行相应的处理逻辑，使得程序逻辑清晰易懂。



#### 6、http处理流程

三步走：

- 浏览器端发出http连接请求，主线程创建http对象接收请求并将所有数据读入对应buffer，将该对象插入任务队列，工作线程从任务队列中取出一个任务进行处理。
- 工作线程取出任务后，调用process_read函数，通过主、从状态机对请求报文进行解析。
- 解析完之后，跳转do_request函数生成响应报文，通过process_write写入buffer，返回给浏览器端。



## 日志系统

#### 基础知识

- **`日志`**，由服务器自动创建，并记录运行状态，错误信息，访问数据的文件。
- **`同步日志`**，日志写入函数与工作线程串行执行，由于涉及到I/O操作，当单条日志比较大的时候，同步模式会阻塞整个处理流程，服务器所能处理的并发能力将有所下降，尤其是在峰值的时候，写日志可能成为系统的瓶颈。
- **`生产者-消费者模型`**，并发编程中的经典模型。以多线程为例，为了实现线程间数据同步，生产者线程与消费者线程共享一个缓冲区，其中生产者线程往缓冲区中push消息，消费者线程从缓冲区中pop消息。
- **`阻塞队列`**，将生产者-消费者模型进行封装，使用循环数组实现队列，作为两者共享的缓冲区。
- **`异步日志`**，将所写的日志内容先存入阻塞队列，写线程从阻塞队列中取出内容，写入日志。
- **`单例模式`**，最简单也是被问到最多的设计模式之一，保证一个类只创建一个实例，同时提供全局访问的方法。



#### 单例模式

> 单例模式作为最常用的设计模式之一，保证一个类仅有一个实例，并提供一个访问它的全局访问点，该实例被所有程序模块共享。
>
> 实现思路：私有化它的构造函数，以防止外界创建单例类的对象；使用类的私有静态指针变量指向类的唯一实例，并用一个公有的静态方法获取该实例。
>
> 单例模式有两种实现方法，分别是懒汉和饿汉模式。顾名思义，懒汉模式，即非常懒，不用的时候不去初始化，所以在第一次被使用时才进行初始化；饿汉模式，即迫不及待，在程序运行时立即初始化。

- **线程安全懒汉模式**

```cpp
 class single{
 private:
     static pthread_mutex_t lock;
     single(){
         pthread_mutex_init(&lock, NULL);
     }
     ~single(){}
 
 public:
    static single* getinstance();

};
pthread_mutex_t single::lock;
single* single::getinstance(){
    pthread_mutex_lock(&lock);
    static single obj;
    pthread_mutex_unlock(&lock);
    return &obj;
}
```

- **饿汉模式**

> 饿汉模式不需要用锁，就可以实现线程安全。原因在于，在程序运行时就定义了对象，并对其初始化。之后，不管哪个线程调用成员函数getinstance()，都只不过是返回一个对象的指针而已。所以是线程安全的，不需要在获取实例的成员函数中加锁。

```cpp
 class single{
 private:
     static single* p;
     single(){}
     ~single(){}
 
 public:
     static single* getinstance();
 
};
single* single::p = new single();
single* single::getinstance(){
    return p;
}

//测试方法
int main(){
    single *p1 = single::getinstance();
    single *p2 = single::getinstance();

    if (p1 == p2)
        cout << "same" << endl;

    system("pause");
    return 0;
}
```

> 饿汉模式虽好，但其存在隐藏的问题，在于非静态对象（函数外的static对象）在不同编译单元中的初始化顺序是未定义的。如果在初始化完成之前调用 getInstance() 方法会返回一个未定义的实例。

