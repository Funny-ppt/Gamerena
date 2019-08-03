#include "MyGamerenaCore.hpp"
#include "StringFormat.hpp"
#include "CommandAnalyzer.hpp"
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <unordered_set>
using namespace GameCore;
using namespace Pptlib;
using namespace std;

inline double Random()
{
	constexpr int RandMax = RAND_MAX + 1;
	return ((double)rand()) / RandMax;
}
inline int Random(int n)
{
	return (int)(Random() * n);
}
inline int Random(int l, int r)
{
	if (l >= r) throw InvalidArgumentException("Condition: l < r.");
	return Random(r - l) + l;
}

namespace {
using Stage = int;
using TargetType = int;
struct StageEnum
{
	constexpr StageEnum() = default;
	const Stage InAction = 1 << 0;
	const Stage BeAttacked = 1 << 1;
	const Stage Waiting = 1 << 2;
};
struct TargetTypeEnum
{
	constexpr TargetTypeEnum() = default;
	const TargetType Teammate = 1 << 0;
	const TargetType Enemy = 1 << 1;
	const TargetType Random = 1 << 2;
};
constexpr StageEnum Stages;
constexpr TargetTypeEnum TargetsType;
}

struct LogEventArgs
{
	string LogText;
	vector<ICustomFormatter*> FormattableObjects;
};

class LogRecorder
{
public:
	MultiDelegate<LogRecorder*, LogEventArgs*> BeforeLog;
	MultiDelegate<LogRecorder*, LogEventArgs*> AfterLog;
	void SetSpaceLevel(int level)
	{
		SpaceLevel = level;
	}
private:
	basic_ostream<char>* OutputStream;
	int SpaceLevel;
};

class GamerenaEntity;
using Targets = List<GamerenaEntity*>; // 暂定, 将来可能会改成类
using SkillDelegateType =
	Delegate<void(GamerenaEntity*, const Targets&)>;
struct Skill
{
	operator bool()const
	{
		return SkillDetails;
	}
	bool operator!()const
	{
		return !SkillDetails;
	}
	void operator()(GamerenaEntity* performer,
		const Targets& targets)const
	{
		SkillDetails.Invoke(performer, targets);
	}
	string Name;
	MultiDelegate<GamerenaEntity*, const Targets&> SkillDetails;
	::TargetType TargetType;
	int Priority;
};

struct GamerenaAttribute;
class SkillSelector
{
public:
	void AddSkill(const Skill& skill)
	{
		if (!skill)
			throw InvalidArgumentException("skill is invalid.");
		Skills.push_back(skill);
		TotalPriority += skill.Priority;
	}
	// TODO:这是最简单的技能选择器; 实际将会根据Int实现多种选择器
	const Skill& RandomSkill()const
	{
		int k = Random(TotalPriority);
		auto iter = Skills.begin();
		while (k >= iter->Priority)
		{
			k -= iter->Priority;
			++iter;
		}
		return *iter;
	}
	void GenerateSkill(GamerenaAttribute* e);
private:
	int TotalPriority = 0;
	List<Skill> Skills;
};

struct GamerenaAttribute : public EntityAttribute
{
	virtual GamerenaAttribute* Clone()const
	{
		return new GamerenaAttribute(*this);
	}
	GamerenaAttribute(const string& groupName, const string& name)
	{
		SetName(name);
		OriginGroupIndex = hash<string>()(groupName);
		const size_t RandomF = 419;
		const size_t RandomS = 1284541;
		srand(hash<string>()(name) * RandomF + RandomS);
		BaseHP = Random(200, 350);
		BaseAttack = Random(30, 100);
		BaseDefense = Random(30, 100);
		BaseMagic = Random(30, 100);
		BaseMagicDefense = Random(30, 100);
		BaseSpeed = Random(30, 100);
		BaseAccuracy = Random(30, 100);
		BaseIntelligence = Random(30, 100);
		tSkillSelector.GenerateSkill(this);
	}
	using DelegateType = Delegate<void(GamerenaEntity*)>;
	using WrappedDelegateType = typename EntityAttribute::ActionHandler;
	virtual void AddAction(const DelegateType& handler)
	{
		EntityAttribute::
			AddAction(MakeWrappedDelegate(handler));
	}
	virtual void AddNamedAction(
		const DelegateType& handler,
		const string& name)
	{
		EntityAttribute::
			AddNamedAction(MakeWrappedDelegate(handler), name);
	}
	SkillSelector tSkillSelector;
	size_t OriginGroupIndex;
	int BaseHP;
	int BaseAttack;
	int BaseDefense;
	int BaseMagic;
	int BaseMagicDefense;
	int BaseSpeed;
	int BaseAccuracy;
	int BaseIntelligence;
protected:
	static WrappedDelegateType MakeWrappedDelegate(const DelegateType& del)
	{
		return [=](Entity* e) { del((GamerenaEntity*)e); };
	}
};

