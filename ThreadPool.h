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

	/* 自旋锁 */
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

	/* 线程池 */
	class ThreadPool {
	public:
		/* 析构 */
		~ThreadPool();

		/* 线程池主备类型 */
		enum class ThreadPoolType {
			MASTER = 0,
			SLAVE
		};

		/* 线程状态 */
		enum class ThreadState {
			BLOCKING = 0,
			READY,
			RUNNING
		};

	private:
		/* 当threads小于等于0时， threads为cpu + 1*/
		ThreadPool(int threads = 0, ThreadPoolType type = ThreadPoolType::MASTER);
		
		/* 禁止拷贝、赋值*/
		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

		/* 构建一个新的线程池 */
		static std::shared_ptr<ThreadPool> create(int threads = 0, ThreadPoolType type = ThreadPoolType::MASTER);

	private:
		/* 备用线程数量 */
		static int s_slaveThreadNum;

		/* 线程状态相关同步变量 */
		static int s_states;
		static std::mutex s_stateMutex;
		static std::condition_variable s_stateCondition;

		/* 任务队列相关同步变量 */
		static std::deque<std::function<void()> > s_tasks;
		static std::mutex s_taskMutex;
		static std::condition_variable s_taskCondition;

		/* 强行停止标识符 */
		static bool s_stop;

	public:
		/* 公共线程池 */
		static std::shared_ptr<ThreadPool> instance(int threads = 0);

		/* 当前线程是否属于线程池 */
		static bool isWorkerThread();

		/* 获取当前线程状态 */
		static ThreadPool::ThreadState getCurrentThreadState();

		/* 设置当前线程状态 */
		static void setCurrentThreadState(ThreadPool::ThreadState state);

		/* 添加任务 */
		template<class F, class... Args, typename R = typename std::decay<typename function_traits<F>::return_type>::type>
		std::future<R> enqueue(F&& f, Args&&... args);

		/* 运行一个线程 */
		template<class F, class... Args, typename R = typename std::decay<typename function_traits<F>::return_type>::type>
		static std::future<R> runWithPool(std::shared_ptr<ThreadPool> pool, F&& f, Args&&... args);

		template<class F, class... Args, typename R = typename std::decay<typename function_traits<F>::return_type>::type>
		static std::future<R> run(F&& f, Args&&... args);

	private:
		/* 线程队列（消费者队列）*/
		std::vector<std::thread> m_workers;
		/* 主备标识 */
		ThreadPoolType m_type;
		/* 停止标识 */
		bool m_stop;
		/* 备用线程池 */
		std::shared_ptr<ThreadPool> m_slaveThreadPool;
	};

	/* 添加任务 */
	template<class F, class... Args, typename R>
	std::future<R> ThreadPool::enqueue(F&& f, Args&&... args)
	{
		auto task = std::make_shared<std::packaged_task<R()> >(
			std::bind(std::forward<F>(f), std::ref(std::forward<Args>(args))...)
			);

		std::future<R> ret = task->get_future();

		/* 如果是从线程池中线程发起的任务，设置为BLOCKING状态，从备用线程中挑选一个开始运行 */
		std::shared_ptr<ThreadPool> needDeletePool;
		if (ThreadPool::isWorkerThread() && (ThreadPool::getCurrentThreadState() != ThreadState::BLOCKING))
		{
			ThreadPool::setCurrentThreadState(ThreadState::BLOCKING);
			std::unique_lock<std::mutex> lock(s_stateMutex);
			/* 如果备用线程不足，生成新的备用线程池 */
			if (s_slaveThreadNum <= 0)
			{
				if (m_slaveThreadPool)
				{
					/* 旧线程池释放任务放进线程池，这是个BLOCKING任务 */
					needDeletePool = m_slaveThreadPool;
					++s_states;
					--s_slaveThreadNum;
					s_stateCondition.notify_one();
				}
				m_slaveThreadPool = ThreadPool::create(m_workers.size(), ThreadPoolType::SLAVE);
			}
			/* 当前线程转换为BLOCKING状态后，通知备用线程改变状态 */
			++s_states;
			--s_slaveThreadNum;
			s_stateCondition.notify_one();
		}
		{
			/* 将任务放进任务队列 */
			std::unique_lock<std::mutex> lock(s_taskMutex);
			if (ThreadPool::isWorkerThread())
				s_tasks.emplace_front([task] { (*task)(); });
			else
				s_tasks.emplace_back([task] { (*task)(); });
			s_taskCondition.notify_one();
			/* 检查旧线程池是否需要释放 */
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

	/* 运行一个线程 */
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

	/* 线程池内线程的TLS数据 */
	class ThreadPoolTLS {
	public:
		/* 当前线程属于线程池标识 */
		thread_local static bool t_threadPoolFlag;
		/* 当前线程状态 */
		thread_local static ThreadPool::ThreadState t_threadState;
	};
}

#endif // FATE_THREAD_POOL_H