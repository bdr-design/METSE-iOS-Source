# METSE Build 009-G — Audio / FX / Visibility

## النطاق

تضيف 009-G طبقة أحداث صوتية ومؤثرات مرئية محدودة السعة، وتحوّل درجات
`VisibilityCore` إلى ميزانية عرض فعلية. لا يملك أي جزء من هذه الطبقة قرارًا قتاليًا،
ولا تغيّر الصوتيات أو المؤثرات مسار المقذوف أو نتيجة الضرر.

تبقى القواعد التالية ثابتة:

- `EngineCore` هو مالك زمن المحاكاة عند 60 Hz.
- `WeaponCore` هو المصدر الوحيد لحالة الإطلاق والتلقيم.
- `WorldCollisionCore` هو المصدر الوحيد لهندسة العالم والمواد وLOS.
- `VisibilityCore` هو المصدر الوحيد لقرار درجة عرض المقاتل.
- Bridge وAVFoundation وMetal مستهلكات عرض فقط.
- `VERSION / BUILD` يبقيان `0.3.0 / 8` حتى Release Seal النهائي لـBuild 009.

## World surface وacoustic truth

يملك `WorldCollisionCore` ست رقع أرضية ثابتة وغير متداخلة للمواد الصريحة:

- Steel
- Concrete
- Rock
- Wood
- Brick
- Glass

كل موضع خارجها يستخدم Soil. نقاط الاختبار لكل مادة قابلة للوصول ولا تقع داخل
collision solid. لا توجد قائمة مواد أو خريطة صوتية ثانية في UIKit أو Bridge.

تصنيف إطلاق النار يستخدم `acousticProbeAt()` فقط:

- 5 raycasts ثابتة السقف.
- شعاع علوي بطول 5m.
- أربعة أشعة أفقية بطول 18m.
- Indoor يتطلب سقفًا وارتدادين أفقيين على الأقل؛ السقف المنفرد يعامل كـoutdoor canopy.

## Footsteps

الخطوة تنتج بعد الحركة المقبولة وworld collision، وليس من joystick الخام. شروطها:

- اللاعب grounded.
- إزاحة أفقية فعلية.
- gait صالح للمشي.
- cadence محسوب بالمسافة، مع spacing ثابت لكل Walk/Tactical/Jog/Sprint/Crouch.
- Idle وCrawl والحركة الهوائية لا تنتج footstep.
- material يقرأ عند موضع القدم من `WorldCollisionCore::surfaceMaterialAt()`.

## Gunshot وprojectile audio

نجاح Fire transaction داخل `EngineCore` ينتج cue واحدًا من:

- `FireOutdoor`
- `FireIndoor`

وينتج `MuzzleFlash` من FX pool بنفس correlation ID. رفض الإطلاق الطبيعي لا ينتج cue
ولا يلوث Integrity Journal.

`BallisticsCore` يمرر `ProjectileSegmentObservation` مباشرة عبر callback محدود؛ لا
ينشئ trajectory queue ولا ينسخ contact list. المراقبة تشمل المسار الحقيقي حتى أقرب
target/world contact، وتحمل علامة termination.

حدود الاستماع الثابتة:

- Bullet crack: سرعة `>= 360 m/s` ومسافة `<= 12m`.
- Near miss: سرعة `>= 80 m/s` ومسافة `<= 2.25m`.
- الذاكرة محدودة إلى `BallisticsCore::kMaxProjectiles = 128` correlation entries.
- كل نوع cue يصدر مرة واحدة لكل projectile correlation.
- بعد terminal observation لا تقبل correlation نفسها near miss متأخرًا.

المسار الباليستي الإنتاجي الحالي مملوك للاعب، لذلك يمرر صراحةً
`hostileToListener=false` إلى مستمع اللاعب. لا تنشئ 009-G مقذوف AI وهميًا ولا ضررًا
مباشرًا. تفعيل hostile provenance ينتظر عقد Player/Team/Faction/Friendly-fire الرسمي.

## السعات وسياسة الضغط

`AudioFXCore` لا يستخدم dynamic containers:

- Audio cue ring: 64.
- FX pool: 48.
- Projectile cue memory: 128.

الـcue ring يحتفظ بآخر 64 sequence؛ يستطيع Bridge اكتشاف أي gap بدل قراءة حدث غير
موجود. FX pool يستخدم `drop-new`: إذا كانت الخانات الـ48 نشطة، يسقط الطلب الجديد
ويزيد `fxDropped`. لا ينتظر simulation ولا يطرد مؤثرًا منشورًا في منتصف عمره.

## Visibility budgets

`VisibilityCore` ينفذ distance/FOV وLOS إلى هندسة `WorldCollisionCore` قبل إعطاء tier.
الهدف المحجوب يبقى Dormant، والـrenderer لا يعيد قرار occlusion من نفسه.

الميزانيات الثابتة لسقف 32 مقاتلًا:

- Full: 8.
- Reduced: 12.
- Minimal: 12.

الترتيب deterministic حسب score ثم entity ID. Metal يستهلك tier المنشور:

- Full: body + head detail.
- Reduced: body detail مخفّض.
- Minimal: marker منخفض التكلفة.
- Dormant: لا يرسم.

هذا تخفيض presentation detail فقط؛ لا يغير AI perception أو ballistics أو damage.

## Apple / Metal presentation

`METSEAudioPresenter` يستخدم `AVAudioSourceNode` مع 12 voice ثابتة. لا توجد dispatch
queue أو audio command queue ثانية. كل voice يولد طبقة PCM قصيرة حسب cue/material،
والـBridge يطبق attenuation/pan فقط بعد أخذ snapshot. عند امتلاء الأصوات يسقط أحدث
صوت presentation دون أي feedback إلى gameplay.

Metal يستهلك `FXInstance` المنشورة:

- شدة muzzle flash مشتقة من العمر المتبقي لـ`MuzzleFlash`، لا من عداد shots مكرر.
- Surface dust له budget عرض ثابت 16، وهو أقل من simulation FX cap.
- اختلاف budget العرض لا يغيّر FX state داخل المحاكاة.

## اختبارات القبول

`Tests/AudioFXVisibilityTests.cpp` يغطي:

- هوية المواد السبعة عند أسطح قابلة للوصول.
- عدم وجود خطوات عند الثبات/Idle/Crawl/airborne.
- حدود cadence لكل gait/stance.
- deterministic indoor/outdoor acoustic probe.
- حدود crack/near-miss الدقيقة.
- عدم إصدار near miss بعد termination.
- callback من trajectory الفعلية في Ballistics.
- FX saturation وdrop-new وcue ring overwrite.
- ضغط وترتيب deterministic لـ32 مصدرًا.
- حدود 35/80/150m للـvisibility.
- WorldCollision LOS ومنع renderer-only visibility.
- ميزانيات 8/12/12 وتكرار السيناريو الحتمي.
- Engine ownership وrollback/reset وعدم self near-miss.

تبقى جميع اختبارات Aim Truth وBallistics/Materials وDamage و009-E و009-F إلزامية في
نفس strict C++ runner.

## خارج النطاق

- AI projectile damage أو Player damage target أو Team/Faction/Friendly-fire.
- إعادة هيكلة `_coreLock` أو حسم سبب hitch 1150ms.
- Observatory V4 وBlack Box V2؛ هذه 009-H.
- رفع VERSION أو BUILD.