struct GamerenaState : public EntityState
{
	virtual GamerenaState* Clone()const
	{
		return new GamerenaState(*this);
	}
	GamerenaState(const GamerenaAttribute* attribute) :
		EntityState(attribute)
	{
		InitFromAttribute();
	}
	template<typename AttributeType>
	GamerenaState(Container<AttributeType> attribute) :
		EntityState(attribute)
	{
		InitFromAttribute();
	}
	const GamerenaAttribute* GetAttribute()const
	{
		return (GamerenaAttribute*)EntityState::GetAttribute();
	}
	virtual void InitFromAttribute()
	{
		auto& attr = *GetAttribute();
		GroupIndex = attr.OriginGroupIndex;
		HP = attr.BaseHP;
		Attack = attr.BaseAttack;
		Defense = attr.BaseDefense;
		Magic = attr.BaseMagic;
		MagicDefense = attr.BaseMagicDefense;
		Speed = attr.BaseSpeed;
		Accuracy = attr.BaseAccuracy;
		Intelligence = attr.BaseIntelligence;
	}
	void GetDamage(int dmg)
	{
		HP = max(HP - dmg, 0);
		if (HP == 0)
		{
			Active = false;
			OnDeath(this);
		}
	}
	Stage Stage = Stages.Waiting;
	size_t GroupIndex;
	bool Active = true;
	int NextActionTime = 0;
	int Score = 0;
	int HP;
	int Attack;
	int Defense;
	int Magic;
	int MagicDefense;
	int Speed;
	int Accuracy;
	int Intelligence;
	MultiDelegate<GamerenaState*> OnDeath;
};

class GamerenaEntity : public Entity, public ICustomFormatter
{
public:
	GamerenaEntity(
		const string& groupName,
		const string& name,
		const GamerenaAttribute::DelegateType& mainAction) :
		Entity(Container<GamerenaState>(new GamerenaState(
			Container<GamerenaAttribute>(
				new GamerenaAttribute(groupName, name)
			))))
	//     这一段比较复杂; 使用Container即shared_ptr包裹的对象的内存
	// 被认为是被托管的, 所以内核接受时将不会执行复制操作（对Clone()函
	// 数的调用）, 这里的话将节省复制的时间和空间
	{
		__GetAttribute()->AddAction(mainAction);
	}
	const GamerenaAttribute* GetAttribute()const
	{
		return (GamerenaAttribute*)Entity::GetAttribute();
	}
	const GamerenaState* GetState()const
	{
		return (GamerenaState*)Entity::GetState();
	}
	GamerenaState* GetState()
	{
		return (GamerenaState*)Entity::GetState();
	}
	virtual string Format(const string& formats)const
	{
		auto& state = *GetState();
		auto& attr = *GetAttribute();
		ostringstream stream;
		auto& str = formats;
		//for (auto& str : Split(formats, ' '))
		//{
			if (str == "hps")
				stream << state.HP << " / " << attr.BaseHP;
			else if (str == "hp")
				stream << attr.BaseHP;
			else if (str == "bhp")
				stream << state.HP;
			else if (str == "name")
				stream << GetName();
			else if (str == "atk")
				stream << attr.BaseAttack;
			else if (str == "def")
				stream << attr.BaseDefense;
			else if (str == "mag")
				stream << attr.BaseMagic;
			else if (str == "magdef")
				stream << attr.BaseMagicDefense;
			else if (str == "spd")
				stream << attr.BaseSpeed;
			else if (str == "acc")
				stream << attr.BaseAccuracy;
			else if (str == "int")
				stream << attr.BaseIntelligence;
			else if (str == "score")
				stream << state.Score;
			else if (str == "hps+")
			{
				stream << state.HP << " / " << attr.BaseHP << "  <";
				int b = (state.HP + 10) / 20;
				for (int i = 0; i < b; ++i) stream.put(2);
				for (int i = b; i < (attr.BaseHP + 10) / 20; ++i) stream.put(1);
				stream.put('>');
			}
			else
				throw InvalidArgumentException(
					"GamerenaEntity.Format(): Invalid format string."
				);
		//	stream.put(' ');
		//}
		return stream.str();
	}
protected:
	GamerenaAttribute* __GetAttribute()
	{
		return (GamerenaAttribute*)Entity::__GetAttribute();
	}
};

