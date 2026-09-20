# Validation

تم اختبار النواة المضمنة على Linux-host محليًا مع تعريف `__ANDROID__` ومحاكاة بسيطة لطبقة Android:

- Compile للـHaris VM + Android runtime: نجح.
- Parse وتشغيل `app/src/main/assets/game/main.hr`: نجح.
- تشغيل عدة frames متتالية: نجح.
- `game2d.window/run/body/body_step/body_draw`: نجح.
- `game2d.sprite/sprite_frame/text/circle`: نجح عبر command buffer.
- Touch pressed/released + sound/vibrate commands: تم التحقق من المرور إلى command buffer.
- Save/load + scene/current_scene: تم التحقق منهما.
- JNI C++ syntax check باستخدام JDK JNI headers: نجح.

لم يتم إنتاج APK داخل بيئة التنفيذ الحالية لأن Android SDK/NDK/CMake غير مثبتة فيها.
