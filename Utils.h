#pragma once
#include <exception>
#include <Windows.h> 

namespace  Lunar
{
	
#ifndef THROW_IF_FAILED 
#define THROW_IF_FAILED(x) { HRESULT hr = x; if (FAILED(hr)) { throw std::exception(); } }
#endif

class Utils
{
public:
	static UINT CalculateConstantBufferByteSize(UINT byteSize)
	{
		// Constant buffers must be a multiple of the minimum hardware allocation size (usually 256 bytes)
		return (byteSize + 255) & ~255;
	}	
};
	
} // namespace Lunar