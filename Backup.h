#pragma once
#include <SDL3/SDL.h>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// BACKUP (BackupMenu.h): everything the game keeps -- the saved game and
// settings (SaveData, save.txt), stats (stats.txt), and training progress
// with the session log (trainer.txt) -- in one text file the player can
// keep anywhere (a USB stick, a cloud drive, email) and load back after a
// reset or on a new phone.
//
// File format, so it survives being opened in a text editor:
//   BLACKJACK VARIANTS BACKUP 1
//   FILE save.txt 123        <- name and byte length, then exactly that
//   ...123 bytes...             many bytes and a newline
//   FILE stats.txt 456
//   ...
//   END
// The web build stores localStorage entries the same way, named by key.
//
// Where the file goes:
//   Android / Windows / Mac / Linux: SDL's own file picker (the system
//     save/open dialog; on Android that includes USB sticks and Drive).
//   iPhone / iPad: the share sheet to save it (Save to Files, AirDrop,
//     Mail...) and the Files picker to load it (ios/Backup.m).
//   Web: a download, and an upload that reloads the page.
#if defined(SDL_PLATFORM_IOS)
extern "C" void UB_ShareFile(const char* path);
extern "C" void UB_PickBackupFile(void);
// 1 = a file was picked (its path copied into buf), -1 = cancelled,
// 0 = nothing yet.
extern "C" int UB_TakePickedFile(char* buf, int size);
#endif

namespace Backup{

inline const char* HEADER = "BLACKJACK VARIANTS BACKUP 1";
inline const char* FILE_NAME = "BlackjackBackup.txt";

using Entries = std::vector<std::pair<std::string, std::string>>;

inline std::string serialize(const Entries& entries){
	std::string out = std::string(HEADER) + "\n";
	for(const auto& [name, data] : entries)
		out += "FILE " + name + " " + std::to_string(data.size()) + "\n" + data + "\n";
	out += "END\n";
	return out;
}

// False (with a reason) for anything that isn't a whole, valid backup --
// nothing is replaced unless the entire file reads correctly.
inline bool parse(const std::string& text, Entries& out, std::string& error){
	out.clear();
	size_t pos = 0;
	auto line = [&](std::string& l) -> bool {
		size_t nl = text.find('\n', pos);
		if(nl == std::string::npos)
			return false;
		l = text.substr(pos, nl - pos);
		if(!l.empty() && l.back() == '\r')
			l.pop_back();
		pos = nl + 1;
		return true;
	};
	std::string l;
	if(!line(l) || l != HEADER){
		error = "NOT A BLACKJACK VARIANTS BACKUP";
		return false;
	}
	while(line(l)){
		if(l == "END")
			return true;
		if(l.rfind("FILE ", 0) != 0){
			error = "THE BACKUP FILE IS DAMAGED";
			return false;
		}
		size_t space = l.rfind(' ');
		std::string name = l.substr(5, space - 5);
		size_t length = 0;
		try{ length = std::stoul(l.substr(space + 1)); } catch(...){ error = "THE BACKUP FILE IS DAMAGED"; return false; }
		if(name.empty() || space <= 5 || pos + length > text.size()){
			error = "THE BACKUP FILE IS CUT SHORT";
			return false;
		}
		out.emplace_back(name, text.substr(pos, length));
		pos += length;
		if(pos < text.size() && text[pos] == '\r')
			pos++;
		if(pos < text.size() && text[pos] == '\n')
			pos++;
	}
	error = "THE BACKUP FILE IS CUT SHORT";
	return false;
}

inline bool writeFile(const std::string& path, const std::string& data){
	SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "wb"); // content:// URIs too, on Android
	if(!io)
		return false;
	bool ok = data.empty() || SDL_WriteIO(io, data.data(), data.size()) == data.size();
	return SDL_CloseIO(io) && ok;
}

