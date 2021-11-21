# ThreadPool

这是一个纯粹的、高性能的、不会自发性死锁的线程池，拿到项目里就能使用。  

## 使用方法

- ### 基本用法 **ThreadPool::run**
  ```c_cpp
    auto asyncTask = [] {
        std::cout << "This is an async task!" << std::endl;
    };
    auto future = Fate::ThreadPool::run(asyncTask);
    future.wait();
  ```
  
  线程池是以单例形式存在的，内部初始化了**cpu+1**个线程进行工作。当然，这个数量是可以控制的：  
  ```batchfile
    auto pool = Fate::ThreadPool::instance(20);
  ```
  
  这个方法必须在使用任何线程池的方法之前进行调用，因为线程池只会创建一次，返回值就是线程池的唯一实例，参数就是你想创建的线程 数量。  
  
  **run**方法以及后面介绍到的任何添加异步任务的方法，可以接受任何形式的函数定义。**C函数、函数引用、函数指针、函数对象，包括lambda表达式、std::function对象、std::bind表达式对象、自定义函子等。**
  
   所以，我们仅仅需要关注异步任务本身逻辑即可。  
  
  **run**方法以及后面介绍到的任何添加异步任务的方法，返回值都是 **std::future\<T\>** 类型，他是c++用于获取异步任务结果的类型，具体使用方法请查看c++标准文档。
  
  如果对结果不关心，就没必要接收返回结果了，程序会继续往下运行，而异步任务在另外的线程进行处理。如果想要等待任务结束或者获取到任务返回结果，请使用 **future.wait()** 或者 **future.get()** 方法。当然，这会阻塞当前线程。  
- ### 高阶函数 **Concurrent::map**
  ```c_cpp
    std::vector<int> num = { 1, 2, 3, 4, 5, 6 };
    auto mapFunc = [](int& element) {
        element += 10;
    };
    auto future = Fate::Concurrent::map(num, mapFunc);
    future.wait();  
  ```
  
  我假设你已经熟悉Map/Reduce编程范式，这里只是并发使用。 **Concurrent::map** 方法可以接受一个容器对象以及一个异步任务，容器中每个元素都会并发的进行异步任务处理，处理结果会体现到容器本身。
  
  换句话说，该方法会影响原容器本身存储元素，前置要求就是异步任务必须以引用的方式接受参数。上述示例的结果就是容器 **num** 的存储元素变为 **11/12/13/14/15/16** 。  
- ### 高阶函数 **Concurrent::mapped**
  ```c_cpp
    std::vector<int> num = { 1, 2, 3, 4, 5, 6 };
    auto mapFunc = [](int element) {
        element += 10;
        return element;
    };
    auto future = Fate::Concurrent::mapped(num, mapFunc);
    auto ret = future.get();
  ```
  
  **Concurrent::mapped** 方法和 **Concurrent::map** 方法的唯一区别在于 **Concurrent::mapped** 不会影响原有容器，而是返回一个新的容器，该容器和原有容器类型相同，存储的却是异步任务处理结果。
  
  当然，如果异步任务使用引用接受参数，还是可以影响到原有容器的。从这一点看，**Concurrent::mapped** 方法比 **Concurrent::map** 方法多做了一步，就是把异步任务处理结果组成容器返回。
- ### 高阶函数 **Concurrent::mappedReduced**
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
  
  该方法就是 **Map/Reduce** 范式的全貌了，对于某个容器，先由 **map** 方法对容器中每一个元素进行并发处理，然后将处理结果有次序的交给 **reduce** 方法进行累加处理。其中， **reduce** 方法接受两个参数，第一个参数是累加计算结果 **reduceRet** ，可以进行初始化，第二个参数是容器中元素经过异步任务处理后的结果 **mapRet** 。
  
  **Concurrent::mappedReduced** 方法接受至少三个参数，分别是容器、Map异步任务、Reduce计算任务，再多的参数用于初始化累加计算结果 **reduceRet**。上述示例就是用 **19** 初始化 **reduceRet** ，所以，在处理第一个 **mapRet** 之前， **reduceRet** 是有值的，就是 **19** 。

