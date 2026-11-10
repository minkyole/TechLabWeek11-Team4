#pragma once

#include <filesystem>
#include <vector>

// 헤더 도구의 실행과 입력 헤더 수집 책임
class FHeaderTool
{
public:
	// 명령행 입력을 사용하는 단일 실행 흐름
	int Run(int ArgumentCount, wchar_t* Arguments[]);

private:
	// 프로젝트 루트 인자의 해석
	void ParseArguments(int ArgumentCount, wchar_t* Arguments[]);

	// 런타임과 에디터의 원본 헤더 수집
	void DiscoverHeaders();

	// 절대 경로로 정규화한 프로젝트 루트
	std::filesystem::path ProjectRoot;

	// 프로젝트 루트 기준의 정렬된 상대 경로 목록
	std::vector<std::filesystem::path> HeaderFiles;
};