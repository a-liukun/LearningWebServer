#pragma once
#include <pthread.h>
#include <exception>
#include <semaphore.h>

//信号量
class sem
{
public:
    sem()
    {
        if(sem_init(&m_sem,0,0) != 0)
        {
            throw std::exception();
        }
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    sem(int num)
    {
        if(sem_init(&m_sem,0,num)!=0)
        {
            throw std::exception();
        }
    }
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    bool post()
    {
        return sem_post(&m_sem) ==0;
    }
private:
    sem_t m_sem;
};

//互斥锁
class locker
{
private:
    pthread_mutex_t m_mutex;
public:
    locker()
    {   //pthread_mutex_init：如果初始化成功，该函数返回0，否则返回一个非0的错误码。
        if(pthread_mutex_init(&m_mutex, NULL)!=0) 
        {
            throw std::exception();
        }
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }
};
