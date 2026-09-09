# Build 009-H — Observatory V4 / Black Box V2

هذه المرحلة تضيف قياسًا bounded من مصدر المحاكاة نفسه دون تغيير gameplay truth.

- Observatory V4 يسجل زمن simulation slice، AI active/LOS/decision counts، وسلسلة projectile contacts.
- Black Box V2 يحتفظ بسجل rolling ثابت السعة، ويضع `preSpike` على الإطار السابق/المرافق للتأخر أو catch-up clamp أو slice أعلى من 20ms.
- قياس `_coreLock` وcallback gaps يبقى في Bridge telemetry؛ لا يثبت سبب hitch 1150ms تلقائيًا.
- لا توجد allocations أو queues جديدة في hot path، ولا تغيير في fixed 60 Hz أو حد 32 combatants.
- AI projectile damage وPlayer/Team/Faction contract خارج نطاق 009-H.

## Acceptance

- strict C++، Swift، Metal، Xcode وunsigned IPA على نفس SHA.
- regression للـpercentiles، simulation slice boundaries، AI/LOS، projectile contact chain، rolling Black Box وdeterminism.
- لا تغيير في VERSION/BUILD قبل Release Seal.
