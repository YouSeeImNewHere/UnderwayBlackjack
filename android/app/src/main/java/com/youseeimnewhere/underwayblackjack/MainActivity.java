package com.youseeimnewhere.underwayblackjack;

import org.libsdl.app.SDLActivity;

// The app's entry point: SDL's activity does all the work. SDL and the
// game are linked into a single libmain.so (see CMakeLists.txt), so there
// is no separate libSDL3.so to load first.
public class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }
}
