// Temporary: exercises UpdateCheck's download-and-install on a real Windows runner.
#define GAME_VERSION_STRING "1.0"
#include "UpdateCheck.h"
#include <cstdio>
int main(){
    UpdateCheck u;
    u.start();
    for(int i = 0; i < 200 && !u.updateAvailable(); i++) Sleep(100);
    printf("available=%d latest=%s\n", (int)u.updateAvailable(), u.latestVersion().c_str());
    if(!u.installUpdate()){ printf("installUpdate refused\n"); return 2; }
    while(u.installState() == UpdateCheck::Install::Working) Sleep(100);
    int st = (int)u.installState();
    printf("state=%d (2=Done 3=Failed)\n", st);
    return st == 2 ? 0 : 1;
}
