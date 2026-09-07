# METSE Runtime Integrity — Build 005

هذه الطبقة تنقل أفضل مبادئ الحماية من مشروع Global Holdings إلى محرك METSE بدون نقل WebView أو JavaScript أو معاملات deep-clone الثقيلة.

## 1. ملكية الحالة
`EngineCore` هو المالك الوحيد لحالة المحاكاة التكتيكية الحالية. أي تعديل خارجي على حالة اللاعب أو القتال يمر عبر مسار `executeAtomic` واحد.

## 2. Command Plane
كل أمر يحصل على:
- Command ID متسلسل.
- Correlation ID افتراضي يساوي Command ID.
- Causation ID قابل للتوسعة لاحقًا لأوامر AI/الشبكة.
- حالة نهائية: Committed / Rejected / RolledBack.

الأوامر الحالية:
- ResetSession
- SetActiveCombatants
- SetMovementIntent
- AddLookIntent
- FireWeapon

## 3. Atomic scoped transaction
المعاملة لا تنسخ العالم كله. قبل العملية يؤخذ checkpoint صغير فقط لحالة الـdomain الحالي:
- EngineSnapshot
- accumulator
- movement intent

بعد التنفيذ تمر الحالة على invariants. عند أي كسر يتم rollback إلى checkpoint وتسجيل CommandRolledBack.

## 4. Event Ledger + SHA-256 chain
الأحداث المهمة تدخل ring buffer محدودًا إلى 256 حدثًا. كل حدث يحمل previousHash/hash بسلسلة SHA-256 portable داخل C++.
الهدف: كشف فساد أو كسر تسلسل السجل، وليس استخدامه كتوقيع تشفيري للمحتوى الخارجي.

## 5. Black Box
آخر 720 frame محفوظة في ring buffer ثابت السعة (~12 ثانية على 60 FPS). كل frame تسجل:
- simulation tick/time
- real frame delta
- position/yaw/pitch
- movement intent
- shots fired
- catch-up steps
- هل تم clamp بسبب hitch/backlog

لا يوجد نمو غير محدود للذاكرة.

## 6. Fail-closed invariants
أمثلة الرفض/rollback:
- أكثر من 32 مقاتل.
- NaN/Infinity في input.
- pitch خارج الحد.
- state غير finite.
- movement vector أكبر من الحد.

## 7. اختبارات Build 005
الاختبارات تغطي:
- SHA-256 known vectors.
- 32-combatant cap.
- deterministic fixed-step movement.
- rejection بدون تلويث state.
- rollback فعلي بعد mutation متعمدة تكسر invariant (test-only hook).
- journal chain verification.
- bounded command/event/black-box capacity.

## 8. حدود المرحلة
هذه ليست بعد Save Vault كامل ولا Networking command replication. المرحلة التالية تبني فوقها domain transactions للأسلحة/الضرر/AI، ثم durable A/B Save Vault باستخدام نفس IDs/events بدون جعل renderer أو UIKit مصدر حقيقة.
