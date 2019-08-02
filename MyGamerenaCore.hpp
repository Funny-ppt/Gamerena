#ifndef MY_GAMERENA_CORE_HPP
#define MY_GAMERENA_CORE_HPP

#include <vector>
#include <unordered_map>
#include <memory>
#include <utility>
#include <string>
#include <algorithm>
#include <functional>
#include "MultiDelegate.hpp"
#include "MyExceptions.hpp"

namespace GameCore
{

template<typename ValueType>
using Container = std::shared_ptr<ValueType>;
template<typename ValueType>
using List = std::vector<ValueType>;
template<typename FunctionType>
using Delegate = std::function<FunctionType>;
template<typename KeyType, typename ValueType>
using HashMap = std::unordered_map<KeyType, ValueType>;
using std::string;
using std::move;
using std::max;
using std::remove;
using Pptlib::MultiDelegate;
using Pptlib::NullArgumentException;
using Pptlib::InvalidArgumentException;

struct INamable
{
public:
	INamable() = default;
	virtual ~INamable() = default;
	virtual bool HasName(const string& name) { return name == Name; }
	const string& GetName()const { return Name; }
protected:
	void SetName(const string& name)
	{
		Name = name;
	}
private:
	string Name;
};

struct ICloneable
{
	ICloneable() = default;
	virtual ~ICloneable() = default;
	virtual ICloneable* Clone()const = 0;
};

struct IState;
struct IAttribute : public ICloneable, public INamable
{
	IAttribute() = default;
	virtual ~IAttribute() = default;
	virtual IAttribute* Clone()const = 0;
};

struct IState : public ICloneable
{
	IState(const IAttribute* attribute)
	{
		if (attribute == nullptr)
			throw NullArgumentException("attribute can\'t be null.");
		Attribute = Container<IAttribute>(attribute->Clone());
	}
	template<typename AttributeType>
	IState(Container<AttributeType> attribute)
	{
		if (attribute == nullptr)
			throw NullArgumentException("attribute can\'t be null.");
		Attribute = attribute; // should here call clone()?
	}
	virtual ~IState() = default;
	virtual IState* Clone()const = 0;
	virtual void InitFromAttribute() = 0;
	const IAttribute* GetAttribute()const
	{
		return Attribute.get();
	}
	IAttribute* GetAttribute()
	{
		return Attribute.get();
	}
private:
	Container<IAttribute> Attribute;
};

class GameObject : public ICloneable, public INamable
{
public:
	GameObject(const IState* state) :
		GameObject(Container<IState>(state ? state->Clone() : nullptr)) {}
	template<typename StateType>
	GameObject(Container<StateType> state) // 也许应该只接受 unique_ptr ? (包括该系列模板构造函数)
	{
		if (state == nullptr)
			throw NullArgumentException("state can\'t be null.");
		State = state;
		SetName(state->GetAttribute()->GetName());
	}
	GameObject(const GameObject& other) :
		State(other.State->Clone())
	{
		SetName(other.GetName());
	}
	GameObject(GameObject&& other)noexcept :
		State(other.State)
	{
		SetName(other.GetName());
	}
	GameObject& operator=(const GameObject& other)
	{
		State = Container<IState>(other.State->Clone());
		SetName(other.GetName());
		return *this;
	}
	GameObject& operator=(GameObject&& other)noexcept
	{
		State = other.State;
		SetName(other.GetName());
		return *this;
	}
	virtual ~GameObject() = default;

