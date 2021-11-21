#include <iostream>
#include <vector>
#include "Concurrent.h"

int main()
{
	{
		auto asyncTask = [](int param) {
			std::cout << "This is an async task! param is " << param << std::endl;
		};
		int i = 9;
		auto future = Fate::ThreadPool::run(asyncTask, 9);
		future.wait();

	}
	{
		std::vector<int> num = { 1, 2, 3, 4, 5, 6 };
		auto mapFunc = [](int& element) {
			element += 10;
		};
		auto future = Fate::Concurrent::map(num, mapFunc);
		future.wait();

		std::cout << "map test begin: " << std::endl;
		for (const auto& element : num)
		{
			std::cout << element << " ";
		}
		std::cout << std::endl;
		std::cout << "map test end!" << std::endl << std::endl;
	}

	{
		std::vector<int> num = { 1, 2, 3, 4, 5, 6 };
		auto mapFunc = [](int element) {
			element += 10;
			return element;
		};
		auto future = Fate::Concurrent::mapped(num, mapFunc);
		auto ret = future.get();

		std::cout << "mapped test begin: " << std::endl;
		std::cout << "Origin: " << std::endl;
		for (const auto& element : num)
		{
			std::cout << element << " ";
		}
		std::cout << std::endl;
		std::cout << "Return: " << std::endl;
		for (const auto& element : ret)
		{
			std::cout << element << " ";
		}
		std::cout << std::endl;
		std::cout << "mapped test end!" << std::endl << std::endl;
	}
	{
		std::vector<int> num = { 1, 2, 3, 4, 5, 6 };
		auto mapFunc = [](int& element) {
			return element + 10;
		};
		auto reduceFunc = [](int& reduceRet, int mapRet)
		{
			reduceRet += mapRet;
		};
		auto future = Fate::Concurrent::mappedReduced(num, mapFunc, reduceFunc, 19);
		auto ret = future.get();

		std::cout << "mappedReduced test begin: " << std::endl;
		std::cout << "ret: " << ret << std::endl;
		std::cout << "mappedReduced test end!" << std::endl << std::endl;
	}
	

	return 0;
}