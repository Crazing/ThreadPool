#ifndef FATE_CONCURRENT_H
#define FATE_CONCURRENT_H

#include "ThreadPool.h"
#include "FunctionTraits.h"

namespace Fate {
	
	//----------------------------------并发主体类型-----------------------------------
	class Concurrent {
	public:
		// map() on sequences
		template <typename T, typename MapFunctor, template <typename T, typename Alloc = std::allocator<T> > class InputSequence,
			typename R = typename std::decay<typename function_traits<MapFunctor>::return_type>::type>
		static std::future<void> map(InputSequence<T> &sequence, MapFunctor map)
		{
			auto func = [&sequence, &map]() {
				std::shared_ptr<ThreadPool> _pool = ThreadPool::instance();

				std::vector<std::future<R> > results;
				for (auto it = sequence.begin(); it != sequence.end(); ++it)
				{
					results.emplace_back(_pool->enqueue(map, *it));
				}
				for (auto&& result : results)
				{
					result.get();
				}
				return;
			};
			
			return ThreadPool::run(func);
		}

		// mapped() for sequences
		template <typename T, typename MapFunctor, template <typename T, typename Alloc = std::allocator<T> > class InputSequence,
			typename R = typename std::decay<typename function_traits<MapFunctor>::return_type>::type>
		static std::future<InputSequence<R> > mapped(InputSequence<T> &sequence, MapFunctor map)
		{
			auto func = [&sequence, &map]() ->InputSequence<R> {
				std::shared_ptr<ThreadPool> _pool = ThreadPool::instance();

				std::vector<std::future<R> > results;
				for (auto it = sequence.begin(); it != sequence.end(); ++it)
				{
					results.emplace_back(_pool->enqueue(map, *it));
				}
				InputSequence<R> ret;
				for (auto&& result : results)
				{
					ret.push_back(result.get());
				}
				return ret;
			};
			return ThreadPool::run(func);
		}

		// mappedReduced() for sequences.
		template <typename T, typename MapFunctor, typename ReduceFunctor, typename... Args,
			template <typename T, typename Alloc = std::allocator<T> > class InputSequence,
			typename R = typename std::decay<typename function_traits<MapFunctor>::return_type>::type,
			typename Arg0 = typename std::decay<typename function_traits<ReduceFunctor>::template args_type<0>>::type>
		static std::future<Arg0> mappedReduced(InputSequence<T> &sequence, MapFunctor map, ReduceFunctor reduce, Args&&... args)
		{
			Arg0 initialValue(std::forward<Args>(args)...);
			auto func = [&sequence, &map, &reduce, initialValue]() ->Arg0 {
				std::shared_ptr<ThreadPool> _pool = ThreadPool::instance();
			
				std::vector<std::future<R> > results;
				for (auto it = sequence.begin(); it != sequence.end(); ++it)
				{
					results.emplace_back(_pool->enqueue(map, *it));
				}
				Arg0 ret = initialValue;
				for (auto&& result : results)
				{
					reduce(ret, result.get());
				}
				return ret;
			};
			return ThreadPool::run(func);
		}
	};
}

#endif // FATE_CONCURRENT_H