#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request, int state); //向请求队列中插入任务请求
    bool append_p(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理 信号量
    connection_pool *m_connPool;  //数据库
    int m_actor_model;          //模型切换
};
//线程池创建与回收
template <typename T>
threadpool<T>::threadpool( int actor_model, connection_pool *connPool, int thread_number, int max_requests): m_actor_model(actor_model),m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];//线程id初始化
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < thread_number; ++i)
    {    //循环创建线程，并将工作线程按要求进行运行  线程创建成功，"pthread_create" 函数返回零
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i])) //将线程进行分离后，不用单独对工作线程进行回收
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}
template <typename T>
bool threadpool<T>::append(T *request, int state)//将一个新的请求追加到线程池的任务队列中
{
    m_queuelocker.lock();  // 互斥锁
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}
/*向请求队列中添加任务
*通过list容器创建请求队列，向队列中添加时，通过互斥锁保证线程安全，
*添加完成后通过信号量提醒有任务要处理，最后注意线程同步。
*/
template <typename T>
bool threadpool<T>::append_p(T *request)  
{
    m_queuelocker.lock(); 
    //根据硬件，预先设置请求队列的最大值
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    //添加任务
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    //信号量提醒有任务要处理
    m_queuestat.post();
    return true;
}
//线程处理函数
//内部访问私有成员函数run，完成线程处理要求。
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    //将参数强转为线程池类，调用成员方法
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while (true)
    {   //信号量等待
        m_queuestat.wait();
        //被唤醒后先加互斥锁
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        //从请求队列中取出第一个任务
        //将任务从请求队列删除
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
            continue;
        //处理 HTTP 请求的逻辑，根据服务器的模式（非阻塞模式或者 Reactor 模式）来处理请求。
        if (1 == m_actor_model) //如果服务器是 Reactor 模式（m_actor_model 等于 1）
        {
            if (0 == request->m_state)
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else//如果服务器是非阻塞模式，则直接通过 connectionRAII 类
        {   //创建一个 MySQL 连接对象 mysqlcon，然后调用 process() 函数处理该请求。
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            //process(模板类中的方法,这里是http类)进行处理
            request->process();
        }
        /*无论是在 Reactor 模式还是非阻塞模式下，都会使用 connectionRAII 类
        创建一个 MySQL 连接对象 mysqlcon，以确保在请求处理完成后，MySQL 连接对象能够被正确释放，避免资源泄露。*/
    }
}
#endif
