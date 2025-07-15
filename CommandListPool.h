#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>

namespace Lunar
{

struct CommandListContext {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    UINT64 fenceValue = 0;
    bool inUse = false;
    
    // 성능 추적
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;
};

class CommandListPool {
public:
    CommandListPool();
    ~CommandListPool();
    
    void Initialize(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, size_t poolSize = 4);
    void Shutdown();
    
    // Command List 대여/반납
    CommandListContext* GetAvailableCommandList(UINT64 completedFenceValue);
    void ReturnCommandList(CommandListContext* context);
    
    // 통계 정보
    struct PoolStats {
        size_t totalContexts = 0;
        size_t availableContexts = 0;
        size_t inUseContexts = 0;
        float averageExecutionTime = 0.0f;
        size_t totalExecutions = 0;
    };
    PoolStats GetStats() const;
    
    // 디버깅
    void LogPoolStatus() const;
    
private:
    void CreateCommandListContext(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
    void UpdateStats(const CommandListContext* context);
    
    std::vector<std::unique_ptr<CommandListContext>> m_contexts;
    std::queue<size_t> m_availableIndices;
    mutable std::mutex m_poolMutex;
    
    ID3D12Device* m_device = nullptr;
    D3D12_COMMAND_LIST_TYPE m_commandListType;
    
    // 성능 통계
    mutable std::mutex m_statsMutex;
    std::atomic<size_t> m_totalExecutions{0};
    std::atomic<float> m_totalExecutionTime{0.0f};
    
    bool m_initialized = false;
};

// RAII 스타일 Command List 관리
class ScopedCommandList {
public:
    ScopedCommandList(CommandListPool* pool, UINT64 completedFenceValue);
    ~ScopedCommandList();
    
    // 복사 방지
    ScopedCommandList(const ScopedCommandList&) = delete;
    ScopedCommandList& operator=(const ScopedCommandList&) = delete;
    
    // 이동 허용
    ScopedCommandList(ScopedCommandList&& other) noexcept;
    ScopedCommandList& operator=(ScopedCommandList&& other) noexcept;
    
    ID3D12GraphicsCommandList* Get() const { return m_context ? m_context->commandList.Get() : nullptr; }
    ID3D12GraphicsCommandList* operator->() const { return Get(); }
    CommandListContext* GetContext() const { return m_context; }
    
    bool IsValid() const { return m_context != nullptr; }
    
private:
    CommandListPool* m_pool = nullptr;
    CommandListContext* m_context = nullptr;
};

} // namespace Lunar
