# METSE — Middle East Tactical Simulation Engine

لعبة تكتيكية Native على iPhone مبنية على C++ مستقل + Objective-C++ bridge + Metal مباشر.

## Baseline الحالي
- Version: 0.2.0
- Build: 007
- iPhone / Landscape / Native Metal
- 32 combatants max
- Runtime Integrity Control Plane
- Character Motor + gait model + camera feel
- World Collision Foundation
- In-game Observatory Center
- Black Box 720 frames
- Observatory frame window 600 frames

## حدود الملكية
- C++ هو مصدر حقيقة الـsimulation والـworld collision.
- Swift مسؤول عن واجهة iOS والإدخال والعرض التشخيصي، ولا يملك gameplay state.
- Metal يستهلك snapshots وworld footprints ولا يقرر منطق اللعب.
- مركز الأرصاد read-only على حالة اللعب ويصدر تقريراً تقنياً للمراجعة.
- Content/Data منفصل عن الكود الأصلي.

## سياسة الإصدارات
من Build 007 فصاعداً نفضل حزم تطوير كبيرة مترابطة بدلاً من IPA جديد لكل تعديل صغير. لا يصدر IPA إلا بعد نجاح integrity + guardrails + C++ tests + Swift parse + Xcode iPhoneOS build + package validation.
