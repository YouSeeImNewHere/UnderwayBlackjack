#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <algorithm>
#include <filesystem>
#include <fstream>

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
// The button installs the update itself (installUpdate()): it downloads the
// release's UnderwayBlackjack-windows.zip, unzips it with Windows' own
// tar.exe (PowerShell as a fallback), copies the files over this folder --
// the running .exe can't be overwritten but can be renamed, so it's moved
// aside to UnderwayBlackjack.exe.old, deleted on the next launch -- and
// mina.cpp then starts the new .exe and quits. If anything fails (no
// internet, a folder it can't write to such as Program Files), it opens
// the release page instead so the player can update by hand.
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
			removeLeftovers();
			std::string json;
			httpGet(L"https://api.github.com/repos/YouSeeImNewHere/UnderwayBlackjack/releases/latest", json, 512 * 1024);
			std::string tag = parseTagName(json);
			// A release shows up a few minutes before the workflow attaches
			// its zip; until then there's nothing to install, so no button.
			std::string zipUrl = parseAssetUrl(json, ZIP_NAME);
			if(!tag.empty() && !zipUrl.empty() && isNewer(tag, currentVersion())){
				std::lock_guard<std::mutex> lock(r->mutex);
				r->latest = stripV(tag);
				r->zipUrl = zipUrl;
				r->available = true;
			}
		}).detach();
#endif
	}

	bool updateAvailable() const{ return result->available; }

	enum class Install{ Idle, Working, Done, Failed };

	// Starts downloading and installing the update in the background.
	// Returns false where that isn't possible (not Windows, no update
	// found); the caller opens the release page instead.
	bool installUpdate(){
#ifdef _WIN32
		if(!result->available || result->install != (int)Install::Idle)
			return false;
		result->install = (int)Install::Working;
		std::shared_ptr<Result> r = result;
		std::thread([r](){
			std::string url;
			{
				std::lock_guard<std::mutex> lock(r->mutex);
				url = r->zipUrl;
			}
			std::filesystem::path newExe;
			bool ok = !url.empty() && downloadAndInstall(url, newExe);
			{
				std::lock_guard<std::mutex> lock(r->mutex);
				r->newExe = newExe;
			}
			r->install = ok ? (int)Install::Done : (int)Install::Failed;
		}).detach();
		return true;
#else
		return false;
#endif
	}

	Install installState() const{ return (Install)result->install.load(); }

	// Once the caller has handled Done or Failed: back to Idle, so the
	// button can be pressed again.
	void acknowledgeInstall(){
		if(result->install != (int)Install::Working)
			result->install = (int)Install::Idle;
	}

	// Once installState() is Done: start the new copy. The caller quits
	// right after. Returns false if it couldn't be started.
	bool launchNewVersion(){
#ifdef _WIN32
		std::filesystem::path exe;
		{
			std::lock_guard<std::mutex> lock(result->mutex);
			exe = result->newExe;
		}
		return runProcess(L"\"" + exe.wstring() + L"\"", exe.parent_path(), false);
#else
		return false;
#endif
	}

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
	// library -- it and the zip's download link are the only fields needed.
	static std::string parseTagName(const std::string& json){
		return stringAfter(json, json.find("\"tag_name\""));
	}

	// The browser_download_url of the release asset named `file`.
	static std::string parseAssetUrl(const std::string& json, const std::string& file){
		const std::string key = "\"browser_download_url\"";
		for(size_t k = json.find(key); k != std::string::npos; k = json.find(key, k + 1)){
			std::string url = stringAfter(json, k);
			if(url.size() >= file.size() + 1 && url.compare(url.size() - file.size() - 1, std::string::npos, "/" + file) == 0)
				return url;
		}
		return "";
	}

