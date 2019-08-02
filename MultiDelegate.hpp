#ifndef MULTI_DELEGATE_HPP
#define MULTI_DELEGATE_HPP

#include <functional>
#include <list>
#include "MyExceptions.hpp"

namespace Pptlib
{

template<typename... Args>
class MultiDelegate
{
public:
	using DelegateType = std::function<void(Args...)>;
	MultiDelegate() = default;
	MultiDelegate(const std::list<DelegateType>& delegates) :
		Delegates(delegates) {}
	void Invoke(Args ... args)const
	{
		for (auto& delegate : Delegates)
			delegate(args...);
	}
	void CheckedInvoke(Args... args)const
	{
		for (auto& delegate : Delegates)
			if (delegate) delegate(args...);
	}
	void operator()(Args... args)const
	{
		for (auto& delegate : Delegates)
			delegate(args...);
	}
	bool IsEmpty()const
	{
		return Delegates.size() == 0;
	}
	operator bool()const
	{
		for (auto& delegate : Delegates)
			if (delegate) return true;
		return false;
	}
	bool operator!()const
	{
		return !operator bool();
	}
	void AddDelegate(const DelegateType& delegate)
	{
		if (!delegate)
			throw InvalidArgumentException("delegate must be callable.");
		Delegates.push_back(delegate);
	}
	void UncheckedAddDelegate(const DelegateType& delegate)
	{
		Delegates.push_back(delegate);
	}
	void RemoveDelegate(const DelegateType& delegate)
	{
		auto iter = Delegates.begin();
		while (iter != Delegates.end())
			if (*iter == delegate)
				iter = Delegates.erase(iter);
			else
				++iter;
	}
	void operator+=(const DelegateType& delegate)
	{
		AddDelegate(delegate);
	}
	void operator-=(const DelegateType& delegate)
	{
		RemoveDelegate(delegate);
	}
private:
	std::list<DelegateType> Delegates;
};

}

#endif // !MULTI_DELEGATE_HPP