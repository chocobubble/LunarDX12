#include "LunarGui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <DirectXMath.h>

using namespace std;
using namespace DirectX;

namespace Lunar
{
LunarGui::LunarGui() : m_initialized(false) { }

LunarGui::~LunarGui() 
{
	if (m_initialized) {
		Shutdown();
	}
}

bool LunarGui::Initialize(HWND hwnd, ID3D12Device* device, UINT bufferCount, DXGI_FORMAT format, ID3D12DescriptorHeap* heap) 
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(
		device,
		bufferCount,
		format,
		heap,
		heap->GetCPUDescriptorHandleForHeapStart(),
		heap->GetGPUDescriptorHandleForHeapStart()
	);

	m_initialized = true;
	return true;
}

void LunarGui::Shutdown() 
{
	if (!m_initialized) return;

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_initialized = false;
}

void LunarGui::BeginFrame() 
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void LunarGui::Render(float dt) 
{
    BeginFrame();

    // Main settings window - only main category elements
    RenderCategory("Settings", "main", dt);
    
    // Render bound windows
    for (const auto& [windowId, windowData] : m_boundWindows)
    {
        RenderBoundWindow(windowId, windowData);
    }
    
    EndFrame();
}

void LunarGui::EndFrame() 
{
	ImGui::Render();
}

void LunarGui::BindCheckbox(const string& id, bool* value, function<void(bool)> onChange, const string& category)
{
	if (GetBoundValue<bool>(id)) 
	{
		LOG_ERROR("Value with ID '%s' already bound.", id);
		return;
	}

	BoundValue boundValue;
	boundValue.ElementType = UIElementType::Checkbox;
	boundValue.DataPtr = value;
	boundValue.Category = category;

	if (onChange) 
	{
		boundValue.OnChange = [onChange](void* data) 
		{
			onChange(*static_cast<bool*>(data));
		};
	}
        
	m_boundValues[id] = boundValue;
}

void LunarGui::BindListBox(const string& id, int* value, vector<string>* items, function<void(vector<string>*)> onChange)
{
    auto it = m_boundValues.find(id);
    if (it != m_boundValues.end())
    {
        LOG_ERROR("Value with ID '%s' already bound.", id);
        return;
    }

    BoundValue boundValue;
    boundValue.ElementType = UIElementType::ListBox;
	boundValue.SelectedValue = value;
    boundValue.DataPtr = items;
    boundValue.Max = any(static_cast<int>(items->size()));

    if (onChange)
    {
        boundValue.OnChange = [onChange](void* data) { onChange(static_cast<vector<string>*>(data)); };
    }

    m_boundValues[id] = boundValue;
}

bool LunarGui::RegisterCallback(const string& id, function<void()> callback)
{
	if (m_callbacks.find(id) != m_callbacks.end()) 
	{
		LOG_ERROR("Callback with ID '%s' already registered.", id);
		return false;
	} 
	m_callbacks[id] = callback;
	return true;
}

void LunarGui::BindReadOnlyFloat(const string& id, const float* value, const string& label, const string& format)
{
    if (GetBoundValue<float>(id)) 
    {
        LOG_ERROR("Value with ID '%s' already bound.", id);
        return;
    }

    BoundValue boundValue;
    boundValue.ElementType = UIElementType::ReadOnlyFloat;
    boundValue.DataType = DataType::Float;
    boundValue.DataPtr = const_cast<float*>(value); // Safe: we won't modify it
    boundValue.Label = label;
    boundValue.Format = format;
    
    m_boundValues[id] = boundValue;
}

void LunarGui::BindReadOnlyText(const string& id, const string* text)
{
    if (GetBoundValue<string>(id)) 
    {
        LOG_ERROR("Value with ID '%s' already bound.", id);
        return;
    }

    BoundValue boundValue;
    boundValue.ElementType = UIElementType::ReadOnlyText;
    boundValue.DataType = DataType::Text;
    boundValue.DataPtr = const_cast<string*>(text); // Safe: we won't modify it
    
    m_boundValues[id] = boundValue;
}

