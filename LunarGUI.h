#pragma once

#include "imgui.h"
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include "Logger.h"
#include <any>
#include <d3d12.h>

namespace Lunar 
{

class LunarGui 
{
private:
    enum class UIElementType : int8
    {
        Checkbox,
        Slider
    };

    struct BoundValue 
    {
        UIElementType type = UIElementType::Checkbox;
        void* dataPtr;
        std::function<void(void*)> onChange;
        std::any min;
        std::any max;
        
        template<typename T>
        T* GetAs() { return static_cast<T*>(dataPtr); }
    	
        template<typename T>
        T GetMinValue() const 
        {
            try 
            {
                return std::any_cast<T>(min);
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
                return std::any_cast<T>(max);
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
	T* GetBoundValue(const std::string& id)
	{
		auto it = m_boundValues.find(id);
		if (it != m_boundValues.end()) 
		{
			return static_cast<T*>(it->second.dataPtr);
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