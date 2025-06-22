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
	ImGui::Begin("Settings");
	ImGui::Text("FPS: %.2f", 1.0f / dt);

	for (auto& pair : m_boundValues) 
	{
		const string& id = pair.first;
		BoundValue&value = pair.second;
    
		if (value.ElementType == UIElementType::Checkbox) 
		{
			bool* boolValue = static_cast<bool*>(value.DataPtr);
			// ImGui::Checkbox returns true if the value changed.
			if (ImGui::Checkbox(id.c_str(), boolValue) && value.OnChange) 
			{
				value.OnChange(value.DataPtr);
			}
		}
		else if (value.ElementType == UIElementType::Slider)
		{
            switch (value.DataType)
            {
                case DataType::Int:
                {
                    int* intValue = static_cast<int*>(value.DataPtr);
                    int minValue = value.GetMinValue<int>();
                    int maxValue = value.GetMaxValue<int>();
                    if (ImGui::SliderInt(id.c_str(), intValue, minValue, maxValue) && value.OnChange)
                    {
                        value.OnChange(value.DataPtr);
                    }
                    break;
                }
                case DataType::Float:
                {
                    float* floatValue = static_cast<float*>(value.DataPtr);
                    float minValue = value.GetMinValue<float>();
                    float maxValue = value.GetMaxValue<float>();
                    if (ImGui::SliderFloat(id.c_str(), floatValue, minValue, maxValue) && value.OnChange)
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
                    if (ImGui::SliderFloat3(id.c_str(), dataPtr, minValue.x, maxValue.x) && value.OnChange)
                    {
                        value.OnChange(value.DataPtr);
                    }
                	break;
                }
                default:
                    LOG_ERROR("Invalid data type for slider.");
                    break;
            }
		}
        else if (value.ElementType == UIElementType::ListBox)
        {
	        const char* const* items = static_cast<const char* const*>(value.DataPtr);
            if (ImGui::ListBox(id.c_str(), value.SelectedValue, items, value.GetMaxValue<int>()) && value.OnChange)
            {
                value.OnChange(value.DataPtr);
            }
        }
	}

	for (auto& pair : m_callbacks) 
	{
		const string& id = pair.first;
		auto& callback = pair.second;
    
		if (ImGui::Button(id.c_str())) callback(); 
	}

	ImGui::End();
    EndFrame();
}

void LunarGui::EndFrame() 
{
	ImGui::Render();
}

void LunarGui::BindCheckbox(const string& id, bool* value, function<void(bool)> onChange)
{
	if (GetBoundValue<bool>(id)) 
	{
		LOG_ERROR("Value with ID '%s' already bound.", id);
		return;
	}

	BoundValue boundValue;
	boundValue.ElementType = UIElementType::Checkbox;
	boundValue.DataPtr = value;
        

	if (onChange) 
	{
		boundValue.OnChange = [onChange](bool data) 
		{
			onChange(data);
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
    boundValue.ElementType = UIElementType::Checkbox;
	boundValue.SelectedValue = value;
    boundValue.DataPtr = items;
    boundValue.Max = items->size();

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

}