void LunarGui::BindText(const string& id, string* text)
{
    if (GetBoundValue<string>(id)) 
    {
        LOG_ERROR("Value with ID '%s' already bound.", id);
        return;
    }

    BoundValue boundValue;
    boundValue.ElementType = UIElementType::Text;
    boundValue.DataType = DataType::Text;
    boundValue.DataPtr = text;
    
    m_boundValues[id] = boundValue;
}

void LunarGui::BindGraph(const string& id, GraphData* graphData)
{
    if (GetBoundValue<GraphData>(id)) 
    {
        LOG_ERROR("Value with ID '%s' already bound.", id);
        return;
    }

    BoundValue boundValue;
    boundValue.ElementType = UIElementType::Graph;
    boundValue.DataType = DataType::Graph;
    boundValue.DataPtr = graphData;
    
    m_boundValues[id] = boundValue;
}

void LunarGui::BindTable(const string& id, TableData* tableData)
{
    if (GetBoundValue<TableData>(id)) 
    {
        LOG_ERROR("Value with ID '%s' already bound.", id);
        return;
    }

    BoundValue boundValue;
    boundValue.ElementType = UIElementType::Table;
    boundValue.DataType = DataType::Table;
    boundValue.DataPtr = tableData;
    
    m_boundValues[id] = boundValue;
}

void LunarGui::BindWindow(const string& id, const string& title, bool* isOpen, const vector<string>& elementIds)
{
    if (m_boundWindows.find(id) != m_boundWindows.end())
    {
        LOG_ERROR("Window with ID '%s' already bound.", id);
        return;
    }
    
    WindowData windowData;
    windowData.title = title;
    windowData.isOpen = isOpen;
    windowData.elementIds = elementIds;
    
    m_boundWindows[id] = windowData;
}