private:
	static constexpr const char* ZIP_NAME = "UnderwayBlackjack-windows.zip";
	static constexpr const wchar_t* EXE_NAME = L"UnderwayBlackjack.exe";

	struct Result{
		std::atomic<bool> available{false};
		std::atomic<int> install{(int)Install::Idle};
		std::mutex mutex;
		std::string latest;
		std::string zipUrl;
		std::filesystem::path newExe;
	};
	std::shared_ptr<Result> result = std::make_shared<Result>();
	bool started = false;

	// The "..." string value of the JSON key found at k.
	static std::string stringAfter(const std::string& json, size_t k){
		if(k == std::string::npos)
			return "";
		size_t colon = json.find(':', k);
		size_t open = colon == std::string::npos ? std::string::npos : json.find('"', colon);
		size_t close = open == std::string::npos ? std::string::npos : json.find('"', open + 1);
		if(close == std::string::npos)
			return "";
		return json.substr(open + 1, close - open - 1);
	}

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
	// GETs an https URL into body (up to limit bytes), following GitHub's
	// redirect from a release asset to its download server. True on a 200.
	static bool httpGet(const std::wstring& url, std::string& body, size_t limit){
		body.clear();
		URL_COMPONENTS parts{};
		parts.dwStructSize = sizeof(parts);
		wchar_t host[256], path[2048];
		parts.lpszHostName = host;
		parts.dwHostNameLength = 256;
		parts.lpszUrlPath = path;
		parts.dwUrlPathLength = 2048;
		if(!WinHttpCrackUrl(url.c_str(), 0, 0, &parts))
			return false;

		HINTERNET session = WinHttpOpen(L"BlackjackVariants-Updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if(!session)
			return false;
		WinHttpSetTimeouts(session, 5000, 10000, 10000, 30000);

		HINTERNET connect = WinHttpConnect(session, host, parts.nPort, 0);
		HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path, nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;

		bool ok = false;
		if(request
			&& WinHttpSendRequest(request, L"Accept: application/vnd.github+json, application/octet-stream\r\n", (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
			&& WinHttpReceiveResponse(request, nullptr)){
			DWORD status = 0, size = sizeof(status);
			WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
			if(status == 200){
				ok = true;
				DWORD available = 0;
				while(WinHttpQueryDataAvailable(request, &available) && available > 0){
					if(body.size() + available > limit){
						ok = false;
						break;
					}
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
		return ok && !body.empty();
	}

	static std::filesystem::path runningExe(){
		wchar_t buf[MAX_PATH * 4];
		DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
		return std::filesystem::path(std::wstring(buf, n));
	}

	// Runs a command line with no console window. wait: block until it
	// exits and report whether it exited with 0.
	static bool runProcess(std::wstring commandLine, const std::filesystem::path& dir, bool wait){
		STARTUPINFOW si{};
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi{};
		std::wstring cwd = dir.wstring();
		if(!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
				nullptr, cwd.empty() ? nullptr : cwd.c_str(), &si, &pi))
			return false;
		bool ok = true;
		if(wait){
			WaitForSingleObject(pi.hProcess, 120000);
			DWORD code = 1;
			ok = GetExitCodeProcess(pi.hProcess, &code) && code == 0;
		}
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return ok;
	}

	static std::wstring quoted(const std::filesystem::path& p){ return L"\"" + p.wstring() + L"\""; }

	// The .exe the last update moved aside, and any update download.
	static void removeLeftovers(){
		std::error_code ec;
		std::filesystem::path exe = runningExe();
		for(const auto& entry : std::filesystem::directory_iterator(exe.parent_path(), ec))
			if(entry.path().extension() == L".old")
				std::filesystem::remove(entry.path(), ec);
		std::filesystem::remove_all(std::filesystem::temp_directory_path(ec) / L"BlackjackVariantsUpdate", ec);
	}

	static bool downloadAndInstall(const std::string& url, std::filesystem::path& newExe){
		namespace fs = std::filesystem;
		std::error_code ec;
		fs::path work = fs::temp_directory_path(ec) / L"BlackjackVariantsUpdate";
		if(ec)
			return false;
		fs::remove_all(work, ec);
		fs::create_directories(work / L"unzipped", ec);
		if(ec)
			return false;

		std::string zip;
		if(!httpGet(std::wstring(url.begin(), url.end()), zip, 200u * 1024 * 1024))
			return false;
		fs::path zipPath = work / L"update.zip";
		{
			std::ofstream out(zipPath, std::ios::binary);
			out.write(zip.data(), (std::streamsize)zip.size());
			if(!out)
				return false;
		}

		// tar.exe ships with Windows 10 (1803+) and 11 and unzips .zip files.
		fs::path unzipped = work / L"unzipped";
		wchar_t sysDir[MAX_PATH];
		UINT n = GetSystemDirectoryW(sysDir, MAX_PATH);
		fs::path tar = fs::path(std::wstring(sysDir, n)) / L"tar.exe";
		bool extracted = runProcess(quoted(tar) + L" -xf " + quoted(zipPath) + L" -C " + quoted(unzipped), work, true);
		if(!extracted)
			extracted = runProcess(L"powershell.exe -NoProfile -NonInteractive -Command \"Expand-Archive -LiteralPath '"
				+ zipPath.wstring() + L"' -DestinationPath '" + unzipped.wstring() + L"' -Force\"", work, true);
		if(!extracted)
			return false;

		// The zip holds an UnderwayBlackjack folder; find the .exe wherever it is.
		fs::path source;
		for(const auto& entry : fs::recursive_directory_iterator(unzipped, ec))
			if(entry.is_regular_file() && entry.path().filename() == EXE_NAME){
				source = entry.path().parent_path();
				break;
			}
		if(source.empty())
			return false;

		fs::path current = runningExe();
		fs::path dir = current.parent_path();
		std::vector<fs::path> movedAside;
		auto rollBack = [&](){
			for(const fs::path& p : movedAside){
				fs::path original = p;
				original.replace_extension();
				fs::remove(original, ec);
				fs::rename(p, original, ec);
			}
		};

		for(const auto& entry : fs::directory_iterator(source, ec)){
			if(!entry.is_regular_file())
				continue;
			fs::path target = dir / entry.path().filename();
			// A running .exe can be renamed but not overwritten.
			if(target.extension() == L".exe" && fs::exists(target)){
				fs::path aside = target;
				aside += L".old";
				fs::remove(aside, ec);
				fs::rename(target, aside, ec);
				if(ec){
					rollBack();
					return false;
				}
				movedAside.push_back(aside);
			}
			fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, ec);
			if(ec){
				rollBack();
				return false;
			}
		}

		newExe = dir / EXE_NAME;
		return fs::exists(newExe);
	}
#endif
};
