# METSE Build 010-B — Combatant Lifecycle

## الهدف

تجعل هذه المرحلة دورة حياة المقاتل عقداً صريحاً bounded بدلاً من اشتقاقها
ضمنياً من عدة طبقات. DamageCore يبقى مالك صحة وإصابة المقاتل، بينما
CombatantCore ينشر الحالة التشغيلية التي يستهلكها TacticalAI وVisibility وObservatory.

## الحالات

- `Active`: حي وقادر على القتال.
- `Wounded`: حي وقادر على القتال مع إصابة مستمرة.
- `Incapacitated`: حي لكن غير قادر على القتال ولا يطلق النار.
- `Dead`: غير حي وغير قابل للاستهداف التشغيلي.
- `Removed`: حالة محجوزة لإزالة مقاتل في جولة لاحقة؛ لا تسمح بإعادة استعمال slot
  أو إخفاء انتقال الحالة داخل طبقة العرض.

كل انتقال يمر من `CombatantCore::syncLifecycle` ويخضع لـ`validate()`، ويظهر في
تقرير ثابت الحجم يدخل في `EngineDiagnostics` وObservatory. الحالة تدخل أيضاً في
`deterministicStateHash` حتى لا تتباعد سيناريوهات replay.

## الملكية والذرية

- DamageCore يحدد `CombatState` من الصحة/النزف.
- EngineCore يحولها إلى lifecycle ويزامنها مع CombatantCore بعد كل fixed slice.
- أي اختلاف بين DamageCore وCombatantCore يفشل invariant ويعيد الشريحة كاملة عبر
  checkpoint/rollback.
- لا توجد قوائم أو allocations جديدة في hot path، والحد الأعلى ما زال 32.

## القبول

- انتقالات Active/Wounded/Incapacitated/Dead/Removed محددة ومتحققة.
- counters lifecycle ثابتة وموجودة في Observatory.
- deterministic hash يتغير عند اختلاف lifecycle.
- اختبارات Build 009 و010-A تبقى خضراء.