inline bool readFile(const std::string& path, std::string& data){
	size_t size = 0;
	void* bytes = SDL_LoadFile(path.c_str(), &size);
	if(!bytes)
		return false;
	data.assign(static_cast<const char*>(bytes), size);
	SDL_free(bytes);
	return true;
}

#ifndef __EMSCRIPTEN__
// The three files every save lives in (SaveData.h, Stats.h, Trainer.h).
inline const char* FILES[3] = { "save.txt", "stats.txt", "trainer.txt" };

inline std::string prefDir(){
	char* pref = SDL_GetPrefPath("UnderwayBlackjack", "Save");
	std::string p = pref ? pref : "";
	if(pref)
		SDL_free(pref);
	return p;
}

inline std::string build(){
	Entries entries;
	for(const char* name : FILES){
		std::string data;
		readFile(prefDir() + name, data); // a file never written yet backs up empty
		entries.emplace_back(name, data);
	}
	return serialize(entries);
}

// Replaces the three files with the backup's. The caller reloads them.
inline bool restore(const std::string& text, std::string& error){
	Entries entries;
	if(!parse(text, entries, error))
		return false;
	for(const char* name : FILES){
		bool found = false;
		for(const auto& e : entries)
			found = found || e.first == name;
		if(!found){
			error = "THE BACKUP IS MISSING PART OF THE DATA";
			return false;
		}
	}
	for(const auto& [name, data] : entries){
		bool known = false;
		for(const char* f : FILES)
			known = known || name == f;
		if(known && !writeFile(prefDir() + name, data)){
			error = "COULD NOT WRITE THE GAME'S DATA";
			return false;
		}
	}
	return true;
}

// The file pickers answer asynchronously (and on Windows, from another
// thread), so their answer waits here until the game loop picks it up.
enum class Op{ None, Save, Load };
struct Pending{
	std::mutex lock;
	Op op = Op::None;
	bool ready = false;
	bool cancelled = false;
	std::string path;
	std::string text; // the backup being saved
};
inline Pending pending;

inline void SDLCALL dialogDone(void*, const char* const* files, int){
	std::lock_guard<std::mutex> guard(pending.lock);
	pending.ready = true;
	pending.cancelled = !files || !files[0];
	pending.path = pending.cancelled ? "" : files[0];
}

inline const SDL_DialogFileFilter FILTERS[1] = { { "Backup", "txt" } };

// SAVE BACKUP: ask where to put it.
inline void requestSave(SDL_Window* window){
	std::string text = build();
#if defined(SDL_PLATFORM_IOS)
	// The share sheet takes a file: write it where the app can, then offer
	// it (Save to Files, AirDrop, Mail...).
	std::string tmp = prefDir() + FILE_NAME;
	if(writeFile(tmp, text))
		UB_ShareFile(tmp.c_str());
	std::lock_guard<std::mutex> guard(pending.lock);
	pending.op = Op::Save;
	pending.ready = true;
	pending.cancelled = false;
	pending.path = ""; // already handed to the share sheet
	(void)window;
#else
	{
		std::lock_guard<std::mutex> guard(pending.lock);
		pending.op = Op::Save;
		pending.ready = false;
		pending.text = text;
	}
	SDL_ShowSaveFileDialog(dialogDone, nullptr, window, FILTERS, 1, FILE_NAME);
#endif
}

// LOAD BACKUP: ask which file.
inline void requestLoad(SDL_Window* window){
	{
		std::lock_guard<std::mutex> guard(pending.lock);
		pending.op = Op::Load;
		pending.ready = false;
	}
#if defined(SDL_PLATFORM_IOS)
	UB_PickBackupFile();
	(void)window;
#else
	SDL_ShowOpenFileDialog(dialogDone, nullptr, window, FILTERS, 1, nullptr, false);
#endif
}

