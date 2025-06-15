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

bool LunarGui::Initialize(HWND hwnd, ID3D12Device* device, int bufferCount, DXGI_FORMAT format, ID3D12DescriptorHeap* heap) 
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

void LunarGui::Render() 
{
	ImGui::Begin("Settings");

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
}

void LunarGui::EndFrame() 
{
	ImGui::Render();
}
}