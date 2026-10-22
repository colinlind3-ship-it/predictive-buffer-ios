# Build from an iPhone using GitHub Actions

1. Open your repository in Safari.
2. Tap **Add file → Upload files** and upload the project files from this ZIP, including the `.github/workflows/build.yml` file. If the iOS app/browser upload flow omits hidden `.github` folders, create `.github/workflows/build.yml` with the GitHub web editor.
3. Commit the files to the repository's default branch.
4. Open **Actions**. If asked, enable GitHub Actions.
5. Choose **Build Geode Mod (iOS)**, then **Run workflow** (or let the push trigger it).
6. Open the run and inspect its logs. If successful, download the `output-ios` artifact.
7. Extract the downloaded ZIP in the iPhone Files app and locate the `.geode` file.

This is an attempted port, not a verified build. Compiler errors may reveal more iOS-specific code that needs changing. The Windows console is disabled on iOS; use Geode's log. The F6–F10 hotkeys may not be practical on a touchscreen.
