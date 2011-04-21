// upd2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

using namespace std;

void foo(string name, const char* base, size_t length, const char* ptr)
{
	printf("%s == %p offset == %p, max == %p\n", name.c_str(), ptr, ptr ? ptr - base : NULL, (base + length) - ptr);
}


int wmain(int argc, wchar_t* argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "missing upd2 filename");
		return E_INVALIDARG;
	}

	HRESULT hr;
	CAtlFile file;
	hr = file.Create(
		argv[1],
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_DELETE,
		OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN);
	if (FAILED(hr))
	{
		fprintf(stderr, "failed to open source file %S, hr = %x\n", argv[1], hr);
		return hr;
	}

	CAtlFileMapping<> mapping;
	hr = mapping.MapFile(file);
	if (FAILED(hr))
	{
		fprintf(stderr, "failed to mmap source file %S, hr = %x\n", argv[1], hr);
		return hr;
	}

	const char* data = static_cast<const char*>(mapping.GetData());
	const size_t size = mapping.GetMappingSize();

	const BYTE hdr_magic[] = { 0x49, 0x7F, 0x16, 0x35 };
	if (data == NULL || size < sizeof(hdr_magic))
	{
		fprintf(stderr, "source file %S mapped to invalid ptr == %p, size == %u", argv[1], data, size);
		return E_FAIL;
	}

	if (memcmp(hdr_magic, data, sizeof(hdr_magic) != 0)
	{
		fprintf(stderr, "incorrect magic value\n", argv[1], data, size);
		return E_FAIL;
	}

	const char* cramfs_begin = NULL;
	const char* kernel_begin = NULL;

	const BYTE cramfs_header[] = { 0x45, 0x3D, 0xCD, 0x28, };
	const BYTE kernel_header[] = { 0x1f, 0x8b, };

	vector<const char*> start;
	vector<const char*> end;
	map<const char*, wstring> names;

	start.push_back(data);
	end.push_back(&data[size]);

	for (size_t i = 0; i < size - 3; ++i)
	{
		if (cramfs_begin == NULL && memcmp(&data[i], cramfs_header, sizeof(cramfs_header)) == 0)
		{
			cramfs_begin = &data[i];
			start.push_back(&data[i]);
			end.push_back(&data[i]);
			names[&data[i]] = L"cramfs";
		}
		else if (kernel_begin == NULL && memcmp(&data[i], kernel_header, sizeof(kernel_header)) == 0)
		{
			kernel_begin = &data[i];
			start.push_back(&data[i]);
			end.push_back(&data[i]);
			names[&data[i]] = L"kernel.gz";
		}
	}

	sort(start.begin(), start.end());
	sort(end.begin(), end.end());

	if (start.size() != end.size())
	{
		fputs("size mismatch in internal arrays\n", stderr);
	}

	for (size_t i = 1; i < start.size(); ++i)
	{
		if (names.find(start[i]) == names.end())
		{
			continue;
		}

		WCHAR path_buff[MAX_PATH+1];
		wcscpy_s(path_buff, argv[1]);

		PathRemoveExtensionW(path_buff);
		wstring path = path_buff + wstring(L".") + names[start[i]];

		const size_t toWrite = end[i] - start[i];
		printf("writing %S\n\tbytes = %u\n\tsource range = %p -> %p\n",
			path.c_str(),
			toWrite,
			start[i],
			end[i]);

		if (toWrite > size)
		{
			fprintf(stderr, "section size for %S larger than expected, %u > %u\n", path.c_str(), toWrite, size);
			continue;
		}

		CAtlFile target;
		hr = target.Create(
			path.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			0,
			CREATE_NEW);
		if (FAILED(hr))
		{
			fprintf(stderr, "failed to open destination file %S, hr = %x\n", path.c_str(), hr);
			return hr;
		}

		hr = target.Write(start[i], static_cast<DWORD>(toWrite));
		if (FAILED(hr))
		{
			fprintf(stderr, "failed to write to destination file %S, hr = %x\n", path.c_str(), hr);
			return hr;
		}
	}

	return 0;
}

