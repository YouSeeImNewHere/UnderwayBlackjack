#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

// The Windows game has no store to update it, so on launch it asks GitHub
// for the latest release (in the background, a few seconds at most) and
// the main menu offers an UPDATE button when that's newer than this build.
// The button opens the release page; the player downloads the new zip
// from there.
//
// GAME_VERSION_STRING comes from CMake's GAME_VERSION, which the Windows
// workflow sets from the release tag (v1.1 -> "1.1"). Anything else --
// Visual Studio builds, CI builds of ordinary pushes -- is "dev" and never
// checks, so development copies don't nag.
#ifndef GAME_VERSION_STRING
#define GAME_VERSION_STRING "dev"
#endif

class UpdateCheck
{
public:
	static constexpr const char* RELEASES_PAGE = "https://github.com/YouSeeImNewHere/UnderwayBlackjack/releases/latest";

	static const char* currentVersion(){ return GAME_VERSION_STRING; }
	static bool isDevBuild(){ return std::string(GAME_VERSION_STRING) == "dev"; }

	void start(){
#ifdef _WIN32
		if(isDevBuild() || started)
			return;
		started = true;
		// The thread holds its own reference to the result, so quitting
		// while it's still waiting on the network is harmless.
		std::shared_ptr<Result> r = result;
		std::thread([r](){
			std::string tag = parseTagName(fetchLatestReleaseJson());
			if(!tag.empty() && isNewer(tag, currentVersion())){
				std::lock_guard<std::mutex> lock(r->mutex);
				r->latest = stripV(tag);
				r->available = true;
			}
		}).detach();
#endif
	}

	bool updateAvailable() const{ return result->available; }

	std::string latestVersion(){
		std::lock_guard<std::mutex> lock(result->mutex);
		return result->latest;
	}

	// "v1.10" vs "1.9": numeric, part by part, missing parts count as 0.
	static bool isNewer(const std::string& candidate, const std::string& current){
		std::vector<int> a = parts(stripV(candidate)), b = parts(stripV(current));
		if(a.empty() || b.empty())
			return false;
		size_t n = std::max(a.size(), b.size());
		for(size_t i = 0; i < n; i++){
			int x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
			if(x != y)
				return x > y;
		}
		return false;
	}

	// Pulls "tag_name":"v1.1" out of GitHub's release JSON without a JSON
	// library -- it's the only field needed.
	static std::string parseTagName(const std::string& json){
		size_t k = json.find("\"tag_name\"");
		if(k == std::string::npos)
			return "";
		size_t colon = json.find(':', k);
		size_t open = colon == std::string::npos ? std::string::npos : json.find('"', colon);
		size_t close = open == std::string::npos ? std::string::npos : json.find('"', open + 1);
		if(close == std::string::npos)
			return "";
		return json.substr(open + 1, close - open - 1);
	}

private:
	struct Result{
		std::atomic<bool> available{false};
		std::mutex mutex;
		std::string latest;
	};
	std::shared_ptr<Result> result = std::make_shared<Result>();
	bool started = false;

	static std::string stripV(const std::string& s){
		return (!s.empty() && (s[0] == 'v' || s[0] == 'V')) ? s.substr(1) : s;
	}

	static std::vector<int> parts(const std::string& s){
		std::vector<int> out;
		int value = 0;
		bool any = false;
		for(char c : s){
			if(c >= '0' && c <= '9'){
				value = value * 10 + (c - '0');
				any = true;
			} else if(c == '.'){
				if(!any) return {};
				out.push_back(value);
				value = 0;
				any = false;
			} else{
				break; // "1.1-beta": stop at the suffix
			}
		}
		if(any)
			out.push_back(value);
		return out;
	}

#ifdef _WIN32
	static std::string fetchLatestReleaseJson(){
		std::string body;
		HINTERNET session = WinHttpOpen(L"BlackjackVariants-UpdateCheck", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if(!session)
			return body;
		WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);

		HINTERNET connect = WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
		HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET",
			L"/repos/YouSeeImNewHere/UnderwayBlackjack/releases/latest",
			nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;

		if(request
			&& WinHttpSendRequest(request, L"Accept: application/vnd.github+json\r\n", (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
			&& WinHttpReceiveResponse(request, nullptr)){
			DWORD status = 0, size = sizeof(status);
			WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
			if(status == 200){
				DWORD available = 0;
				while(WinHttpQueryDataAvailable(request, &available) && available > 0 && body.size() < 512 * 1024){
					std::string chunk(available, '\0');
					DWORD read = 0;
					if(!WinHttpReadData(request, chunk.data(), available, &read) || read == 0)
						break;
					body.append(chunk.data(), read);
				}
			}
		}

		if(request) WinHttpCloseHandle(request);
		if(connect) WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return body;
	}
#endif
};
