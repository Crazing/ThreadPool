#ifndef FATE_FUNCTION_TRAITS_H
#define FATE_FUNCTION_TRAITS_H

#include <tuple>
#include <type_traits>

/* �������ģ���� */
template <typename T>
struct function_traits;

/* ���帨������ */
template <typename R, typename... Args>
struct function_traits_helper
{
	static constexpr int param_count = sizeof...(Args);
	using return_type = R;

	template<std::size_t N>
	using args_type = std::tuple_element_t<N, std::tuple<Args...>>;
};

/* �ػ��������� */
template <typename R, typename... Args>
struct function_traits<R(Args...)> : public function_traits_helper<R, Args...> {};

/* �ػ������������� */
template <typename R, typename... Args>
struct function_traits<R(&)(Args...)> : public function_traits_helper<R, Args...> {};

/* �ػ�����ָ������ */
template <typename R, typename... Args>
struct function_traits<R(*)(Args...)> : public function_traits_helper<R, Args...> {};

/* �ػ��������󣬰���lambda���ʽ��std::function����std::bind���ʽ�����Զ��庯�ӵ� */
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

/* �Ժ����������operator()չ�� */
template <typename T>
struct function_traits : public function_traits<decltype(&std::remove_reference_t<T>::operator())> {};


#endif // FATE_RPC_FUNCTION_TRAITS_H