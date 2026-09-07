# METSE V0.1 — المعمارية

## نطاق النسخة
قاعدة تشغيلية لمحاكي تكتيكي iPhone-first بحد أقصى 32 مقاتلاً. لا MassEntity ولا محاكاة جيوش ضخمة.

## مبادئ ثابتة
- اللاعب والاشتباك القريب أعلى أولوية.
- Server authoritative لأي حالة قتالية مشتركة.
- لا Tick عشوائي.
- كل نظام له fallback وميزانية.
- لا Lumen/Nanite في baseline المحمول.
- جلسة 120 دقيقة هي معيار الأداء لاحقاً على جهاز فعلي.

## طبقات V0.1
1. Core: MovementIntent مضغوط شبكياً.
2. Character: Pawn قابل للحركة بدون Tick مخصص.
3. Combat: Health replication وسلطة الضرر على السيرفر.
4. Performance: Governor + World budgets.
5. CI: manifest + guardrails + Automation Tests + iOS build gate.
