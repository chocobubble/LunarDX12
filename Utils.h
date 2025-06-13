#pragma once
#include <exception>
#include <string>
#include <Windows.h> 

namespace  Lunar
{
class LunarException
{
public:
	LunarException(HRESULT hr, std::string function, std::string file, int line)
		: m_hr(hr), m_function(function), m_file(file), m_line(line) {}
	std::string ToString() const;
	HRESULT m_hr;
	std::string m_function;
	std::string m_file;
	int m_line;    
};

#ifndef THROW_IF_FAILED 
#define THROW_IF_FAILED(x) { HRESULT hr = x; if (FAILED(hr)) { throw Lunar::LunarException(hr, #x, __FILE__, __LINE__); } }
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