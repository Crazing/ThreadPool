[中文版入口](README_cn.md)

# ThreadPool

This is a pure, high-performance thread pool with no spontaneous deadlocks that you can use in your project.

## Method of use

- ### Basic usage: **ThreadPool::run**
  ```c_cpp
    auto asyncTask = [] {
        std::cout << "This is an async task!" << std::endl;
    };
    auto future = Fate::ThreadPool::run(asyncTask);
    future.wait();
  ```
  
  The thread pool is a singleton that internally initializes ** CPU +1** threads to work on. Of course, this amount can be controlled:  
  ```c_cpp
    auto pool = Fate::ThreadPool::instance(20);
  ```
  
  This method must be called before using any of the thread pool methods, because the thread pool is created only once, and the return value is a unique instance of the thread pool, and the argument is the number of threads you want to create.
  
  The **run** method, and any method that adds asynchronous tasks as described below, can accept any form of function definition. **C functions, function references, function Pointers, function objects, including lambda expressions, std::function objects, std::bind expression objects, custom functors, and more. **
  
  Therefore, we only need to focus on the asynchronous task logic itself.
  
  The **run** method, and any subsequent methods that add asynchronous tasks, return a **std::future\<T\>** , which is the type c++ uses to retrieve the result of an asynchronous task. See the c++ standard documentation for details.
  
  If you don't care about the result, there is no need to receive the return result, and the program will continue to run while the asynchronous task is processed in another thread. If you want to wait for the task to finish or get the result of the task, use either the **future.wait()** or the **future.get()** methods. Of course, this blocks the current thread.  
- ### Higher-order functions: **Concurrent::map**
  ```c_cpp
    std::vector<int> num = { 1, 2, 3, 4, 5, 6 };
    auto mapFunc = [](int& element) {
        element += 10;
    };
    auto future = Fate::Concurrent::map(num, mapFunc);
    future.wait();  
  ```
  
  I'm assuming you're already familiar with the Map/Reduce programming paradigm, which is just concurrent use. The **Concurrent::map** method can accept a container object and an asynchronous task, and each element in the container concurrently performs asynchronous task processing, the results of which are reflected in the container itself.
  
  In other words, this method affects the original container itself to store elements, with the prerequisite that the asynchronous task must accept parameters by reference. The result of the example above is that the storage element of the container **num** becomes **11/12/13/14/15/16**  
- ### Higher-order functions: **Concurrent::mapped**
  ```c_cpp
    std::vector<int> num = { 1, 2, 3, 4, 5, 6 };
    auto mapFunc = [](int element) {
        element += 10;
        return element;
    };
    auto future = Fate::Concurrent::mapped(num, mapFunc);
    auto ret = future.get();
  ```
  
  The only difference between the **Concurrent:: Mapped ** methods and the **Concurrent::map** methods is that the **Concurrent::mapped** does not affect the original container, but returns a new container of the same type, It stores the results of asynchronous task processing.
  
  Of course, asynchronous tasks can still affect the original container if they accept parameters using references. From this point of view, the **Concurrent:: Mapped** method goes one step further than the **Concurrent::map** method by returning the asynchronous task processing results as containers
- ### Higher-order functions: **Concurrent::mappedReduced**
  ```c_cpp
    std::vector<int> num = { 1, 2, 3, 4, 5, 6 };
      auto mapFunc = [](int& element) {
              return element + 10;
      };
      auto reduceFunc = [](int& reduceRet, int mapRet){
              reduceRet += mapRet;
      };
      auto future = Fate::Concurrent::mappedReduced(num, mapFunc, reduceFunc, 19);
      auto ret = future.get();
  ```
  
  This method is the full picture of the **Map/Reduce** paradigm. For a container, the **Map** method concurrently processes every element in the container, and then the results are handed over to the **Reduce** method in an orderly manner for cumulative processing. Wherein, the **Reduce** method accepts two parameters, the first parameter is the cumulative calculation result **reduceRet**, which can be initialized, and the second parameter is the result of the elements in the container after asynchronous task processing **mapRet**.
  
  **Concurrent::mappedReduced** method takes at least three parameters, are asynchronous task containers, Map, Reduce computing tasks, and no amount of parameters is used to initialize the accumulative calculation results **reduceRet**. The above example is to initialize the **reduceRet** with **19**, so before processing the first **mapRet**, the **reduceRet** has a value, that is **19**.

