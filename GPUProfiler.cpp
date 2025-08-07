#include "GPUProfiler.h"
#include "Utils/Logger.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Lunar
{

GPUProfiler::GPUProfiler()
    : m_maxEvents(256)
    , m_currentQueryIndex(0)
    , m_timestampFrequency(0)
    , m_frameInProgress(false)
    , m_enabled(true)
    , m_currentFrameTriangles(0)
    , m_currentFrameDrawCalls(0)
{
}

GPUProfiler::~GPUProfiler()
{
    Shutdown();
}

void GPUProfiler::Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, UINT maxEvents)
{
    m_device = device;
    m_commandQueue = commandQueue;
    m_maxEvents = maxEvents;
    
    m_events.reserve(m_maxEvents);
    
    CreateQueryHeap();
    CreateReadbackBuffer();
    
    m_timestampFrequency = GetTimestampFrequency();
    
    Logger::Log("GPUProfiler initialized with " + std::to_string(m_maxEvents) + " max events");
    Logger::Log("GPU timestamp frequency: " + std::to_string(m_timestampFrequency) + " Hz");
}

void GPUProfiler::Shutdown()
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    m_timestampHeap.Reset();
    m_readbackBuffer.Reset();
    m_device.Reset();
    m_commandQueue.Reset();
    
    m_events.clear();
    m_activeEvents.clear();
}

void GPUProfiler::CreateQueryHeap()
{
    D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    queryHeapDesc.Count = m_maxEvents * 2; // 시작과 끝 타임스탬프
    queryHeapDesc.NodeMask = 0;

    HRESULT hr = m_device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_timestampHeap));
    if (FAILED(hr))
    {
        Logger::Log("Failed to create timestamp query heap");
        return;
    }
}

void GPUProfiler::CreateReadbackBuffer()
{
    UINT64 bufferSize = m_maxEvents * 2 * sizeof(UINT64);
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = bufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_readbackBuffer)
    );

    if (FAILED(hr))
    {
        Logger::Log("Failed to create readback buffer for GPU profiler");
    }
}

UINT64 GPUProfiler::GetTimestampFrequency()
{
    UINT64 frequency = 0;
    HRESULT hr = m_commandQueue->GetTimestampFrequency(&frequency);
    if (FAILED(hr))
    {
        Logger::Log("Failed to get timestamp frequency, using default");
        return 1000000; // 기본값 1MHz
    }
    return frequency;
}

void GPUProfiler::BeginFrame(ID3D12GraphicsCommandList* cmdList)
{
    if (!m_enabled) return;
    
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    // 이전 프레임 정리
    m_events.clear();
    m_activeEvents.clear();
    m_currentQueryIndex = 0;
    m_currentFrameTriangles = 0;
    m_currentFrameDrawCalls = 0;
    
    m_frameInProgress = true;
}

void GPUProfiler::EndFrame(ID3D12GraphicsCommandList* cmdList)
{
    if (!m_enabled || !m_frameInProgress) return;
    
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    // 활성 이벤트들 강제 종료
    for (auto& pair : m_activeEvents)
    {
        EndEvent(cmdList);
    }
    
    // 타임스탬프 결과를 readback 버퍼로 복사
    if (m_currentQueryIndex > 0)
    {
        cmdList->ResolveQueryData(
            m_timestampHeap.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            0,
            m_currentQueryIndex,
            m_readbackBuffer.Get(),
            0
        );
    }
    
    m_frameInProgress = false;
}

void GPUProfiler::BeginEvent(ID3D12GraphicsCommandList* cmdList, const std::string& name)
{
    if (!m_enabled || !m_frameInProgress) return;
    
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    if (m_currentQueryIndex >= m_maxEvents * 2 - 1) return; // 공간 부족
    
    EventData event;
    event.name = name;
    event.queryStartIndex = m_currentQueryIndex++;
    event.queryEndIndex = 0; // 나중에 설정
    event.triangleCount = 0;
    event.drawCalls = 0;
    event.cpuStartTime = std::chrono::high_resolution_clock::now();
    event.isActive = true;
    
    // GPU 타임스탬프 기록
    cmdList->EndQuery(m_timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, event.queryStartIndex);
    
    size_t eventIndex = m_events.size();
    m_events.push_back(event);
    m_activeEvents[name] = eventIndex;
}

void GPUProfiler::EndEvent(ID3D12GraphicsCommandList* cmdList)
{
    if (!m_enabled || !m_frameInProgress) return;
    
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    if (m_activeEvents.empty()) return;
    
    // 가장 최근에 시작된 이벤트 종료 (스택 방식)
    auto lastEvent = m_activeEvents.rbegin();
    size_t eventIndex = lastEvent->second;
    
    if (eventIndex >= m_events.size()) return;
    
    EventData& event = m_events[eventIndex];
    if (!event.isActive) return;
    
    if (m_currentQueryIndex >= m_maxEvents * 2) return;
    
    event.queryEndIndex = m_currentQueryIndex++;
    event.cpuEndTime = std::chrono::high_resolution_clock::now();
    event.isActive = false;
    
    // GPU 타임스탬프 기록
    cmdList->EndQuery(m_timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, event.queryEndIndex);
    
    m_activeEvents.erase(lastEvent->first);
}