class Dispatcher
{
	static bool Compare(Container<GamerenaEntity> lhs,
		Container<GamerenaEntity> rhs)
	{
		return
			lhs->GetState()->NextActionTime
				>
			rhs->GetState()->NextActionTime;
	}
	static void SetNextActionTime(Container<GamerenaEntity> e)
	{
		auto& attr = * e->GetAttribute();
		const int BaseWaitTime = 160;
		int waitTime = (int)
			(BaseWaitTime
			- attr.BaseSpeed * 0.3
			- (attr.BaseSpeed >> 1) * Random());
		// BaseWaitTime(160) - [0.3, 0.8) * Speed[30,100) => WaitTime (80, 151]
		e->GetState()->NextActionTime += waitTime;
	}
public:
	void SetListener(const function<void(Dispatcher*, int)>& listener)
	{
		Listener = listener;
	}
	void AddEntity(Container<GamerenaEntity> entity)
	{
		SetNextActionTime(entity);
		Entities.push_back(entity);
		push_heap(Entities.begin(), Entities.end(), Compare);
	}
	void DispatchNext()
	{
		GamerenaState* state;
		do
		{
			state = Entities.front()->GetState();
			if (!state->Active)
			{
				pop_heap(Entities.begin(), Entities.end(), Compare);
				Entities.pop_back();
			}
		} while (!state->Active && Entities.size() > 1);
		if (Entities.size() <= 1)
		{
			Listener(this, -1);
			return;
		}
		Time = state->NextActionTime;
		pop_heap(Entities.begin(), Entities.end(), Compare);
		SetNextActionTime(Entities.back());
		Entities.back()->DoActions();
		_LastEntity = Entities.back().get();
		push_heap(Entities.begin(), Entities.end(), Compare);
		if (Listener) Listener(this, Time);
	}
	GamerenaEntity* LastEntity()
	{
		return _LastEntity;
	}
	int GetCurrentTime()const
	{
		return Time;
	}
private:
	int	Time = 0;
	function<void(Dispatcher*, int)> Listener;
	List<Container<GamerenaEntity>> Entities;
	GamerenaEntity* _LastEntity = nullptr;
};

class TargetSelector
{
public:
	// TODO: GetRandomTarget()是最简单的实现; 具体选择算法实现将会取决于Int
	List<GamerenaEntity*> GetRandomTarget(GamerenaEntity* entity)
	{
		if (UpdateFlag) Update();
		auto& state = *entity->GetState();
		if (state.GroupIndex == 0xffffffff)
		{
			int select = ActiveGroups[Random(ActiveGroups.size())];
			return { Entities[select][Random(Entities[select].size())].get() };
		}
		size_t nth =
			find(ActiveGroups.begin(), ActiveGroups.end(), state.GroupIndex)
		  - ActiveGroups.begin();
		size_t select = Random(ActiveGroups.size() - 1);
		if (select >= nth) ++select;
		select = ActiveGroups[select];
		return { Entities[select][Random(Entities[select].size())].get() };
	}
	List<GamerenaEntity*> GetRandomTeammate(GamerenaEntity* entity)
	{
		if (UpdateFlag) Update();
		auto& state = *entity->GetState();
		if (state.GroupIndex == 0xffffffff)
		{
			size_t select = ActiveGroups[Random(ActiveGroups.size())];
			return { Entities[select][Random(Entities[select].size())].get() };
		}
		int teammateCount = Entities[state.GroupIndex].size();
		if (teammateCount > 0)
			return { Entities[state.GroupIndex][Random(teammateCount)].get() };
		return { entity };

	}
	void AddEntity(Container<GamerenaEntity> entity)
	{
		auto& attr = *entity->GetAttribute();
		if (Entities.count(attr.OriginGroupIndex) == 0)
		{
			auto iter =lower_bound(
				ActiveGroups.begin(), ActiveGroups.end(),
				attr.OriginGroupIndex);
			ActiveGroups.insert(iter, attr.OriginGroupIndex);
		}
		Entities[attr.OriginGroupIndex].push_back(entity);
	}
	void SetUpdateFlag(bool flag = true)
	{
		UpdateFlag = flag;
	}
	int GroupsKeep()
	{
		if (UpdateFlag) Update();
		return ActiveGroups.size();
	}
protected:
	void Update()
	{
		UpdateFlag = false;
		for (auto& pair : Entities)
		{
			auto& group = pair.second;
			if (group.size() == 0) continue;
			for (size_t i = 0; i < group.size();)
			{
				if (!group[i]->GetState()->Active)
					group.erase(group.begin() + i);
				else
					++i;
			}
			if (pair.second.size() == 0)
			{
				auto iter = find(ActiveGroups.begin(),
					ActiveGroups.end(), pair.first);
				ActiveGroups.erase(iter);
			}
		}
	}
private:
	bool UpdateFlag = true;
	List<size_t> ActiveGroups;
	HashMap<size_t, List<Container<GamerenaEntity>>> Entities;
};

