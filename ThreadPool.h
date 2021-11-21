#ifndef FATE_THREAD_POOL_H
#define FATE_THREAD_POOL_H

#include "FunctionTraits.h"
#include <vector>
#include <deque>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <future>
#include <functional>

namespace Fate {

	/* ������ */
	/*class SpinLock {
	public:
		inline void lock() {
			while (m_flag.test_and_set()) {
				std::this_thread::yield();
			}
		}
		inline void unlock() { m_flag.clear(); }

	private:
		std::atomic_flag m_flag = ATOMIC_FLAG_INIT;
	};*/

	/* �̳߳� */
	class ThreadPool {
	public:
		/* ���� */
		~ThreadPool();

		/* �̳߳��������� */
		enum class ThreadPoolType {
			MASTER = 0,
			SLAVE
		};

		/* �߳�״̬ */
		enum class ThreadState {
			BLOCKING = 0,
			READY,
			RUNNING
		};

	private:
		/* ��threadsС�ڵ���0ʱ�� threadsΪcpu + 1*/
		ThreadPool(int threads = 0, ThreadPoolType type = ThreadPoolType::MASTER);
		
		/* ��ֹ��������ֵ*/
		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

		/* ����һ���µ��̳߳� */
		static std::shared_ptr<ThreadPool> create(int threads = 0, ThreadPoolType type = ThreadPoolType::MASTER);

	private:
		/* �����߳����� */
		static int s_slaveThreadNum;

		/* �߳�״̬���ͬ������ */
		static int s_states;
		static std::mutex s_stateMutex;
		static std::condition_variable s_stateCondition;

		/* ����������ͬ������ */
		static std::deque<std::function<void()> > s_tasks;
		static std::mutex s_taskMutex;
		static std::condition_variable s_taskCondition;

		/* ǿ��ֹͣ��ʶ�� */
		static bool s_stop;

	public:
		/* �����̳߳� */
		static std::shared_ptr<ThreadPool> instance(int threads = 0);

		/* ��ǰ�߳��Ƿ������̳߳� */
		static bool isWorkerThread();

		/* ��ȡ��ǰ�߳�״̬ */
		static ThreadPool::ThreadState getCurrentThreadState();

		/* ���õ�ǰ�߳�״̬ */
		static void setCurrentThreadState(ThreadPool::ThreadState state);

		/* ������� */
		template<class F, class... Args, typename R = typename std::decay<typename function_traits<F>::return_type>::type>
		std::future<R> enqueue(F&& f, Args&&... args);

		/* ����һ���߳� */
		template<class F, class... Args, typename R = typename std::decay<typename function_traits<F>::return_type>::type>
		static std::future<R> runWithPool(std::shared_ptr<ThreadPool> pool, F&& f, Args&&... args);

		template<class F, class... Args, typename R = typename std::decay<typename function_traits<F>::return_type>::type>
		static std::future<R> run(F&& f, Args&&... args);

	private:
		/* �̶߳��У������߶��У�*/
		std::vector<std::thread> m_workers;
		/* ������ʶ */
		ThreadPoolType m_type;
		/* ֹͣ��ʶ */
		bool m_stop;
		/* �����̳߳� */
		std::shared_ptr<ThreadPool> m_slaveThreadPool;
	};

	/* ������� */
	template<class F, class... Args, typename R>
	std::future<R> ThreadPool::enqueue(F&& f, Args&&... args)
	{
		auto task = std::make_shared<std::packaged_task<R()> >(
			std::bind(std::forward<F>(f), std::ref(std::forward<Args>(args))...)
			);

		std::future<R> ret = task->get_future();

		/* ����Ǵ��̳߳����̷߳������������ΪBLOCKING״̬���ӱ����߳�����ѡһ����ʼ���� */
		std::shared_ptr<ThreadPool> needDeletePool;
		if (ThreadPool::isWorkerThread() && (ThreadPool::getCurrentThreadState() != ThreadState::BLOCKING))
		{
			ThreadPool::setCurrentThreadState(ThreadState::BLOCKING);
			std::unique_lock<std::mutex> lock(s_stateMutex);
			/* ��������̲߳��㣬�����µı����̳߳� */
			if (s_slaveThreadNum <= 0)
			{
				if (m_slaveThreadPool)
				{
					/* ���̳߳��ͷ�����Ž��̳߳أ����Ǹ�BLOCKING���� */
					needDeletePool = m_slaveThreadPool;
					++s_states;
					--s_slaveThreadNum;
					s_stateCondition.notify_one();
				}
				m_slaveThreadPool = ThreadPool::create(m_workers.size(), ThreadPoolType::SLAVE);
			}
			/* ��ǰ�߳�ת��ΪBLOCKING״̬��֪ͨ�����̸߳ı�״̬ */
			++s_states;
			--s_slaveThreadNum;
			s_stateCondition.notify_one();
		}
		{
			/* ������Ž�������� */
			std::unique_lock<std::mutex> lock(s_taskMutex);
			if (ThreadPool::isWorkerThread())
				s_tasks.emplace_front([task] { (*task)(); });
			else
				s_tasks.emplace_back([task] { (*task)(); });
			s_taskCondition.notify_one();
			/* �����̳߳��Ƿ���Ҫ�ͷ� */
			if (needDeletePool)
			{
				auto deleteTask = [needDeletePool] {
					ThreadPool::setCurrentThreadState(ThreadState::BLOCKING);
					((std::shared_ptr<ThreadPool>)needDeletePool).reset();
				};
				s_tasks.emplace_back(deleteTask);
				s_taskCondition.notify_one();
			}
		}

		return ret;
	}

	/* ����һ���߳� */
	template<class F, class... Args, typename R>
	static std::future<R> ThreadPool::runWithPool(std::shared_ptr<ThreadPool> pool, F&& f, Args&&... args)
	{
		std::shared_ptr<ThreadPool> ins = pool;
		if (!ins)
			ins = ThreadPool::instance();

		return ins->enqueue(std::forward<F>(f), std::forward<Args>(args)...);
	}

	template<class F, class... Args, typename R>
	static std::future<R> ThreadPool::run(F&& f, Args&&... args)
	{
		std::shared_ptr<ThreadPool> ins = ThreadPool::instance();
		return ThreadPool::runWithPool(ins, std::forward<F>(f), std::forward<Args>(args)...);
	}

	/* �̳߳����̵߳�TLS���� */
	class ThreadPoolTLS {
	public:
		/* ��ǰ�߳������̳߳ر�ʶ */
		thread_local static bool t_threadPoolFlag;
		/* ��ǰ�߳�״̬ */
		thread_local static ThreadPool::ThreadState t_threadState;
	};
}

#endif // FATE_THREAD_POOL_H