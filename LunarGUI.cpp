#include "LunarGui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

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
		const std::string& id = pair.first;
		BoundValue& value = pair.second;
    
		if (value.type == UIElementType::Checkbox) 
		{
			bool* boolValue = static_cast<bool*>(value.dataPtr);
			// ImGui::Checkbox returns true if the value changed.
			if (ImGui::Checkbox(id.c_str(), boolValue) && value.onChange) 
			{
				value.onChange(value.dataPtr);
			}
		}
		else if (value.type == UIElementType::Slider)
		{
			float* floatValue = static_cast<float*>(value.dataPtr);
			float minValue = value.GetMinValue<float>();
			float maxValue = value.GetMaxValue<float>();
			if (ImGui::SliderFloat(id.c_str(), floatValue, minValue, maxValue) && value.onChange) 
			{
				value.onChange(value.dataPtr);
			}
		}
	}

	for (auto& pair : m_callbacks) 
	{
		const std::string& id = pair.first;
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

void LunarGui::BindCheckbox(const std::string& id, bool* value, std::function<void(bool)> onChange)
{
	if (GetBoundValue<bool>(id)) 
	{
		LOG_ERROR("Value with ID '%s' already bound.", id);
		return;
	}

	BoundValue boundValue;
	boundValue.type = UIElementType::Checkbox;
	boundValue.dataPtr = value;
        

	if (onChange) 
	{
		boundValue.onChange = [onChange](bool data) 
		{
			onChange(data);
		};
	}
        
	m_boundValues[id] = boundValue;
}

bool LunarGui::RegisterCallback(const std::string& id, std::function<void()> callback)
{
	if (m_callbacks.find(id) != m_callbacks.end()) 
	{
		LOG_ERROR("Callback with ID '%s' already registered.", id);
		return false;
	} 
	m_callbacks[id] = callback;
	return true;
}

template <typename T>
void LunarGui::BindSlider(const std::string& id, T* value, T* min, T* max, std::function<void(T*)> onChange)
{
	if (GetBoundValue<T>(id)) 
	{
		LOG_ERROR("Value with ID '%s' already bound.", id); 
		return;
	}

	BoundValue boundValue;
	boundValue.type = UIElementType::Slider;
	boundValue.dataPtr = value;
	boundValue.min = min;
	boundValue.max = max;
    
	if (onChange) 
	{
		boundValue.onChange = [onChange](void* data) { onChange(static_cast<T*>(data)); };
	}
    
	m_boundValues[id] = boundValue;
}

template <typename T>
T* LunarGui::GetBoundValue(const std::string& id)
{
	auto it = m_boundValues.find(id);
	if (it != m_boundValues.end()) 
	{
		return static_cast<T*>(it->second.dataPtr);
	}
	return nullptr;
}

}