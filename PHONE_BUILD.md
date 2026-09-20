# Build from phone

The easiest path for this Haris Android project is GitHub Actions.

## First time

1. Create a GitHub repository.
2. Upload the contents of `Haris2DEngine` to the repository root.
3. Make sure this file exists:
   `.github/workflows/build-apk.yml`

## Every build

1. Open the repo in GitHub on your phone.
2. Tap **Actions**.
3. Tap **Build Haris 2D Engine APK**.
4. Tap **Run workflow**.
5. Open the completed run.
6. Download `Haris2DEngine-debug-apk`.
7. Extract the APK and install it.

No Android Studio or Android SDK is required on your phone.

## Automatic builds

The workflow also runs automatically when you push to `main` or `master`.
