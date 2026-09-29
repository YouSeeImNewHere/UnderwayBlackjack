# Building Blackjack Variants

Every version is built from the same C++ source. GitHub Actions builds all of
them for you, so you don't need a Mac or Android Studio just to get a copy.

| Platform | How | Workflow |
| --- | --- | --- |
| iPhone/iPad (TestFlight) | Actions → **iOS TestFlight** → Run workflow | `.github/workflows/testflight.yml` (setup: `ios/README.md`) |
| Windows | Automatic on every push | `.github/workflows/windows.yml` |
| Android | Automatic on every push | `.github/workflows/android.yml` |
| Web | `sh web/build.sh` | — |

## Windows

### Download a build

1. Open <https://github.com/YouSeeImNewHere/UnderwayBlackjack/actions> and
   click **Windows build** in the left sidebar.
2. Click the newest run with a green check.
3. Scroll to **Artifacts** and click **UnderwayBlackjack-windows** to download
   a zip.
4. Unzip it anywhere and double-click `UnderwayBlackjack.exe`. Keep the four
   `.png` files in the same folder as the `.exe`.

The first time, Windows SmartScreen may say "Windows protected your PC",
because the `.exe` isn't code-signed. Click **More info**, then **Run anyway**.

The `.exe` is fully standalone: the Visual C++ runtime and SDL are built into it.

### Publish a download page

See [Releases](#releases-a-download-page-for-everyone) below. A release
gets the Windows zip and the Android APK attached automatically.

### Build it yourself

Day-to-day development still works from `UnderwayBlackjack.sln` in Visual
Studio. To build the same standalone `.exe` the workflow makes:

```
cmake -S . -B build-win
cmake --build build-win --config Release
```

The result is `build-win\Release\UnderwayBlackjack.exe`, with the PNGs copied
next to it.

## Android

The Android app lives in `android/`. It's SDL's own Android template: its Java
activity code in `android/app/src/main/java/org/libsdl/app` is copied
unchanged, and the game's C++ comes from the root `CMakeLists.txt`.

### Install it on your phone (no Play Store)

1. Open <https://github.com/YouSeeImNewHere/UnderwayBlackjack/actions> and
   click **Android build**, then the newest green run.
2. Under **Artifacts**, download **UnderwayBlackjack-android-apk**. It's a
   zip containing `app-debug.apk`.
3. Get `app-debug.apk` onto the phone, for example by emailing it to yourself
   or putting it in Google Drive, then tap it on the phone.
4. Android asks to allow installing apps from that source (Chrome, Drive,
   Files…). Allow it, then tap **Install**.

Anyone can install that APK the same way.

### Build it yourself

Install [Android Studio](https://developer.android.com/studio), choose
**Open**, and pick the `android` folder. Android Studio downloads the right
NDK and CMake on the first build. Press the green **Run** button with a phone
plugged in (USB debugging on) or an emulator running.

From a terminal it's `./gradlew assembleDebug` inside `android/` (on Windows,
`gradlew.bat assembleDebug`).

### Publishing on Google Play

Google Play takes a signed **App Bundle** (`.aab`), not the debug APK.

1. **Make a Google Play developer account** at
   <https://play.google.com/console/signup>. It's a one-time $25 fee.
2. **Create an upload key.** You only do this once, and you must keep the
   file and passwords safe: every future update has to be signed with the
   same key. With Android Studio installed, run this in a terminal:
   ```
   keytool -genkeypair -v -keystore upload.jks -alias upload -keyalg RSA -keysize 2048 -validity 10000
   ```
   It asks for a password and some name fields.
3. **Add four repository secrets** at
   <https://github.com/YouSeeImNewHere/UnderwayBlackjack/settings/secrets/actions>:

   | Name | Value |
   | --- | --- |
   | `ANDROID_KEYSTORE_BASE64` | The keystore file as base64. Git Bash: `base64 -w0 upload.jks`. PowerShell: `[Convert]::ToBase64String([IO.File]::ReadAllBytes("upload.jks"))` |
   | `ANDROID_KEYSTORE_PASSWORD` | The keystore password |
   | `ANDROID_KEY_ALIAS` | `upload` |
   | `ANDROID_KEY_PASSWORD` | The key password. If keytool didn't ask separately, it's the same as the keystore password |

4. **Get the bundle.** The next **Android build** run also produces
   **UnderwayBlackjack-android-aab**. Download it and unzip `app-release.aab`.
5. **Upload it.** In Play Console: **Create app**, then **Testing → Internal
   testing → Create new release**, and upload `app-release.aab`. Internal
   testing is Google Play's version of TestFlight: add testers by email and
   send them the opt-in link.

The version code goes up automatically with each workflow run. Change
`versionName` in `android/app/build.gradle` for a new user-facing version.

## Releases: a download page for everyone

A GitHub Release is a page like
<https://github.com/YouSeeImNewHere/UnderwayBlackjack/releases/latest> where
anyone can download the game without an account. Creating one runs the
Windows and Android workflows, which attach:

- `UnderwayBlackjack-windows.zip`: unzip it and run `UnderwayBlackjack.exe`
- `UnderwayBlackjack-android.apk`: open it on an Android phone to install

To publish one:

1. Make sure the code you want is merged into `master`.
2. Go to <https://github.com/YouSeeImNewHere/UnderwayBlackjack/releases/new>.
3. Click **Choose a tag**, type a new version like `v1.0`, and click
   **Create new tag: v1.0 on publish**. It must start with `v`. **Target**
   stays `master`.
4. Give it a title such as `Blackjack Variants 1.0` and, optionally, a few
   lines on what's new.
5. Click **Publish release**.
6. Wait about 5 minutes and refresh the release page. The two files appear
   under **Assets** once the Windows and Android builds finish. You can watch
   them on the Actions tab.

Share the link <https://github.com/YouSeeImNewHere/UnderwayBlackjack/releases/latest>.
It always points to the newest release. For the next version, repeat with
`v1.1`, `v1.2` and so on.

If the repository is private, only people with access to it can see
releases. To let anyone download, make it public under **Settings →
General → Danger Zone → Change visibility**. That also makes the source
code public.