	virtual GameObject* Clone()const
	{
		return new GameObject(*this);
	}
	bool IsTypeEquals(const GameObject& other)const
	{
		return State->GetAttribute() == other.State->GetAttribute();
	}
	bool HasType(IAttribute* attribute)const
	{
		return State->GetAttribute() == attribute;
	}
	const IState* GetState()const { return State.get(); }
	IState* GetState() { return State.get(); }
	const IAttribute* GetAttribute()const { return State->GetAttribute(); }
protected:
	IAttribute* __GetAttribute() { return State->GetAttribute(); }
	void UncheckedTypeSetState(IState* state)
	{
		if (state == nullptr)
			throw NullArgumentException("state can\'t be null.");
		State = Container<IState>(state->Clone());
	}
	template<typename StateType>
	void UncheckedTypeSetState(Container<StateType> state)
	{
		SetState(state.get());
	}
	void SetState(IState* state)
	{
		if (state == nullptr)
			throw NullArgumentException("state can\'t be null.");
		if (!HasType(state->GetAttribute()))
			throw InvalidArgumentException("state have different type.");
		State = Container<IState>(state->Clone());
	}
	template<typename StateType>
	void SetState(Container<StateType> state)
	{
		SetState(state.get());
	}
private:
	Container<IState> State;
};

struct EntityState;
class Entity;

struct EntityAttribute : public IAttribute
{
	EntityAttribute() = default;
	virtual ~EntityAttribute() = default;
	virtual EntityAttribute* Clone()const
	{
		return new EntityAttribute(*this);
	}
	using ActionHandler = Delegate<void(Entity*)>;
	virtual void AddAction(const ActionHandler& handler)
	{
		Actions += handler;
	}
	virtual void AddNamedAction(const ActionHandler& handler, const string& name)
	{
		Actions += handler;
		NamedActions[name] = handler;
	}
	bool TryInvokeAction(const string& actionName, Entity* entity)const
	{
		if (NamedActions.count(actionName))
		{
			NamedActions.find(actionName)->second(entity);
			return true;
		}
		return false;
	}
	void InvokeAllActions(Entity* entity)const
	{
		Actions.Invoke(entity);
	}
private:
	MultiDelegate<Entity*> Actions;
	HashMap<string, ActionHandler> NamedActions;
};

struct EntityState : public IState
{
	EntityState(const EntityAttribute* attribute) : IState(attribute) {}
	template<typename AttributeType>
	EntityState(Container<AttributeType> attribute) : IState(attribute) {}
	virtual EntityState* Clone()const { return new EntityState(*this); }
	virtual void InitFromAttribute() {}
	virtual ~EntityState() = default;
};

class Entity : public GameObject
{
	static HashMap<string, Container<EntityState>> StateMap;
public:
	static bool NamedState(
		EntityState* state,
		const string& name,
		bool replaceOld = false)
	{
		if (state == nullptr)
			throw NullArgumentException("state can\'t be null.");
		if (StateMap.count(name) && !replaceOld)
			throw InvalidArgumentException("name has been used.");
		StateMap[name] = Container<EntityState>(state->Clone());
	}
	Entity(const string& entityName) :
		GameObject(StateMap.count(entityName)
			? StateMap[entityName].get()
			: throw InvalidArgumentException("Can\'t find name.")) {}
	Entity(const EntityState* state) : GameObject(state) {}
	template<typename StateType>
	Entity(Container<StateType> state) : GameObject(state) {}
	virtual Entity* Clone()const { return new Entity(*this); }
	virtual ~Entity() = default;
	const EntityAttribute* GetAttribute()const
	{
		return (const EntityAttribute*)GameObject::GetAttribute();
	}
	EntityState* GetState()
	{
		return (EntityState*)GameObject::GetState();
	}
	const EntityState* GetState()const
	{
		return (const EntityState*)GameObject::GetState();
	}
	void DoActions()
	{
		GetAttribute()->InvokeAllActions(this);
	}
	bool TryDoAction(const string& actionName)
	{
		return GetAttribute()->TryInvokeAction(actionName, this);
	}
protected:
	EntityAttribute* __GetAttribute()
	{
		return (EntityAttribute*)GameObject::__GetAttribute();
	}
};
HashMap<string, Container<EntityState>> Entity::StateMap;

} // namespace GameCore

#endif // !MY_GAMERENA_CORE_HPP