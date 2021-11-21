#ifndef FATE_FUNCTION_TRAITS_H
#define FATE_FUNCTION_TRAITS_H

#include <tuple>
#include <type_traits>

/* 定义基础模板类 */
template <typename T>
struct function_traits;

/* 定义辅助类型 */
template <typename R, typename... Args>
struct function_traits_helper
{
	static constexpr int param_count = sizeof...(Args);
	using return_type = R;

	template<std::size_t N>
	using args_type = std::tuple_element_t<N, std::tuple<Args...>>;
};

/* 特化函数类型 */
template <typename R, typename... Args>
struct function_traits<R(Args...)> : public function_traits_helper<R, Args...> {};

/* 特化函数引用类型 */
template <typename R, typename... Args>
struct function_traits<R(&)(Args...)> : public function_traits_helper<R, Args...> {};

/* 特化函数指针类型 */
template <typename R, typename... Args>
struct function_traits<R(*)(Args...)> : public function_traits_helper<R, Args...> {};

/* 特化函数对象，包括lambda表达式、std::function对象、std::bind表达式对象、自定义函子等 */
template <typename ClassType, typename R, typename... Args>
struct function_traits<R(ClassType::*)(Args...) const> : public function_traits_helper<R, Args...>
{
	using class_type = ClassType;
};

template <typename ClassType, typename R, typename... Args>
struct function_traits<R(ClassType::*)(Args...)> : public function_traits_helper<R, Args...>
{
	using class_type = ClassType;
};

/* 对函数对象进行operator()展开 */
template <typename T>
struct function_traits : public function_traits<decltype(&std::remove_reference_t<T>::operator())> {};


#endif // FATE_RPC_FUNCTION_TRAITS_H