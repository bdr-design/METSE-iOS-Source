# METSE Architecture — Native Shell / Independent Engine

## الحدود الصلبة

### App Shell
يعرض Gateway، Settings، Update Center، Diagnostics، ويقرر متى يدخل المستخدم جلسة اللعبة.
لا يحسب ballistics أو AI أو damage.

### Engine Core
C++ portable. يملك الزمن الثابت وحالة الـsimulation.
لا يستورد UIKit/Metal/Foundation.

من Build 005 يملك أيضًا Runtime Integrity داخليًا:
`Intent -> Command Admission -> Scoped Atomic Mutation -> Invariant Validation -> Commit/Rollback -> Event Ledger`.

### Runtime Integrity
- Control Plane typed داخل C++ وليس JavaScript.
- Command IDs + correlation/causation IDs.
- معاملات صغيرة scoped وليست نسخًا للعالم كله.
- Event Ledger محدود 256 حدثًا مع SHA-256 chain.
- Command Ledger محدود 96 أمرًا.
- Black Box محدود 720 frame (~12 ثانية عند 60 FPS).
- أي invalid input يفشل مغلقًا بدون تلويث state.

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
- Ring buffers ثابتة السعة للـIntegrity/Black Box حتى لا تنمو الذاكرة مع طول الجلسة.
- عند الضغط الحراري لاحقًا نقلل presentation quality قبل gameplay correctness.

## بوابة الدخول
البوابة Native UIKit وليست login barrier.
المسار الأساسي واضح: `ابدأ جلسة تكتيكية`.
الخيارات الثانوية منظمة كوحدات قابلة للتوسع دون ازدحام الشاشة الرئيسية.

راجع `RUNTIME_INTEGRITY_AR.md` لعقد Build 005 الكامل.
