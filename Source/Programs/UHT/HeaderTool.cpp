#include "HeaderTool.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

// 입력 해석과 헤더 탐색의 순차 실행
int FHeaderTool::Run(int ArgumentCount, wchar_t* Arguments[])
{
	if (ArgumentCount == 2 && std::wstring_view(Arguments[1]) == L"--help")
	{
		std::cout << "Usage: UHT.exe --root <ProjectRoot>\n";
		return 0;
	}

	ParseArguments(ArgumentCount, Arguments);
	DiscoverHeaders();

	// 한글 경로를 포함한 입력 목록의 UTF-8 출력
	const std::u8string RootText = ProjectRoot.generic_u8string();
	std::cout << "Project: " << std::string(RootText.begin(), RootText.end()) << '\n';
	for (const std::filesystem::path& HeaderFile : HeaderFiles)
	{
		const std::u8string HeaderText = HeaderFile.generic_u8string();
		std::cout << std::string(HeaderText.begin(), HeaderText.end()) << '\n';
	}
	std::cout << "Discovered " << HeaderFiles.size() << " header(s).\n";
	return 0;
}

// 고정된 명령 형식과 프로젝트 루트의 확정
void FHeaderTool::ParseArguments(int ArgumentCount, wchar_t* Arguments[])
{
	if (ArgumentCount != 3 || std::wstring_view(Arguments[1]) != L"--root" || std::wstring_view(Arguments[2]).empty())
	{
		throw std::invalid_argument("Usage: UHT.exe --root <ProjectRoot>");
	}

	// 상대 경로와 중간 경로 요소의 정규화
	ProjectRoot = std::filesystem::canonical(Arguments[2]);
}

// 두 소스 영역의 원본 헤더 수집과 순서 고정
void FHeaderTool::DiscoverHeaders()
{
	HeaderFiles.clear();
	for (const wchar_t* SourceDirectory : { L"Source/Runtime", L"Source/Editor" })
	{
		const std::filesystem::path SearchRoot = ProjectRoot / SourceDirectory;
		for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(SearchRoot))
		{
			if (!Entry.is_regular_file())
			{
				continue;
			}

			const std::filesystem::path& HeaderPath = Entry.path();
			if (HeaderPath.extension() != L".h" || HeaderPath.filename().wstring().ends_with(L".generated.h"))
			{
				continue;
			}

			// 후속 분석과 출력 식별에 사용할 루트 기준 경로
			HeaderFiles.push_back(HeaderPath.lexically_relative(ProjectRoot));
		}
	}

	// 파일 시스템의 열거 순서와 독립적인 처리 순서
	std::sort(HeaderFiles.begin(), HeaderFiles.end());
}