class Game
{
public:
	Game()
	{
		auto listener = [&](Dispatcher* d, int time)
		{
			if (time == -1)
			{
				DispatcherErrorHandler(d);
				return;
			}
		};
		tDispatcher.SetListener(listener);
	}
	void AddName(const string& groupName, const string& name)
	{
		size_t hashCode = hash<string>()(groupName);
		auto entity = Container<GamerenaEntity>(
			new GamerenaEntity(
				groupName,
				name,
				[&](GamerenaEntity* e)
				{
					auto& attr = *e->GetAttribute();
					auto& skill = attr.tSkillSelector.RandomSkill();
					List<GamerenaEntity*> target;
					switch (skill.TargetType)
					{
					case TargetsType.Enemy:
						target = tTargetSelector.GetRandomTarget(e);
						break;
					case TargetsType.Teammate:
						target = tTargetSelector.GetRandomTeammate(e);
						break;
					}
					skill(e, target);
				}));
		auto& state = *entity->GetState();
		state.OnDeath += [&](GamerenaState*) {
			tTargetSelector.SetUpdateFlag();
		};
		Groups[hashCode].push_back(entity);
		tDispatcher.AddEntity(entity);
		tTargetSelector.AddEntity(entity);
	}
	void Start()
	{
		while (!IsDone())
			tDispatcher.DispatchNext();
	}
	using Group = List<Container<GamerenaEntity>>;
	const HashMap<size_t, Group>& GetGroups()const
	{
		return Groups;
	}
	bool IsDone()
	{
		if (DoneFlag)
			return true;
		if (tTargetSelector.GroupsKeep() < 2)
			return DoneFlag = true;
		return false;
	}
protected:
	void DispatcherErrorHandler(Dispatcher* d)
	{
		//TODO
		DoneFlag = true;
	}
private:
	bool DoneFlag = false;
	Dispatcher tDispatcher;
	TargetSelector tTargetSelector;
	HashMap<size_t, Group> Groups;
};

void CausePhysicDamage(
	GamerenaEntity* p,
	List<GamerenaEntity*> targets,
	double dmgFactor = 1.0)
{
	auto t = targets.front();
	auto& pState = *p->GetState();
	auto& tState = *t->GetState();
	auto& pAttr = *p->GetAttribute();
	auto& tAttr = *t->GetAttribute();
	// 闪避判定
	const int BaseDodgeChance = 16;
	int dodgeChance = BaseDodgeChance
		+ (tAttr.BaseAccuracy - pAttr.BaseAccuracy) / 4
		+ (tAttr.BaseDefense - pAttr.BaseAttack) / 8;
	if (Random(100) < dodgeChance)
	{
		cout << " 但 " << tAttr.GetName() << " 闪避了攻击.\n";
		return;
	}
	const int BaseDamage = 15;
	int damage = max(1,
		(int)(BaseDamage
			+ pAttr.BaseAttack * 0.3 + pAttr.BaseAttack * 0.9 * Random()
			- tAttr.BaseDefense * 0.2 + tAttr.BaseDefense * 1.3 * Random())) * dmgFactor;
	cout << " 对 " << tAttr.GetName() << " 造成了 " << damage << "点伤害.\n";
	pState.Score += damage;
	tState.GetDamage(damage);
	cout << FormatStringAnalyzer::
		Analysis("    {0:name}  HP {0:hps+}\n", { t });
	if (tState.Active == false)
	{
		pState.Score += 30;
		cout << tAttr.GetName() << " 死亡了, 凶手是 " << pAttr.GetName() << '\n';
	}
}