## 解决痛点

- ### 线程池的自发性死锁问题
  
  一开始，我没有注意到线程池的自发性死锁问题，也没打算开源做个线程池，就打算简单封装个线程池自己使用。原始的线程池封装原理比较简单，创建 **cpu+1** 个线程进行休眠，将异步任务添加到任务队列，然后唤醒某个线程进行处理即可。
  
  但是，很快我就发现了问题，如果存在一个异步任务 **A** ，发起了异步任务 **B** ， **B** 又发起了 **C**，...，直到第 **N** 个异步任务。这 **N** 个异步任务占满了线程池里的所有线程，这时候第 **N** 个异步任务发起的新任务或者从其他地方发起的新任务都将堵死在任务队列中，等待线程进行处理，而这时已经没有空闲的线程了，都已经阻塞在前 **N** 个异步任务里。  
  
  这是一种线程池自发性死锁案例，其实，线程池的自发性死锁触发没这么苛刻，只要满足一个要求即可：***当前处理的异步任务发起新的异步任务，而新的异步任务阻塞在任务队列里。***  在并发比较高的情形下，线程池的自发性死锁问题就不能忽视了。
- ### cpu无法满载运行的问题
  
  在并发比较高的情形下，cpu没有满载运行，这是怎么回事？我们使用线程池的目的，不就是为了充分使用多核的计算能力进行并行计算嘛，现在，高并发下cpu没有满载，那线程池的意义就没那么大了。
  
  问题排查之下，发现还是和线程池的自发性死锁有关，只是线程池还没死锁，但是线程池已经出现了以下情形：**当前处理的异步任务发起新的异步任务，而新的异步任务阻塞在任务队列里。** 相当于直接废掉了一个线程！
  
  换句话说，假如当前系统有8核，线程池默认开启 **9** 个线程，原则上跑满 **9** 个线程就可以让cpu满载。但是，其中4个异步任务发起了新的异步任务，而新的异步任务阻塞在任务队列，原来的 **4** 个异步任务直接让线程阻塞了，线程被调度到内核阻塞队列等待唤醒。这一段时间，系统内只有 **5** 个线程在进行异步任务处理，cpu只能跑到 **50%** 左右。更关键的是，异步任务既然发起了新的异步任务，那大概率不会只发起一个，反而是多个异步任务扔到任务队列里。结果就是，原来的异步任务就会一直阻塞线程，导致线程池里的线程根本无法跑满，cpu远远达不到 **100%** 。
- ### 解决线程池的自发性死锁问题以及cpu无法满载运行的问题
  
  怎么解决呢？我们必须保证线程池任一时刻都有 **cpu+1** 个线程在满载运行，而关键在于一旦 **当前处理的异步任务发起了新的异步任务** 就有可能阻塞线程池里的线程。解决的关键就是给线程分状态，分别是 **Ready/Blocking/Running**。
  
  状态转换规则如下： **Ready->Running->Blocking->Ready。** 我们保证处于 **Running** 状态的线程永远有 **cpu+1** 个，而一旦 **当前处理的异步任务发起了新的异步任务** ，该线程就由 **Running** 转变为 **Blocking** ，同时唤醒一个阻塞的 **Ready** 线程转变为 **Running** 状态，保证处于 **Running** 状态的线程永远有 **cpu+1** 个。一旦处于 **Blocking** 状态的线程结束异步任务，就将状态转变为 **Ready** 状态进行休眠，等待唤醒转变为 **Running** 状态。
  
  其中还有其他优化措施，比如任务队列使用双端队列，允许由 **异步任务发起的新的异步任务** 进行插队，主要是为了防止添加大量 **Blocking** 异步任务之后线程数量陡增的问题，也是为了促使处于 **Blocking** 状态的线程尽快结束异步任务。
具体的实现方案请看代码，如果各路大佬发现了新的问题或者有更好的实现方案，请提交 **issue** 进行商讨。
