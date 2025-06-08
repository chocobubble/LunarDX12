#pragma once
#include <dxgi1_6.h>

namespace Lunar 
{
namespace Constants
{
	
constexpr DXGI_FORMAT SWAP_CHAIN_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr UINT SAMPLE_COUNT = 1;  // MSAA 사용하지 않음
constexpr UINT BUFFER_COUNT = 2;
	
}	
}