## To solve the pain points

- ### Spontaneous deadlocks in thread pools
  
  At first, I didn't notice spontaneous deadlocks in thread pools, and instead of making an open source thread pool, I simply wrapped a thread pool for my own use. The original thread pool encapsulation principle was simple: create **CPU+1** threads to hibernate, add asynchronous tasks to the task queue, and wake up a thread for processing.
  
  However, I soon found that if there is an asynchronous task **A**, initiates the asynchronous task **B**, initiates the asynchronous task **C**,... , until the **N** asynchronous task. The **N** asynchronous task occupies all threads in the thread pool, and the new tasks initiated by the first **N** asynchronous task or from other places will be blocked in the task queue, waiting for the thread to process, and there are no free threads, all are blocked in the first **N** asynchronous task.  
  
  This is a spontaneous thread pool deadlock case. In fact, the spontaneous thread pool deadlock trigger is not so severe, as long as one requirement is met: *** the current asynchronous task initiates a new asynchronous task, and the new asynchronous task blocks in the task queue. *** In the case of high concurrency, the problem of spontaneous deadlock in the thread pool cannot be ignored.
- ### CPU cannot run at full load
  
  In the case of high concurrency, the CPU is not running at full load. The purpose of using thread pools is to make full use of multi-core computing power for parallel computation. Now, when the CPU is not full at high concurrency, the meaning of thread pools is not so great.
  
  Problem check，the discovery is still related to the spontaneous deadlock of the thread pool, except that the thread pool is not yet deadlocked, but the thread pool has the following situation：**The currently processed asynchronous task initiates a new asynchronous task, and the new asynchronous task blocks in the task queue.** It's like killing a thread!
  
  In other words, if the current system has 8 cores and the thread pool is enabled for **9** threads by default, the CPU can be fully loaded by running **9** threads. However, four of the asynchronous tasks initiated new asynchronous tasks, and the new asynchronous tasks blocked in the task queue. The original **4** asynchronous tasks directly blocked the thread, which was scheduled to the kernel blocking queue waiting to wake up. At this time, there are only **5** threads in the system for asynchronous task processing, and the CPU can only run to **50%**. More importantly, if an asynchronous task initiates a new asynchronous task, there is a high probability that multiple asynchronous tasks will be thrown into the task queue instead of just one. As a result, the original asynchronous task keeps blocking the thread, causing the thread pool to never fill up and the CPU to fall far short of **100%**.
- ### Resolve spontaneous deadlocks in thread pools and CPU failure to run at full capacity
  
  So how do we solve this？We must ensure that the thread pool is full at any given time with **CPU+1** threads running, and the key is that currently asynchronous processing can block threads in the pool if **an asynchronous task initiates a new asynchronous task** . The key to the solution is to assign a thread state, which is **Ready/Blocking/Running**.
  
  State transition rules are as follows: **Ready->Running->Blocking->Ready.** We guarantee that threads in the **Running** state always have **CPUS+1**，As soon as ** an asynchronous task initiates a new asynchronous task**, the thread changes from **Running** to **Blocking** and wakes up a **Ready** thread to **Running**. Ensure that Running threads always have **CPUS+1**. As soon as a **Blocking** thread terminates an asynchronous task, it changes its state to **Ready** for sleep, and waits for awakening to change to **Running**.
  
  There are other optimizations, such as the use of double-ended queues for task queues. This is mainly to prevent the number of **Blocking** threads from increasing when a large number of **Blocking** asynchronous tasks are added, and also to encourage **Blocking** threads to terminate the asynchronous task as soon as possible.
  
  Please refer to the code for the specific implementation scheme. If you find a new problem or have a better implementation scheme, please submit **issue** for discussion.
