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
};
	
} // namespace Lunar