void LunarGui::RenderBoundWindow(const string& windowId, const WindowData& windowData)
{
    if (!windowData.isOpen || !*windowData.isOpen) return;
    
    if (ImGui::Begin(windowData.title.c_str(), windowData.isOpen))
    {
        // Render all bound elements for this window
        for (const string& elementId : windowData.elementIds)
        {
            auto it = m_boundValues.find(elementId);
            if (it != m_boundValues.end())
            {
                const BoundValue& value = it->second;
                
                switch (value.ElementType)
                {
                    case UIElementType::ReadOnlyFloat:
                    {
                        const float* floatPtr = static_cast<const float*>(value.DataPtr);
                        string labelAndFormat = value.Label.empty() ? value.Format : value.Label + ": " + value.Format;
                        ImGui::Text(labelAndFormat.c_str(), *floatPtr);
                        break;
                    }
                    case UIElementType::ReadOnlyText:
                    {
                        const string* textPtr = static_cast<const string*>(value.DataPtr);
                        ImGui::Text("%s", textPtr->c_str());
                        break;
                    }
                    case UIElementType::Text:
                    {
                        string* textPtr = static_cast<string*>(value.DataPtr);
                        ImGui::Text("%s", textPtr->c_str());
                        break;
                    }
                    case UIElementType::Graph:
                    {
                        GraphData* graphData = static_cast<GraphData*>(value.DataPtr);
                    	if (graphData->updateCallback) graphData->updateCallback();
                        if (!graphData->values.empty())
                        {
                            ImGui::PlotLines(graphData->label.c_str(),
                                           graphData->values.data(),
                                           static_cast<int>(graphData->values.size()),
                                           0, nullptr,
                                           graphData->minValue,
                                           graphData->maxValue,
                                           graphData->size);
                        }
                        break;
                    }
                    case UIElementType::Table:
                    {
                        TableData* tableData = static_cast<TableData*>(value.DataPtr);
                        if (!tableData->headers.empty())
                        {
                        	if (tableData->updateCallback)
                        	{
                        		tableData->updateCallback();
                        	}
                            if (ImGui::BeginTable(elementId.c_str(), 
                                                static_cast<int>(tableData->headers.size()), 
                                                tableData->flags))
                            {
                                for (const string& header : tableData->headers)
                                {
                                    ImGui::TableSetupColumn(header.c_str());
                                }
                                ImGui::TableHeadersRow();
                                
                                for (const auto& row : tableData->rows)
                                {
                                    ImGui::TableNextRow();
                                    for (const string& cell : row)
                                    {
                                        ImGui::TableNextColumn();
                                        ImGui::Text("%s", cell.c_str());
                                    }
                                }
                                
                                ImGui::EndTable();
                            }
                        }
                        break;
                    }
                	case UIElementType::Checkbox:
                    {
                        bool* boolValue = static_cast<bool*>(value.DataPtr);
                        if (ImGui::Checkbox(elementId.c_str(), boolValue) && value.OnChange)
                        {
                            value.OnChange(value.DataPtr);
                        }
                        break;
                    }
                    case UIElementType::Slider:
                    {
	                    switch (value.DataType)
	                    {
	                    	case DataType::Float:
	                    	{
	                    		float* floatValue = static_cast<float*>(value.DataPtr);
	                    		float minValue = value.GetMinValue<float>();
	                    		float maxValue = value.GetMaxValue<float>();
	                    		if (ImGui::SliderFloat(elementId.c_str(), floatValue, minValue, maxValue) && value.OnChange)
	                    		{
	                    			value.OnChange(value.DataPtr);
	                    		}
	                    		break;
	                    	}
	                    	case DataType::Int:
	                    	{
	                    		int* intValue = static_cast<int*>(value.DataPtr);
	                    		int minValue = value.GetMinValue<int>();
	                    		int maxValue = value.GetMaxValue<int>();
	                    		if (ImGui::SliderInt(elementId.c_str(), intValue, minValue, maxValue) && value.OnChange)
	                    		{
	                    			value.OnChange(value.DataPtr);
	                    		}
	                    		break;
	                    	}
	                    	case DataType::Float3:
	                    	{
	                    		float* dataPtr = reinterpret_cast<float*>(value.DataPtr);
	                    		auto minValue = value.GetMinValue<XMFLOAT3>();
	                    		auto maxValue = value.GetMaxValue<XMFLOAT3>();
	                    		if (ImGui::SliderFloat3(elementId.c_str(), dataPtr, minValue.x, maxValue.x) && value.OnChange)
	                    		{
	                    			value.OnChange(value.DataPtr);
	                    		}
	                    		break;
	                    	}
	                    }
                    	break;
                    }
                    default:
                        break;
                }
            }
        }
    }
    ImGui::End();
}

void LunarGui::RenderCategory(const string& windowTitle, const string& category, float dt)
{
    ImGui::Begin(windowTitle.c_str());
    
    if (category == "main" && dt > 0.0f)
        ImGui::Text("FPS: %.2f", 1.0f / dt);
    
    for (auto& pair : m_boundValues)
    {
        const string& id = pair.first;
        BoundValue& value = pair.second;
        
        if (value.Category == category)
        {
            RenderElement(id, value);
        }
    }
    
    if (category == "main")
    {
        for (auto& pair : m_callbacks) 
        {
            if (ImGui::Button(pair.first.c_str())) 
                pair.second(); 
        }
    }
    
    ImGui::End();
}

void LunarGui::RenderElement(const string& id, BoundValue& value)
{
    switch (value.ElementType)
    {
        case UIElementType::Checkbox:
        {
            bool* boolValue = static_cast<bool*>(value.DataPtr);
            if (ImGui::Checkbox(id.c_str(), boolValue) && value.OnChange) 
            {
                value.OnChange(value.DataPtr);
            }
            break;
        }
        default:
            break;
    }
}

}