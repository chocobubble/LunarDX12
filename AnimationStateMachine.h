#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <DirectXMath.h>

namespace Lunar
{

class SkeletalAnimationSystem;

class AnimationStateMachine
{
public:
    // 애니메이션 파라미터 타입
    enum class ParameterType
    {
        Float,
        Int,
        Bool,
        Trigger
    };
    
    struct Parameter
    {
        std::string name;
        ParameterType type;
        
        union Value
        {
            float floatValue;
            int intValue;
            bool boolValue;
            
            Value() : floatValue(0.0f) {}
        } value;
        
        // 트리거는 한 프레임 후 자동으로 false가 됨
        bool isTriggered = false;
    };
    
    // 애니메이션 상태
    struct State
    {
        std::string name;
        std::string animationName;  // 재생할 애니메이션 클립 이름
        float speed = 1.0f;
        bool isLooping = true;
        
        // 상태 진입/종료 시 호출되는 콜백
        std::function<void()> onEnter;
        std::function<void()> onExit;
        std::function<void(float)> onUpdate; // deltaTime 전달
        
        // 블렌딩 설정
        float blendInTime = 0.2f;   // 이 상태로 전환할 때 블렌딩 시간
        float blendOutTime = 0.2f;  // 이 상태에서 나갈 때 블렌딩 시간
        
        // 상태별 파라미터 오버라이드
        std::unordered_map<std::string, Parameter::Value> parameterOverrides;
    };
    
    // 전환 조건
    struct Condition
    {
        enum class CompareType
        {
            Greater,
            Less,
            Equal,
            NotEqual,
            GreaterEqual,
            LessEqual
        };
        
        std::string parameterName;
        CompareType compareType;
        Parameter::Value compareValue;
        
        bool Evaluate(const Parameter& parameter) const;
    };
    
    // 상태 전환
    struct Transition
    {
        std::string fromState;
        std::string toState;
        std::vector<Condition> conditions;
        float blendTime = 0.2f;
        bool hasExitTime = false;       // 애니메이션이 끝날 때까지 기다릴지
        float exitTime = 0.9f;          // 애니메이션의 몇 % 지점에서 전환할지 (0.0 ~ 1.0)
        int priority = 0;               // 우선순위 (높을수록 먼저 검사)
        
        // 전환 시 호출되는 콜백
        std::function<void()> onTransition;
        
        bool CanTransition(const std::unordered_map<std::string, Parameter>& parameters, 
                          float currentAnimationTime, float animationDuration) const;
    };
    
    // 서브 상태 머신 (계층적 상태 머신)
    struct SubStateMachine
    {
        std::string name;
        std::unique_ptr<AnimationStateMachine> stateMachine;
        std::string entryState;
        std::string exitState;
    };

    AnimationStateMachine();
    ~AnimationStateMachine();

    void Initialize(SkeletalAnimationSystem* animationSystem);
    void Shutdown();
    
    // 상태 관리
    void AddState(const State& state);
    void RemoveState(const std::string& stateName);
    bool HasState(const std::string& stateName) const;
    State* GetState(const std::string& stateName);
    
    // 전환 관리
    void AddTransition(const Transition& transition);
    void RemoveTransition(const std::string& fromState, const std::string& toState);
    void RemoveAllTransitionsFrom(const std::string& stateName);
    void RemoveAllTransitionsTo(const std::string& stateName);
    
    // 파라미터 관리
    void AddParameter(const std::string& name, ParameterType type, Parameter::Value defaultValue = {});
    void RemoveParameter(const std::string& name);
    bool HasParameter(const std::string& name) const;
    
    // 파라미터 값 설정
    void SetFloat(const std::string& name, float value);
    void SetInt(const std::string& name, int value);
    void SetBool(const std::string& name, bool value);
    void SetTrigger(const std::string& name);
    
    // 파라미터 값 조회
    float GetFloat(const std::string& name) const;
    int GetInt(const std::string& name) const;
    bool GetBool(const std::string& name) const;
    
    // 상태 머신 제어
    void Start(const std::string& initialState);
    void Stop();
    void Update(float deltaTime);
    
    // 현재 상태 정보
    std::string GetCurrentState() const { return m_currentState; }
    std::string GetPreviousState() const { return m_previousState; }
    bool IsTransitioning() const { return m_isTransitioning; }
    float GetTransitionProgress() const { return m_transitionProgress; }
    float GetCurrentStateTime() const { return m_currentStateTime; }
    float GetCurrentStateNormalizedTime() const; // 0.0 ~ 1.0
    