// Called every frame. When a picker has answered: a save is written,
// a load is read and restored. Returns the finished operation (and sets
// ok / message), or Op::None while nothing has finished.
inline Op poll(bool& ok, std::string& message){
#if defined(SDL_PLATFORM_IOS)
	{
		std::lock_guard<std::mutex> guard(pending.lock);
		if(pending.op == Op::Load && !pending.ready){
			char buf[2048];
			int r = UB_TakePickedFile(buf, sizeof(buf));
			if(r != 0){
				pending.ready = true;
				pending.cancelled = r < 0;
				pending.path = r > 0 ? buf : "";
			}
		}
	}
#endif
	Op op;
	bool cancelled;
	std::string path, text;
	{
		std::lock_guard<std::mutex> guard(pending.lock);
		if(pending.op == Op::None || !pending.ready)
			return Op::None;
		op = pending.op;
		cancelled = pending.cancelled;
		path = pending.path;
		text = pending.text;
		pending.op = Op::None;
		pending.ready = false;
		pending.text.clear();
	}
	ok = false;
	if(cancelled){
		message = "CANCELLED";
		return op;
	}
	if(op == Op::Save){
		if(path.empty()){ // iOS: the share sheet takes it from here
			ok = true;
			message = "CHOOSE WHERE TO SAVE IT - SAVE TO FILES WORKS";
		} else if(writeFile(path, text)){
			ok = true;
			message = "BACKUP SAVED";
		} else
			message = "COULD NOT SAVE THERE - TRY ANOTHER PLACE";
		return op;
	}
	std::string data;
	if(!readFile(path, data)){
		message = "COULD NOT OPEN THAT FILE";
		return op;
	}
	ok = restore(data, message);
	if(ok)
		message = "BACKUP LOADED";
	return op;
}
#endif

}

#ifdef __EMSCRIPTEN__
// The web build: localStorage entries named underwayBlackjack..., saved as
// a download and loaded from an upload (which reloads the page).
EM_JS(void, ub_web_export, (), {
	var out = 'BLACKJACK VARIANTS BACKUP 1\n';
	for(var i = 0; i < localStorage.length; i++){
		var k = localStorage.key(i);
		if(k.indexOf('underwayBlackjack') !== 0) continue;
		var v = localStorage.getItem(k) || '';
		out += 'FILE ' + k + ' ' + v.length + '\n' + v + '\n';
	}
	out += 'END\n';
	var a = document.createElement('a');
	a.href = URL.createObjectURL(new Blob([out], { type: 'text/plain' }));
	a.download = 'BlackjackBackup.txt';
	document.body.appendChild(a);
	a.click();
	setTimeout(function(){ URL.revokeObjectURL(a.href); a.remove(); }, 1000);
});

EM_JS(void, ub_web_import, (), {
	var input = document.createElement('input');
	input.type = 'file';
	input.accept = '.txt,text/plain';
	input.onchange = function(){
		var f = input.files && input.files[0];
		if(!f) return;
		f.text().then(function(text){
			var lines = text.replace(/\r\n/g, '\n');
			var pos = 0;
			function line(){ var nl = lines.indexOf('\n', pos); if(nl < 0) return null; var l = lines.substring(pos, nl); pos = nl + 1; return l; }
			if(line() !== 'BLACKJACK VARIANTS BACKUP 1'){ alert('That is not a Blackjack Variants backup.'); return; }
			var entries = [], l;
			while((l = line()) !== null){
				if(l === 'END'){
					for(var i = localStorage.length - 1; i >= 0; i--){
						var k = localStorage.key(i);
						if(k.indexOf('underwayBlackjack') === 0) localStorage.removeItem(k);
					}
					entries.forEach(function(e){ localStorage.setItem(e[0], e[1]); });
					location.reload();
					return;
				}
				var m = /^FILE (\S+) (\d+)$/.exec(l);
				if(!m || m[1].indexOf('underwayBlackjack') !== 0){ alert('The backup file is damaged.'); return; }
				var n = parseInt(m[2]);
				entries.push([m[1], lines.substr(pos, n)]);
				pos += n + 1;
			}
			alert('The backup file is cut short.');
		});
	};
	input.click();
});
#endif
