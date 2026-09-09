# Build 009-H — Observatory V4 / Black Box V2

هذه المرحلة تضيف قياسًا bounded من مصدر المحاكاة نفسه دون تغيير gameplay truth.

- Observatory V4 يسجل زمن simulation slice، AI active/LOS/decision counts، وسلسلة projectile contacts.
- التقرير يفرق صراحةً بين session counters ونافذة القياس rolling (600 إطار)، ويعرض مدة كل نافذة وعدد العينات المرفوضة وانحدار simulation tick؛ لذلك لا تختلط أرقام جلسة طويلة مع percentile قصيرة.
- Black Box V2 يحتفظ بسجل rolling ثابت السعة، ويضع `preSpike` على الإطار السابق/المرافق للتأخر أو catch-up clamp أو slice أعلى من 20ms.
- كل `preSpike` يحمل سببًا bounded مستقلًا: callback delta، catch-up clamp، أو slow simulation slice، وتظهر عدادات session وretained لكل سبب.
- لا يعتبر fallback العرض الحراري إلى 30 FPS (إطاران من المحاكاة 60 Hz) spike بحد ذاته؛ تبقى clamp أو slice البطيئة أو التأخر المادي هي الإشارات المعتمدة.
- قياس `_coreLock` وcallback gaps يبقى في Bridge telemetry مع متوسط/أقصى gap وحدود >budget و50/100/250ms، مع استبعاد فجوات lifecycle المقصودة؛ لا يثبت سبب hitch 1150ms تلقائيًا.
- التقرير يضع `diagnosticProblemMask` للمشاكل الفعلية و`acceptanceCoverageMask` للفجوات التجريبية (جلسة قصيرة، AI دون 32 عميلًا، أو دون مقذوفات) حتى لا تُعرض جلسة هادئة كدليل تغطية كامل.
- يتضمن التقرير قاموس bitmask ثابتًا؛ `thermalCritical` فقط عطل حراري، بينما `thermalFallback` إلى 30 FPS حالة تخفيض عرض متوقعة وليست فشل محاكاة.
- لا توجد allocations أو queues جديدة في hot path، ولا تغيير في fixed 60 Hz أو حد 32 combatants.
- AI projectile damage وPlayer/Team/Faction contract خارج نطاق 009-H.

## Acceptance

- strict C++، Swift، Metal، Xcode وunsigned IPA على نفس SHA.
- regression للـpercentiles، simulation slice boundaries، AI/LOS، projectile contact chain، rolling Black Box وdeterminism.
- regression لفصل session/window، رفض عينات telemetry غير الصالحة، وانحدار tick، وإسناد سبب الـpre-spike.
- لا تغيير في VERSION/BUILD قبل Release Seal.
