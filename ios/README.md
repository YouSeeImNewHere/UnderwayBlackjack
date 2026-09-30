# iOS / TestFlight

The iOS app is built from the same C++/SDL3 code as the Windows and web
versions (`CMakeLists.txt` at the repo root). You don't need a Mac:
GitHub Actions builds it on a hosted Mac, signs it, and uploads it to
App Store Connect (`.github/workflows/testflight.yml`).

## One-time setup

Apple's pages get small wording changes now and then; if a button isn't
exactly where described, it's nearby with a similar name.

### 1. Copy your Team ID

1. Go to <https://developer.apple.com/account> and sign in.
2. Scroll down to **Membership details**.
3. Copy the **Team ID** (10 characters, like `ABCDE12345`) somewhere handy.

### 2. Register the bundle ID

1. Go to <https://developer.apple.com/account/resources/identifiers/list>.
2. Click the blue **+** next to "Identifiers".
3. Select **App IDs** → **Continue**.
4. Select **App** → **Continue**.
5. **Description:** `Blackjack Variants`.
   **Bundle ID:** leave **Explicit** selected and enter
   `com.youseeimnewhere.underwayblackjack` exactly.
6. Leave every capability unchecked → **Continue** → **Register**.

### 3. Create the app in App Store Connect

1. Go to <https://appstoreconnect.apple.com/apps>.
2. If there's a banner at the top asking you to review or accept an
   agreement, accept it first. Uploads fail until it's accepted.
3. Click the blue **+** next to "Apps" → **New App**.
4. Fill in:
   - **Platforms:** check **iOS**
   - **Name:** `Blackjack Variants`. This has to be unique across the whole
     App Store; if it's taken, try something like `Blackjack Variants 21`.
     It's only the store name and can be changed later.
   - **Primary Language:** English (U.S.)
   - **Bundle ID:** pick `Blackjack Variants - com.youseeimnewhere.underwayblackjack`
     (it appears after step 2; refresh if not)
   - **SKU:** `underwayblackjack`
   - **User Access:** Full Access
5. Click **Create**.

### 4. Create an App Store Connect API key

1. In App Store Connect, click **Users and Access** (top menu) →
   **Integrations** tab → **App Store Connect API** in the left sidebar →
   **Team Keys** tab.
2. The first time, you'll see **Request Access**. Click it, accept the
   terms, and reload the page.
3. Click **Generate API Key** (or the **+** next to "Active").
4. **Name:** `GitHub Actions`. **Access:** **Admin**. Admin is required:
   it's what lets the workflow create the distribution certificate and
   provisioning profile for you. Click **Generate**.
5. On that page, copy:
   - **Issuer ID**: shown above the table, with a **Copy** link.
   - **Key ID**: in the new key's row (10 characters).
6. Click **Download** in the key's row. You get
   `AuthKey_<KEYID>.p8`. **Apple only lets you download it once**, so keep
   it somewhere safe. If you lose it, revoke the key and make a new one.

### 5. Add the GitHub secrets

1. Go to
   <https://github.com/YouSeeImNewHere/UnderwayBlackjack/settings/secrets/actions>.
2. For each row below, click **New repository secret**, enter the
   **Name** exactly as shown, paste the value, and click **Add secret**:

   | Name | Value |
   | --- | --- |
   | `APPLE_TEAM_ID` | Team ID from step 1 |
   | `APPSTORE_API_KEY_ID` | Key ID from step 4 |
   | `APPSTORE_API_ISSUER_ID` | Issuer ID from step 4 (looks like a UUID with dashes) |
   | `APPSTORE_API_PRIVATE_KEY` | The whole `.p8` file |

   To get the `.p8` contents on Windows, right-click the file → **Open
   with** → **Notepad**, press Ctrl+A, then Ctrl+C. Paste all of it,
   including the `-----BEGIN PRIVATE KEY-----` and
   `-----END PRIVATE KEY-----` lines.

   If you registered a different bundle ID in step 2, also open the
   **Variables** tab on that page → **New repository variable**, name it
   `IOS_BUNDLE_ID`, and set it to your bundle ID.

## Uploading a build

1. Go to <https://github.com/YouSeeImNewHere/UnderwayBlackjack/actions>.
2. Click **iOS TestFlight** in the left sidebar. (It only appears once this
   workflow file is on the `master` branch.)
3. Click **Run workflow** (right side) → leave Branch as `master` → click
   the green **Run workflow**.
4. Click the run that appears to watch it. It takes about 10–20 minutes.
   A green check means the upload succeeded.

Pushing a tag such as `ios-v1.0` also starts it.

Then, in App Store Connect → **Apps** → your app → **TestFlight** tab, the
build shows as "Processing" for about 5–30 minutes. Apple also emails you
when it's done.

### Testing it yourself (internal testing, no review)

1. In the **TestFlight** tab, click the **+** next to **Internal Testing**
   in the left sidebar.
2. Name the group (e.g. `Me`), leave **Enable automatic distribution**
   checked, and click **Create**.
3. In the group, click **+** next to **Testers**, check yourself, and click
   **Add**. Anyone you add must already be a user in Users and Access.
   Up to 100 people.
4. On your iPhone, install **TestFlight** from the App Store and sign in
   with the same Apple ID.
5. Open the invite email on the phone and tap **View in TestFlight**, or
   just open the TestFlight app. Tap **Install**.

With automatic distribution on, every later build shows up there by itself.

### Sharing with anyone (external testing, one-time review)

1. In the **TestFlight** tab, click the **+** next to **External Testing**,
   name the group (e.g. `Friends`), and click **Create**.
2. Under **Test Information** in the left sidebar, fill in **Beta App
   Description**, **Feedback Email**, and your contact info. Uncheck **Sign-in
   required**, since the app has no login.
3. Back in the group, click **+** next to **Builds**, pick the build, and
   enter "What to Test" (e.g. "Try a few hands of blackjack").
4. Click **Submit for Review**. The first review usually takes about a
   day; later builds of the same version are often approved automatically.
5. Once approved, either add testers by email (**+** next to Testers) or
   click **Enable Public Link** and share that link. Up to 10,000 testers.

### If the workflow fails

Click the failed step to see its log.

- **"Missing secret for ..."**: a secret name is misspelled or empty
  (step 5).
- **"Cloud signing permission error"**, or it can't create a certificate:
  the API key isn't **Admin** (step 4).
- **"No profiles for 'com.youseeimnewhere.underwayblackjack' were found"**:
  the bundle ID in step 2 doesn't match exactly, or `APPLE_TEAM_ID` is wrong.
- **"Cannot determine the Apple ID from Bundle ID"**, or upload errors
  about the app: the App Store Connect app (step 3) doesn't exist yet or
  uses a different bundle ID.
- **An error about an agreement**: accept it in App Store Connect
  (step 3.2) or under **Business**.
- **An error that the build number was already used**: re-run the
  workflow; each run gets a new number.

## Versions

- The build number is set automatically from the workflow run number.
- The version (shown in TestFlight and in the bottom-right corner of the
  game's main menu) is the latest GitHub release tag: after publishing
  release `v1.5`, the next TestFlight run is version 1.5. To upload a
  version without making a GitHub release, push a tag like `ios-v1.5`.

## Building locally on a Mac (optional)

```sh
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
      -DIOS_TEAM_ID=YOURTEAMID
open build-ios/UnderwayBlackjack.xcodeproj
```

Then choose your iPhone or a simulator in Xcode and press Run.

## App icon

`ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png` is generated by
`web/static/gen_icons.py`, the same script that makes the web icons.
Replace it with your own 1024×1024 PNG with no transparency if you like.