void GPUProfiler::AddDrawCall(UINT64 triangleCount)
{
    if (!m_enabled) return;
    
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    m_currentFrameDrawCalls++;
    m_currentFrameTriangles += triangleCount;
    
    // 현재 활성 이벤트에 추가
    if (!m_activeEvents.empty())
    {
        auto lastEvent = m_activeEvents.rbegin();
        size_t eventIndex = lastEvent->second;
        if (eventIndex < m_events.size())
        {
            m_events[eventIndex].drawCalls++;
            m_events[eventIndex].triangleCount += triangleCount;
        }
    }
}

void GPUProfiler::SetTriangleCount(const std::string& eventName, UINT64 triangleCount)
{
    if (!m_enabled) return;
    
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    auto it = m_activeEvents.find(eventName);
    if (it != m_activeEvents.end())
    {
        size_t eventIndex = it->second;
        if (eventIndex < m_events.size())
        {
            m_events[eventIndex].triangleCount = triangleCount;
        }
    }
}

bool GPUProfiler::IsFrameComplete() const
{
    return !m_frameInProgress;
}

GPUProfiler::FrameStats GPUProfiler::GetLastFrameStats()
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_lastFrameStats;
}

void GPUProfiler::ResolveTimestamps()
{
    if (!m_enabled || m_frameInProgress) return;
    
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    if (m_events.empty()) return;
    
    // readback 버퍼에서 타임스탬프 읽기
    UINT64* timestampData = nullptr;
    D3D12_RANGE readRange = { 0, m_currentQueryIndex * sizeof(UINT64) };
    
    HRESULT hr = m_readbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&timestampData));
    if (FAILED(hr))
    {
        Logger::Log("Failed to map readback buffer");
        return;
    }
    
    // 프레임 통계 초기화
    m_lastFrameStats = {};
    m_lastFrameStats.events.clear();
    m_lastFrameStats.events.reserve(m_events.size());
    
    // 각 이벤트의 시간 계산
    for (const auto& event : m_events)
    {
        if (event.isActive) continue; // 완료되지 않은 이벤트 스킵
        
        TimingResult result;
        result.name = event.name;
        result.triangleCount = event.triangleCount;
        result.drawCalls = event.drawCalls;
        result.queryStartIndex = event.queryStartIndex;
        result.queryEndIndex = event.queryEndIndex;
        
        // CPU 시간 계산
        auto cpuDuration = std::chrono::duration_cast<std::chrono::microseconds>(
            event.cpuEndTime - event.cpuStartTime);
        result.cpuTimeMs = cpuDuration.count() / 1000.0f;
        
        // GPU 시간 계산
        if (event.queryEndIndex < m_currentQueryIndex && event.queryStartIndex < m_currentQueryIndex)
        {
            UINT64 startTime = timestampData[event.queryStartIndex];
            UINT64 endTime = timestampData[event.queryEndIndex];
            
            if (endTime >= startTime)
            {
                UINT64 gpuTicks = endTime - startTime;
                result.gpuTimeMs = (float)(gpuTicks * 1000.0 / m_timestampFrequency);
                result.isComplete = true;
            }
        }
        
        m_lastFrameStats.events.push_back(result);
        m_lastFrameStats.totalGPUTime += result.gpuTimeMs;
        m_lastFrameStats.totalCPUTime += result.cpuTimeMs;
        m_lastFrameStats.totalTriangles += result.triangleCount;
        m_lastFrameStats.totalDrawCalls += result.drawCalls;
    }
    
    D3D12_RANGE writeRange = { 0, 0 };
    m_readbackBuffer->Unmap(0, &writeRange);
}

void GPUProfiler::LogFrameStats() const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    if (m_lastFrameStats.events.empty()) return;
    
    std::stringstream ss;
    ss << "\n=== GPU Profiler Frame Stats ===\n";
    ss << "Total GPU Time: " << std::fixed << std::setprecision(3) << m_lastFrameStats.totalGPUTime << " ms\n";
    ss << "Total CPU Time: " << std::fixed << std::setprecision(3) << m_lastFrameStats.totalCPUTime << " ms\n";
    ss << "Total Draw Calls: " << m_lastFrameStats.totalDrawCalls << "\n";
    ss << "Total Triangles: " << m_lastFrameStats.totalTriangles << "\n\n";
    
    ss << "Event Details:\n";
    ss << std::left << std::setw(25) << "Event Name" 
       << std::setw(12) << "GPU (ms)" 
       << std::setw(12) << "CPU (ms)" 
       << std::setw(10) << "Draw Calls" 
       << std::setw(12) << "Triangles" << "\n";
    ss << std::string(80, '-') << "\n";
    
    for (const auto& event : m_lastFrameStats.events)
    {
        ss << std::left << std::setw(25) << event.name
           << std::setw(12) << std::fixed << std::setprecision(3) << event.gpuTimeMs
           << std::setw(12) << std::fixed << std::setprecision(3) << event.cpuTimeMs
           << std::setw(10) << event.drawCalls
           << std::setw(12) << event.triangleCount << "\n";
    }
    
    Logger::Log(ss.str());
}

} // namespace Lunar
