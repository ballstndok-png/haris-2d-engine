# Android build

## Build the APK from your phone (recommended)

This project includes a GitHub Actions workflow at:

`.github/workflows/build-apk.yml`

After uploading the project to GitHub:

1. Open the repository in GitHub from your phone.
2. Open **Actions**.
3. Select **Build Haris 2D Engine APK**.
4. Tap **Run workflow**.
5. Wait for the build job to finish.
6. Open the workflow run and download the artifact named:
   `Haris2DEngine-debug-apk`
7. Extract the artifact and install the APK on your Android phone.

The cloud workflow installs the Android SDK Platform 36, NDK `27.3.13750724`, CMake `3.22.1`, and uses JDK 17 + Gradle 9.6.0.

## Local build

Android Studio is optional. A local machine may build the project with Gradle once the required Android SDK/NDK/CMake versions are installed.

Required:
- SDK Platform 36
- NDK `27.3.13750724`
- CMake 3.22.1
- JDK 17+
- Gradle 9.6.0
