LearningWebServer
===============
此项目为一个学习TinyWebServer项目的记录，旨在亲自实现该项目。

## 01

##### 信号量：

```
sem_init函数用于初始化一个未命名的信号量
sem_destory函数用于销毁信号量
sem_wait函数将以原子操作方式将信号量减一,信号量为0时,sem_wait阻塞
sem_post函数以原子操作方式将信号量加一,信号量大于0时,唤醒调用sem_post的线程
```

> 成功返回0，失败返回errno
