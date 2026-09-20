# Haris 2D Engine — Android

محرك ألعاب 2D أصلي يستخدم **Haris** كلغة اللعبة، ويحوّل مشروعك إلى تطبيق Android عبر Android Studio + NDK.

## الفكرة

```text
Haris source (.hr)
      ↓
Haris VM + JIT (C)
      ↓
Haris game2d API
      ↓
Command Buffer
      ↓
Android Canvas / SoundPool / Vibrator
      ↓
APK
```

ملف اللعبة الافتراضي هو:

`app/src/main/assets/game/main.hr`

استبدله بكود Haris الخاص بك، وضع الصور داخل:

`app/src/main/assets/game/sprites/`

والأصوات داخل:

`app/src/main/assets/game/audio/`

## الميزات

- تشغيل Haris VM داخل عملية Android نفسها.
- JIT الموجود في Haris يبقى جزءًا من الـruntime بدل تشغيل مفسّر منفصل.
- رسم 2D: rectangles, circles, lines, text.
- Sprites + sprite sheets/animation frames.
- كاميرا 2D.
- Touch متعدد حتى 10 أصابع.
- Keyboard/gamepad key events الأساسية عند توفر جهاز إدخال.
- Physics خفيفة: velocity, gravity, AABB collision, collision resolve.
- `dt`, `fps`, `time`, `distance`, `lerp`, animation frame.
- Audio عبر `SoundPool`.
- Vibration/Haptics.
- Save/Load داخل مساحة التطبيق الخاصة بـAndroid.
- Scene name state بسيط عبر `scene/current_scene`.
- Command buffer ثابت الحجم لتقليل تخصيصات Java/C في كل frame.
- التطبيق لا يحتاج WebView أو JavaScript لتشغيل اللعبة.

## API الأساسية في Haris

```haris
win = game2d.window(1280, 720, "My Game")

game2d.begin(win, "#08111f")
game2d.rect(20, 20, 100, 60, "#37d4ff", true)
game2d.circle(200, 80, 30, "#ffd166", true)
game2d.line(0, 0, 400, 200, "#ffffff", 3)
game2d.text(24, 130, "Hello Haris", "#ffffff", 24)
game2d.screen_text(24, 40, "UI", "#ffffff", 18)

game2d.sprite("player", 300, 200, 64, 64)
game2d.sprite_frame("player_sheet", 380, 200, 64, 64, frame, 4, 1)

game2d.camera(100, 0)

game2d.camera_move(10, 0)

player = game2d.body(100, 100, 64, 64, "#37d4ff")
game2d.body_set(player, "gravity", 1450)
game2d.body_set(player, "vx", 300)
game2d.body_step(player, dt)
game2d.body_draw(player)

if game2d.collide(player, wall) {
    game2d.resolve(player, wall)
}

if game2d.key_down("left") { ... }
if game2d.key_pressed("space") { ... }
if game2d.touch_down() { ... }
if game2d.touch_pressed() { ... }

let x = game2d.touch_x()
let y = game2d.touch_y()
let f = game2d.anim_frame(game2d.time(), 8, 4)

game2d.sound("tap", 0.5)
game2d.vibrate(15)
game2d.save("score", "1200")
let score = game2d.load("score", "0")
game2d.scene("game")
let current = game2d.current_scene()

game2d.end(win)
```

## دورة اللعبة

المحرك على Android **host-driven**: لا تجعل Haris تنشئ loop blocking. بدلًا من ذلك، `game2d.run(win, update, 60)` يسجّل callback، ثم Android يستدعيه مع كل frame.

```haris
win = game2d.window(1280, 720, "Game")

fn update(dt) {
    # update + draw هنا
}

game2d.run(win, update, 60)
```

## بناء APK

1. ثبّت Android Studio وAndroid SDK Platform 36 وNDK `27.3.13750724` وCMake.
2. افتح مجلد المشروع هذا في Android Studio.
3. نفّذ Gradle Sync.
4. شغّل `app` على هاتف Android أو Emulator.
5. لبناء APK: `Build > Build APK(s)`.

إعدادات المشروع مثبتة على:

- Android Gradle Plugin: `9.4.1`
- Gradle: `9.6.0`
- Kotlin: `2.4.20`
- Compile/Target SDK: `36`
- Minimum SDK: `26`
- NDK: `27.3.13750724`

## ملاحظات الأداء

المحرك يستخدم C++/C في طبقة التنفيذ وCommand Buffer لتمرير الرسم إلى Android Canvas. الـVM نفسه مأخوذ من Haris Forge ويُضمَّن بدون وحدات الشبكة/الـweb/الـdatabase غير المطلوبة للعبة Android، لتقليل حجم وتعقيد الـnative build.

## حدود هذه النسخة

هذه نسخة **Engine v1.0** جاهزة كنواة فعلية لصنع ألعاب 2D، وليست بديلًا كاملًا لـGodot/Unity بكل أدوات المحرر. لا يوجد داخل هذه النسخة محرر Scene/Tilemap بصري، asset importer، أو physics solver متقدم؛ اللعبة تُكتب مباشرةً بـHaris وتُبنى كتطبيق Android.
