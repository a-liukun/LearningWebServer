#pragma once
#include <pthread.h>
#include <list>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
//定义线程池
template <typename T>
class threadpool
{
public:
    threadpool(int actor_model, connection_pool *connPool, int thread_number =8, int max_request =10000);
    ~threadpool();
private:
    static void *worker(void *arg);
    void run();
    bool append(T *request, int state); // 添加新线程到线程池
    bool append_p(T *request);
private:
    int m_thread_number; //线程池中线程数
    int m_max_requests;  //最大请求数
    pthread_t *m_threads; //描述线程池的数组
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker; //保护请求队列的互斥锁
    sem m_queuestat; // 是否有任务要处理
    connection_pool *m_connPool; //数据库连接池？
    int m_actor_model;  //模型切换
};

//线程池创建与回收
template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number =8, int max_request =10000)
:m_actor_model(actor_model),m_thread_number(thread_number),m_max_requests(max_request),m_threads(NULL),m_connection(connPool)
{
    if (thread_number <=0 || max_request <=0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads)
        throw std::exception();
    for(int i = 0; i< thread_number; ++i)
    {//循环创建线程，并将工作线程按要求进行运行  线程创建成功，"pthread_create" 函数返回零
        if(pthread_create(m_threads+i,NULL,worker,this)!=0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])) //将线程进行分离后，不用单独对工作线程进行回收
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

//添加线程
template<typename T>
bool threadpool<T>::append(T *request, int state){
    m_queuelocker.lock();// 互斥锁
    if(m_workqueue >= m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
bool threadpool<T>::append_p(T *request){
    m_queuelocker.lock();
    if(m_workqueue >= m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

//线程处理函数
template<typename T>
void *threadpool<T>::worker(void *arg){
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run()
{
    while (true)
    {
        //信号量等待 是否有任务处理
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }

        //从队列中取出第一个任务 并 删除
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request)
            continue;
        
        //处理 HTTP 请求的逻辑，根据服务器的模式（非阻塞模式或者 Reactor 模式）来处理请求。
        if(1 == m_actor_model)
        {
            /*
            

            
            
            */
        }else{
            // 从连接池中取出一个数据库连接
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            //调用process()函数处理请求
            request->process();
        }
    }
    
}


