#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <any>
#include <d3d12.h>
#include <DirectXMath.h>

#include "imgui.h"
#include "Logger.h"

namespace Lunar 
{

class LunarGui 
{
private:
    enum class UIElementType : int8_t
    {
        Checkbox,
        Slider,
        ListBox
    };

    enum class DataType {
        Float,
        Int,
        Bool,
        Float3
    };

    struct BoundValue 
    {
        UIElementType ElementType = UIElementType::Checkbox;
        DataType DataType = DataType::Float;
        void* DataPtr;
        std::function<void(void*)> OnChange;
        std::any Min;
        std::any Max;
        int* SelectedValue;
        
        template<typename T>
        T* GetAs() { return static_cast<T*>(DataPtr); }
    	
        template<typename T>
        T GetMinValue() const 
        {
            try 
            {
                return std::any_cast<T>(Min);
            }
            catch (const std::bad_any_cast& e)
            {
                LOG_ERROR("Failed to get min value: ", e.what());
                return T();
            }
        }
    	
        template<typename T>
        T GetMaxValue() const
        {
            try
            {
                return std::any_cast<T>(Max);
            }
            catch (const std::bad_any_cast& e)
            {
                LOG_ERROR("Failed to get max value: ", e.what());
                return T();
            }
        }
    };
    
public:
	LunarGui();
	~LunarGui();
    
	bool Initialize(HWND hwnd, ID3D12Device* device, UINT bufferCount, DXGI_FORMAT format, 
				   ID3D12DescriptorHeap* heap);
	void Shutdown();
    
	void BeginFrame();
	void Render(float dt);
	void EndFrame();
    
	void BindCheckbox(const std::string& id, bool* value, std::function<void(bool)> onChange = nullptr);
	
	template <typename T>
	void BindSlider(const std::string& id, T* value, T min, T max, std::function<void(T*)> onChange = nullptr)
	{
		auto it = m_boundValues.find(id);
		if (it != m_boundValues.end()) 
		{
			LOG_ERROR("Value with ID '%s' already bound.", id); 
			return;
		}

		BoundValue boundValue;
		boundValue.ElementType = UIElementType::Slider;
		boundValue.DataPtr = value;
		boundValue.Min = std::any(min);
		boundValue.Max = std::any(max);

        if constexpr (std::is_same_v<T, float>)
        {
            boundValue.DataType = DataType::Float;
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            boundValue.DataType = DataType::Int;
        }
        else if constexpr (std::is_same_v<T, DirectX::XMFLOAT3>)
        {
            boundValue.DataType = DataType::Float3;
        }
		else
		{
			LOG_ERROR(typeid(T).name(), " is not supported.");
		}
    
		if (onChange) 
		{
			boundValue.OnChange = [onChange](void* data) { onChange(static_cast<T*>(data)); };
		}
    
		m_boundValues[id] = boundValue;
	}

    void BindListBox(const std::string& id, int* value, std::vector<std::string>* items, std::function<void(std::vector<std::string>*)> onChange = nullptr);

	template <typename T>
	T* GetBoundValue(const std::string& id)
	{
		auto it = m_boundValues.find(id);
		if (it != m_boundValues.end()) 
		{
			return static_cast<T*>(it->second.DataPtr);
		}
		return nullptr;
	}

    bool RegisterCallback(const std::string& id, std::function<void()> callback);

private:
    bool m_initialized;
	std::unordered_map<std::string, BoundValue> m_boundValues;
	std::unordered_map<std::string, std::function<void()>> m_callbacks;
};

} // namespace Lunar