void CauseMagicDamage(
	GamerenaEntity* p,
	List<GamerenaEntity*> targets,
	double dmgFactor = 1.0)
{
	auto t = targets.front();
	auto& pState = *p->GetState();
	auto& tState = *t->GetState();
	auto& pAttr = *p->GetAttribute();
	auto& tAttr = *t->GetAttribute();
	// 闪避判定
	const int BaseDodgeChance = 25;
	int dodgeChance = BaseDodgeChance
		- (pAttr.BaseIntelligence >> 3)
		+ (tAttr.BaseAccuracy - pAttr.BaseAccuracy) / 8
		+ (tAttr.BaseMagicDefense - pAttr.BaseMagic) / 8;
	if (Random(100) < dodgeChance)
	{
		cout << " 但 " << tAttr.GetName() << " 闪避了攻击.\n";
		return;
	}
	const int BaseDamage = 25;
	int damage = max(1,
		(int)(BaseDamage
			+ pAttr.BaseMagic * 0.6 + pAttr.BaseMagic * 0.6 * Random()
			- tAttr.BaseMagicDefense * 0.75 + tAttr.BaseMagicDefense * 0.75 * Random()
			+ pAttr.BaseIntelligence * 0.2)) * dmgFactor;
	cout << " 对 " << tAttr.GetName() << " 造成了 " << damage << "点魔法伤害.\n";
	pState.Score += damage;
	tState.GetDamage(damage);
	cout << FormatStringAnalyzer::
		Analysis("    {0:name}  HP {0:hps+}\n", { t });
	if (tState.Active == false)
	{
		pState.Score += 30;
		cout << tAttr.GetName() << " 死亡了, 凶手是 " << pAttr.GetName() << '\n';
	}
}

void MakeCure(
	GamerenaEntity* p,
	List<GamerenaEntity*> targets,
	double hFactor = 1.0)
{
	auto t = targets.front();
	auto& pState = *p->GetState();
	auto& tState = *t->GetState();
	auto& pAttr = *p->GetAttribute();
	auto& tAttr = *t->GetAttribute();
	const int BaseHeal = 10;
	int heal = max(1,
		(int)(BaseHeal
			+ pAttr.BaseMagic * 0.25 + pAttr.BaseMagic * 0.35 * Random()
			+ pAttr.BaseIntelligence * 0.4)) * hFactor;
	heal = min(tAttr.BaseHP - tState.HP, heal);
	pState.Score += heal;
	tState.HP += heal;
	cout << " " << tAttr.GetName() << " 恢复了 "<< heal << " 点生命值.\n";
	cout << FormatStringAnalyzer::
		Analysis("    {0:name}  HP {0:hps+}\n", { t });
}

void BaseAttack(GamerenaEntity* p, const Targets& targets)
{
	cout << FormatStringAnalyzer::
		Analysis("{0:name} HP  {0:hps+}\n", { p });
	cout << "  发起了攻击,";
	CausePhysicDamage(p, targets);
}

void BaseMagic(GamerenaEntity* p, const Targets& targets)
{
	cout << FormatStringAnalyzer::
		Analysis("{0:name} HP  {0:hps+}\n", { p });
	cout << "  使用法术攻击,";
	CauseMagicDamage(p, targets);
}

void FireBall(GamerenaEntity* p, const Targets& targets)
{
	cout << FormatStringAnalyzer::
		Analysis("{0:name} HP  {0:hps+}\n", { p });
	cout << "  发射出火球,";
	CauseMagicDamage(p, targets, 1.5);
}

void Critical(GamerenaEntity* p, const Targets& targets)
{
	cout << FormatStringAnalyzer::
		Analysis("{0:name} HP  {0:hps+}\n", { p });
	cout << "  瞄准了目标的弱点攻击,";
	CausePhysicDamage(p, targets, 2);
}

void Cure(GamerenaEntity* p, const Targets& targets)
{
	cout << FormatStringAnalyzer::
		Analysis("{0:name} HP  {0:hps+}\n", { p });
	cout << "  使用了治愈魔法,";
	MakeCure(p, targets, 1.2);
}

