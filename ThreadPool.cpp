#include "ThreadPool.h"

namespace Fate {

	thread_local bool ThreadPoolTLS::t_threadPoolFlag = false;
	thread_local ThreadPool::ThreadState ThreadPoolTLS::t_threadState = ThreadPool::ThreadState::READY;
	int ThreadPool::s_slaveThreadNum = 0;
	int ThreadPool::s_states = 0;
	std::mutex ThreadPool::s_stateMutex;
	std::condition_variable ThreadPool::s_stateCondition;
	std::deque<std::function<void()> > ThreadPool::s_tasks;
	std::mutex ThreadPool::s_taskMutex;
	std::condition_variable ThreadPool::s_taskCondition;
	bool ThreadPool::s_stop = false;

	ThreadPool::ThreadPool(int threads, ThreadPoolType type):
		m_stop(false)
	{
		/* 默认线程数为核心数 + 1*/
		if (threads <= 0)
			threads = std::thread::hardware_concurrency() + 1;
		/* 如果为备用线程池，备用线程数增加 */
		if (type == ThreadPoolType::SLAVE)
			s_slaveThreadNum += threads;
		m_type = type;

		/* 循环创建线程 */
		for (int i = 0; i < threads; ++i)
		{
			auto worker = [this, type] {
				ThreadPoolTLS::t_threadPoolFlag = true;
				if (type == ThreadPoolType::MASTER)
					ThreadPoolTLS::t_threadState = ThreadState::RUNNING;

				while (true)
				{
					std::function<void()> task;
					/* 非是运行线程，就是备用线程*/
					if (ThreadPoolTLS::t_threadState != ThreadState::RUNNING)
					{
						std::unique_lock<std::mutex> lock(s_stateMutex);
						/* 如果当前线程是BLOCKING，转变成READY模式 */
						if (ThreadPoolTLS::t_threadState == ThreadState::BLOCKING)
						{
							ThreadPoolTLS::t_threadState = ThreadState::READY;
							++s_slaveThreadNum;
						}
						s_stateCondition.wait(lock, [this] {
							return m_stop || s_states > 0;
						});
						if (m_stop && s_states <= 0)
						{
							--s_slaveThreadNum;
							break;
						}
						--s_states;
						ThreadPoolTLS::t_threadState = ThreadState::RUNNING;
					}
					/* 运行线程监听任务队列 */
					if (ThreadPoolTLS::t_threadState == ThreadState::RUNNING)
					{
						std::unique_lock<std::mutex> lock(s_taskMutex);
						s_taskCondition.wait(lock, [this] {
							return s_stop || !(s_tasks.empty());
						});
						if (s_stop && s_tasks.empty())
							break;				
						task = std::move(s_tasks.front());
						s_tasks.pop_front();
					}

					/* 执行任务 */
					task();
				}
			};

			m_workers.emplace_back(worker);
		}
	}

	ThreadPool::~ThreadPool()
	{
		m_stop = true;
		s_stateCondition.notify_all();
		if (!ThreadPoolTLS::t_threadPoolFlag)
		{
			s_stop = true;
			s_taskCondition.notify_all();
		}
		for (std::thread& worker : m_workers)
			worker.join();
	}

	std::shared_ptr<ThreadPool> ThreadPool::create(int threads, ThreadPoolType type)
	{
		return std::shared_ptr<ThreadPool>(new ThreadPool(threads, type));
	}

	std::shared_ptr<ThreadPool> ThreadPool::instance(int threads)
	{
		static std::shared_ptr<ThreadPool> s_singleton;
		static std::mutex s_singletonMutex;

		if (!s_singleton)
		{
			std::unique_lock<std::mutex> lock(s_singletonMutex);
			if (!s_singleton)
			{
				s_singleton = std::shared_ptr<ThreadPool>(new ThreadPool(threads));
			}
		}
		return s_singleton;
	}

	bool ThreadPool::isWorkerThread()
	{
		return ThreadPoolTLS::t_threadPoolFlag;
	}

	ThreadPool::ThreadState ThreadPool::getCurrentThreadState()
	{
		return ThreadPoolTLS::t_threadState;
	}

	void ThreadPool::setCurrentThreadState(ThreadPool::ThreadState state)
	{
		ThreadPoolTLS::t_threadState = state;
	}
}