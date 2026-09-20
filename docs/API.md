# Haris 2D Android API

| API | الوظيفة |
|---|---|
| `game2d.window(w,h,title)` | تعريف المقاس المنطقي للعبة |
| `game2d.run(win, fn, fps)` | تسجيل callback اللعبة؛ لا يحجب UI thread |
| `game2d.begin(win,color)` / `end(win)` | بداية/نهاية الرسم |
| `game2d.clear(color)` | Clear command |
| `rect`, `circle`, `line`, `text` | رسم 2D |
| `screen_text` | نص بدون تأثر بالكاميرا |
| `sprite(name,x,y,w,h,alpha)` | رسم صورة من assets |
| `sprite_frame(name,x,y,w,h,frame,cols,rows)` | رسم frame من sprite sheet |
| `camera`, `camera_move` | كاميرا 2D |
| `window_size`, `screen_size` | المقاسات |
| `dt`, `fps`, `time` | معلومات الزمن |
| `key_down`, `key_pressed`, `key_released` | لوحة المفاتيح/أجهزة الإدخال |
| `touch_count` | عدد الأصابع الفعالة |
| `touch_down/pressed/released` | حالة touch |
| `touch_x/y/dx/dy` | إحداثيات وحركة الإصبع؛ optional pointer id |
| `mouse_position`, `mouse_down` | compatibility layer باستخدام أول touch |
| `body` | إنشاء جسم 2D كـHaris struct |
| `body_set`, `body_step`, `body_draw` | فيزياء خفيفة |
| `collide`, `resolve` | AABB collision + resolution |
| `distance`, `lerp` | math helpers |
| `anim_frame` | حساب frame متكرر من الزمن |
| `sound` | تشغيل sound asset |
| `vibrate` | haptic |
| `save`, `load` | تخزين نصي داخل app-private storage |
| `scene`, `current_scene` | حالة scene الحالية |
| `quit`, `close`, `is_open`, `poll` | lifecycle flags |

## Sprite assets

ضع:

`app/src/main/assets/game/sprites/player.png`

ثم:

```haris
game2d.sprite("player", 100, 100, 64, 64)
```

للـsprite sheet استخدم grid:

```haris
frame = game2d.anim_frame(game2d.time(), 10, 8)
game2d.sprite_frame("player_sheet", 100, 100, 64, 64, frame, 4, 2)
```
