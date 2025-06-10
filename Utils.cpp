#include "Utils.h"
#include <comdef.h>

std::string Lunar::LunarException::ToString() const
{
	// Get error message as wstring
	std::wstring wmsg = _com_error(m_hr).ErrorMessage();
    
	// Convert to string using proper Windows API
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wmsg.c_str(), -1, NULL, 0, NULL, NULL);
	std::string msg(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wmsg.c_str(), -1, &msg[0], size_needed, NULL, NULL);
    
	return "[Error] " + m_function + " " + m_file + " " + std::to_string(m_line) + " error: " + msg;
}

