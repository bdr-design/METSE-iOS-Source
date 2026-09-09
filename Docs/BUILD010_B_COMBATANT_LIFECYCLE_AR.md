# METSE Build 010-B — Combatant Lifecycle

## الهدف

تجعل هذه المرحلة دورة حياة المقاتل عقداً صريحاً bounded بدلاً من اشتقاقها
ضمنياً من عدة طبقات. DamageCore يبقى مالك صحة وإصابة المقاتل، بينما
CombatantCore ينشر مرآة الحالة التشغيلية إلى Observatory؛ يزامن EngineCore أيضاً
TacticalAI وVisibility من DamageCore مباشرة ويتحقق من اتفاق المرايا، لا من ملكية جديدة للصحة.

## الحالات

- `Active`: حي وقادر على القتال.
- `Wounded`: حي وقادر على القتال مع إصابة مستمرة.
- `Incapacitated`: حي لكن غير قادر على القتال ولا يطلق النار.
- `Dead`: غير حي وغير قابل للاستهداف التشغيلي.
- `Removed`: حالة محجوزة لإزالة مقاتل في جولة لاحقة؛ لا تسمح بإعادة استعمال slot
  أو إخفاء انتقال الحالة داخل طبقة العرض.

مزامنة runtime تمر من `CombatantCore::syncLifecycle` وتخضع لـ`validate()`، وتظهر في
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

## تدقيق ما قبل 010-C

- تُرفض قيم enum غير الصالحة والهوية المكررة وتهيئة slot تترك فجوة قبلها دون mutation.
- Dead لا يعود حياً عبر sync؛ يمكن أن يبقى Dead أو يصبح Removed فقط. Removed نهائية
  ضمن عمر الهوية. `configure`/`reset` مسار تهيئة صريح وليس إعادة إحياء أثناء القتال.
- `syncState` يستخدم نفس بوابة `syncLifecycle` ولا يتجاوزها؛ EngineCore لا يكتب المرآة مرتين.
- اختبارات الحتمية تقارن محركين مستقلين في كل شريحة، وتختبر rollback ورفض الانتقالات.
- Removed مختبرة في النواة فقط: لا spawn/reuse أو تعزيزات أو أحداث squad/mission مضافة
  في 010-B. هذه وظائف لاحقة بعقود مستقلة، وليست قبولاً منجزاً لهذه المرحلة.
