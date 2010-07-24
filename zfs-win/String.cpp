#include "stdafx.h"
#include "String.h"

namespace Util
{
	std::wstring Format(const wchar_t* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);

		wchar_t* buff = NULL;

		std::wstring str;

		int len = _vscwprintf(fmt, args) + 1;

		if(len > 0)
		{
			buff = new wchar_t[len];

			vswprintf_s(buff, len, fmt, args);

			str = std::wstring(buff, len - 1);
		}

		va_end(args);

		delete [] buff;

		return str;
	}

	std::string Format(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);

		char* buff = NULL;

		std::string str;

		int len = _vscprintf(fmt, args) + 1;

		if(len > 0)
		{
			buff = new char[len];

			vsprintf_s(buff, len, fmt, args);

			str = std::string(buff, len - 1);
		}

		va_end(args);

		delete [] buff;

		return str;
	}

	std::wstring TrimLeft(const std::wstring& s, LPCWSTR chrs)
	{
		std::wstring::size_type i = s.find_first_not_of(chrs);

		return i != std::wstring::npos ? s.substr(i) : std::wstring();
	}

	std::wstring TrimRight(const std::wstring& s, LPCWSTR chrs)
	{
		std::wstring::size_type i = s.find_last_not_of(chrs);

		return i != std::wstring::npos ? s.substr(0, i + 1) : s;
	}

	std::wstring Trim(const std::wstring& s, LPCWSTR chrs)
	{
		return TrimLeft(TrimRight(s, chrs), chrs);
	}

	std::string TrimLeft(const std::string& s, LPCSTR chrs)
	{
		std::string::size_type i = s.find_first_not_of(chrs);

		return i != std::string::npos ? s.substr(i) : std::string();
	}

	std::string TrimRight(const std::string& s, LPCSTR chrs)
	{
		std::string::size_type i = s.find_last_not_of(chrs);

		return i != std::string::npos ? s.substr(0, i + 1) : s;
	}

	std::string Trim(const std::string& s, LPCSTR chrs)
	{
		return TrimLeft(TrimRight(s, chrs), chrs);
	}

	std::wstring MakeUpper(const std::wstring& s)
	{
		std::wstring str = s;

		if(!str.empty()) _wcsupr(&str[0]);

		return str;
	}

	std::wstring MakeLower(const std::wstring& s)
	{
		std::wstring str = s;

		if(!str.empty()) _wcslwr(&str[0]);

		return str;
	}

	std::string MakeUpper(const std::string& s)
	{
		std::string str = s;

		if(!str.empty()) strupr(&str[0]);

		return str;
	}

	std::string MakeLower(const std::string& s)
	{
		std::string str = s;

		if(!str.empty()) strlwr(&str[0]);

		return str;
	}

	std::wstring UTF8To16(LPCSTR s)
	{
		std::wstring ret;

		int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0) - 1;

		if(n >= 0)
		{
			wchar_t* buff = new wchar_t[n + 1];

			n = MultiByteToWideChar(CP_UTF8, 0, s, -1, buff, n + 1);

			if(n > 0)
			{
				ret = std::wstring(buff);
			}

			delete [] buff;
		}

		return ret;
	}

	std::string UTF16To8(LPCWSTR s)
	{
		std::string ret;

		int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL) - 1;

		if(n >= 0)
		{
			char* buff = new char[n + 1];

			n = WideCharToMultiByte(CP_UTF8, 0, s, -1, buff, n + 1, NULL, NULL);

			if(n > 0)
			{
				ret = std::string(buff);
			}

			delete [] buff;
		}

		return ret;
	}

	DWORD CharSetToCodePage(DWORD charset)
	{
		if(charset == CP_UTF8) return CP_UTF8;
		if(charset == CP_UTF7) return CP_UTF7;

		CHARSETINFO cs = {0};
		
		TranslateCharsetInfo((DWORD*)charset, &cs, TCI_SRCCHARSET);

		return cs.ciACP;
	}

	std::string ConvertMBCS(const std::string& s, DWORD src, DWORD dst)
	{
		wchar_t* utf16 = new wchar_t[s.size() + 1];

		memset(utf16, 0, (s.size() + 1) * sizeof(wchar_t));

		char* mbcs = new char[s.size() * 6 + 1];

		memset(mbcs, 0, s.size() * 6 + 1);

		int len = MultiByteToWideChar(CharSetToCodePage(src), 0, s.c_str(), s.size(), utf16, (s.size() + 1) * sizeof(wchar_t));

		WideCharToMultiByte(CharSetToCodePage(dst), 0, utf16, len, mbcs, s.size() * 6, NULL, NULL);

		std::string res = mbcs;

		delete [] utf16;
		delete [] mbcs;

		return res;
	}

	std::wstring ConvertMBCS(const std::string& s, DWORD src)
	{
		return UTF8To16(ConvertMBCS(s, src, CP_UTF8).c_str());
	}

	std::wstring CombinePath(LPCWSTR dir, LPCWSTR fn)
	{
		wchar_t buff[MAX_PATH];

		PathCombine(buff, dir, fn);

		return std::wstring(buff);
	}

	std::wstring RemoveFileSpec(LPCWSTR path)
	{
		WCHAR buff[MAX_PATH];

		wcscpy(buff, path);

		PathRemoveFileSpec(buff);

		return std::wstring(buff);
	}

	std::wstring RemoveFileExt(LPCWSTR path)
	{
		WCHAR buff[MAX_PATH];

		wcscpy(buff, path);

		PathRemoveExtension(buff);

		return std::wstring(buff);
	}
}