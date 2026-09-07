# METSE Architecture — Native Shell / Independent Engine

## الحدود الصلبة

### App Shell
يعرض Gateway، Settings، Update Center، Diagnostics، ويقرر متى يدخل المستخدم جلسة اللعبة.
لا يحسب ballistics أو AI أو damage.

### Engine Core
C++ portable. يملك الزمن الثابت والحالة المستقبلية للـsimulation.
لا يستورد UIKit/Metal/Foundation.

### Apple Platform Adapter
Objective-C++ فقط. يربط `MTKView` وMetal بالـEngine Core.
هذه الطبقة هي المكان الوحيد المسموح له بعبور C++ ↔ Apple APIs.

### Renderer
Metal presentation. يأخذ snapshot/telemetry من المحرك ولا يصبح مصدر حقيقة للـgameplay.

### Content
JSON/data فقط. كل حزمة تحديث لها schema/sequence/hash. لا JavaScript ولا dylib ولا native code.

## الأداء
- هدف العرض 60 FPS.
- Simulation fixed step = 60 Hz.
- max catch-up = 4 steps لمنع spiral-of-death.
- لا allocations دورية داخل hot simulation path في baseline.
- عند الضغط الحراري لاحقًا نقلل presentation quality قبل gameplay correctness.

## بوابة الدخول
البوابة Native UIKit وليست login barrier.
المسار الأساسي واضح: `ابدأ جلسة تكتيكية`.
الخيارات الثانوية منظمة كوحدات قابلة للتوسع دون ازدحام الشاشة الرئيسية.