void SkillSelector::GenerateSkill(GamerenaAttribute* pAttr)
{
	auto& attr = *pAttr;
	int BaseAttackPriority =
		250 + (attr.BaseAttack - attr.BaseMagic) * (0.5 + Random()) * 4;
	int BaseMagicPriority =
		250 + (attr.BaseMagic - attr.BaseAttack) * (0.5 + Random()) * 4;
	int FireBallPriority =
		50 + (attr.BaseIntelligence >> 1) + (attr.BaseMagic);
	int CriticalPriority =
		30 + (attr.BaseIntelligence >> 2) + (attr.BaseAttack >> 1)
		+ (attr.BaseAccuracy >> 1);
	int CurePriority =
		60 + (attr.BaseIntelligence >> 1) + (attr.BaseMagic >> 2);
	AddSkill(
		{ "BaseAttack", std::list<SkillDelegateType>{ BaseAttack },
		TargetsType.Enemy, BaseAttackPriority });
	AddSkill(
		{ "BaseMagic", std::list<SkillDelegateType>{ BaseMagic },
		TargetsType.Enemy, BaseMagicPriority });
	if (FireBallPriority > 140)
		AddSkill(
			{ "FireBall", std::list<SkillDelegateType>{ FireBall },
			TargetsType.Enemy, FireBallPriority });
	if (CriticalPriority > 125)
		AddSkill(
			{ "Critical", std::list<SkillDelegateType>{ Critical },
			TargetsType.Enemy, CriticalPriority });
	if (CurePriority > 100)
		AddSkill(
			{ "Cure", std::list<SkillDelegateType>{ Cure },
			TargetsType.Teammate, CurePriority });
}

int main()
{
	ios::sync_with_stdio(false);
	CommandAnalyzer Analyzer;
	string fullName;
	size_t seed = time(0);
	Game game;
	unordered_set<string> nameUsed;
	Analyzer.AddCommand("SetSeed",
		[&](List<string>& args)
		{
			seed = hash<string>()(args.at(0));
			cout << "Set seed to \"" << seed << "\".\n";
		});
	while (getline(cin, fullName))
	{
		if (fullName[0] == '>')
		{
			// TODO: CommandMode
			try
			{
				Analyzer.Analysis(fullName);
			}
			catch (Exception& ex)
			{
				cerr << ex.what() << '\n';
			}
			continue;
		}
		size_t nameLength = fullName.find_last_of('@');
		string name(fullName, 0, nameLength);
		if (name == "")
		{
			cout << "Name shouldn\'t be empty.\n";
			continue;
		}
		if (nameUsed.count(name) == 0)
		{
			nameUsed.insert(name);
			string groupName;
			if (nameLength != string::npos)
				groupName = string(fullName, nameLength + 1, string::npos);
			else
				groupName = name;
			if (groupName == "")
			{
				cout << "GroupName shouldn\'t be empty.\n";
				continue;
			}
			cout << "Name: " << name << ", GroupName: " << groupName << ".\n";
			game.AddName(groupName, name);
		}
		else
		{
			cout << "Name \"" << name << "\" has been used.\n"
				 << "Please use another name instead.\n";
		}
	}
	const size_t srandF = 73;
	const size_t srandS = 749431;
	srand(seed * srandF + srandS);
	for (auto& pair : game.GetGroups())
	{
		cout << "GroupName: " << pair.first << '\n';
		for (auto& member : pair.second)
			cout << FormatStringAnalyzer::
				Analysis(
					"    {0:name}    HP  {0:hp}  Spd  {0:spd}\n"
					"    Atk  {0:atk}  Def     {0:def}  Acc  {0:acc}\n"
					"    Mag  {0:mag}  MagDef  {0:magdef}  Int  {0:int}\n",
					{ member.get() });
	}
	cin.ignore(1024, '\n');
	cout << "PressAnyKeyToStart...\n";
	cin.get();
	game.Start();
	cin.ignore(1024, '\n');
	for (auto& pair : game.GetGroups())
	{
		cout << "GroupName: " << pair.first << '\n';
		for (auto& member : pair.second)
			cout << FormatStringAnalyzer::
				Analysis(
					"    {0:name}    HP {0:hps+}\n"
					"    Score: {0:score}\n",
					{ member.get() });
	}
	cout << "Done...\n";
	cin.get();
}