    // 서브 상태 머신
    void AddSubStateMachine(const std::string& name, std::unique_ptr<AnimationStateMachine> subMachine);
    void RemoveSubStateMachine(const std::string& name);
    AnimationStateMachine* GetSubStateMachine(const std::string& name);
    
    // 강제 전환
    void ForceTransition(const std::string& toState, float blendTime = 0.2f);
    
    // 디버깅
    void LogCurrentState() const;
    void LogAllStates() const;
    void LogAllTransitions() const;
    void LogAllParameters() const;
    
    // 시각화용 정보 (에디터에서 사용)
    struct DebugInfo
    {
        std::vector<std::string> stateNames;
        std::vector<std::pair<std::string, std::string>> transitionPairs;
        std::string currentState;
        std::string targetState;
        float transitionProgress;
        std::unordered_map<std::string, Parameter> parameters;
    };
    DebugInfo GetDebugInfo() const;

private:
    void UpdateTransitions();
    void ExecuteTransition(const Transition& transition);
    void UpdateCurrentState(float deltaTime);
    void UpdateParameters();
    
    // 블렌딩 계산
    void UpdateBlending(float deltaTime);
    
    // 애니메이션 시스템 연동
    void ApplyStateToAnimationSystem();
    
    // 리소스
    SkeletalAnimationSystem* m_animationSystem = nullptr;
    
    // 상태 머신 데이터
    std::unordered_map<std::string, State> m_states;
    std::vector<Transition> m_transitions;
    std::unordered_map<std::string, Parameter> m_parameters;
    std::unordered_map<std::string, SubStateMachine> m_subStateMachines;
    
    // 현재 상태
    std::string m_currentState;
    std::string m_previousState;
    std::string m_targetState;
    
    // 전환 상태
    bool m_isTransitioning = false;
    float m_transitionProgress = 0.0f;
    float m_transitionDuration = 0.0f;
    
    // 시간 추적
    float m_currentStateTime = 0.0f;
    float m_previousStateTime = 0.0f;
    
    // 상태 머신 상태
    bool m_isRunning = false;
    bool m_isInitialized = false;
    
    // 블렌딩 가중치
    float m_currentStateWeight = 1.0f;
    float m_previousStateWeight = 0.0f;
    
    // 디버깅
    bool m_enableDebugLogging = false;
    
    // 상수
    static constexpr float EPSILON = 0.001f;
};

// 상태 머신 빌더 (편의 클래스)
class StateMachineBuilder
{
public:
    StateMachineBuilder();
    
    // 상태 추가
    StateMachineBuilder& AddState(const std::string& name, const std::string& animationName);
    StateMachineBuilder& SetStateSpeed(const std::string& stateName, float speed);
    StateMachineBuilder& SetStateLooping(const std::string& stateName, bool looping);
    StateMachineBuilder& SetStateBlendTimes(const std::string& stateName, float blendIn, float blendOut);
    
    // 전환 추가
    StateMachineBuilder& AddTransition(const std::string& from, const std::string& to);
    StateMachineBuilder& SetTransitionBlendTime(float blendTime);
    StateMachineBuilder& SetTransitionExitTime(float exitTime);
    StateMachineBuilder& AddCondition(const std::string& parameter, AnimationStateMachine::Condition::CompareType compareType, float value);
    StateMachineBuilder& AddCondition(const std::string& parameter, AnimationStateMachine::Condition::CompareType compareType, int value);
    StateMachineBuilder& AddCondition(const std::string& parameter, AnimationStateMachine::Condition::CompareType compareType, bool value);
    
    // 파라미터 추가
    StateMachineBuilder& AddFloatParameter(const std::string& name, float defaultValue = 0.0f);
    StateMachineBuilder& AddIntParameter(const std::string& name, int defaultValue = 0);
    StateMachineBuilder& AddBoolParameter(const std::string& name, bool defaultValue = false);
    StateMachineBuilder& AddTriggerParameter(const std::string& name);
    
    // 빌드
    std::unique_ptr<AnimationStateMachine> Build();

private:
    std::unique_ptr<AnimationStateMachine> m_stateMachine;
    AnimationStateMachine::Transition* m_currentTransition = nullptr;
};

} // namespace Lunar
