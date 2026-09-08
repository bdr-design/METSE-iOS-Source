# METSE Build 009 — Tactical Combat & Battlefield Systems

## الهدف
نقل METSE من Combat Foundation إلى قتال تكتيكي قابل للتوسع على iPhone، مع سقف ثابت 32 مقاتل من الطرفين مجتمعين، وبدون إضافة أنظمة ضخمة لا تخدم الاشتباك المباشر.

## مبادئ غير قابلة للكسر
1. Simulation Core هو المالك الوحيد لحالة القتال والزمن والقرارات التكتيكية.
2. UIKit/Metal Bridge ينقل الإدخال والـsnapshots فقط، ولا يقرر gameplay state.
3. لا معرفة سحرية للـAI: الرؤية والسمع والذاكرة هي المصادر الوحيدة للمعرفة عن اللاعب.
4. Aim Truth مستمر: المقذوف، نقطة التصويب، الـcrosshair، والـLOS يجب أن تتفق هندسيًا.
5. كل قائمة/Pool حرجة bounded مسبقًا؛ الحد الأعلى للمقاتلين 32.
6. لا allocations غير محدودة ولا work queues ثانية في المسار الحرج.
7. أي rollback أو Integrity rejection يجب أن يعني خللًا حقيقيًا، وليس رفضًا طبيعيًا لقواعد اللعب.
8. الأداء يقاس على الجهاز الحقيقي بجلسات طويلة، وليس benchmark قصير فقط.

## مرجع تصميمي عام
- Call of Duty العلني يركز على اتساق weapon motion مع مسار الطلقة، إزالة عدم اليقين المخفي من الدقة، ومحاذاة FOV/السلاح/الكاميرا بحيث يكون الإطلاق مقروءًا ومتوقعًا.
- PUBG العلني يفصل vertical recoil وhorizontal recoil وcamera shake/recovery كعوامل موازنة مستقلة، مع اختلافات حسب وضعية اللاعب والفئة.
- Apple توصي بقياس frame pacing وCPU/GPU timelines على الجهاز، وتعتبر hitch الطويل مشكلة منفصلة عن مجرد متوسط FPS، مع تصميم إعدادات مستدامة للأجهزة المحمولة.

هذه مراجع سلوكية/هندسية فقط. لا يُنسخ كود أو Assets أو بيانات خاصة من أي لعبة أخرى.

## مراحل Build 009
### 009-A — Tactical Perception Contract
- FOV + distance + world LOS.
- hearing radius مشتق من شدة الضوضاء.
- last-known-position + confidence + memory decay.
- threat score.
- squad order أولي Hold/Search/Assault.
- لا تنفيذ إطلاق نار للـAI قبل ثبات الإدراك.

### 009-B — Player & Weapon Handling V2
- sprint-to-fire readiness.
- weapon mass/inertia profile.
- recoil impulse + recovery منفصلان.
- stance modifiers.
- chamber/reload/fire mode states.
- wall obstruction/retraction.

### 009-C — Ballistics & Materials V2
- مواد: concrete/brick/wood/steel/glass/soil/rock.
- resistance/thickness contract.
- penetration chain bounded.
- ricochet eligibility bounded ومحدد بالزاوية والطاقة.
- terminal contact telemetry منفصل عن contact count.

### 009-D — Anatomy & Combat State
- hit regions أكثر دقة.
- armor zones.
- incapacitation/bleeding bounded state machine.
- reaction direction based on impact vector.

### 009-E — Tactical AI Actions
- cover candidates محدودة ومسبقة.
- move/hold/peek/suppress/flank/retreat.
- squad knowledge sharing محدود المدى والزمن.
- لا global hive mind.

### 009-F — Battlefield Test Map
- خريطة شرق أوسطية كثيفة ومحدودة الحجم لاختبار 32 مقاتلًا.
- urban + industrial + rocky/open lanes.
- cover/LOS traversal test routes.

### 009-G — Audio / FX / Visibility
- surface footsteps.
- indoor/outdoor firing layers.
- bullet crack/near miss.
- bounded FX pools.
- visibility budget tiers.

### 009-H — Observatory V4 + Black Box V2
- AI perception cost / LOS count / active agents.
- gameplay denial reasons.
- projectile contact chain.
- core lock wait / callback gaps / simulation slice cost.
- rolling pre-spike black box.

## بوابات القبول
- 32 combatants max enforced.
- no-magical-awareness tests.
- deterministic AI perception for identical state/input.
- 25/50/100m Aim Truth regression.
- nearest-hit collision ordering.
- no integrity pollution from ordinary gameplay denials.
- Source Integrity + Architecture Guardrails + C++ + Swift + Metal + Xcode + unsigned IPA all green.
- real-device 5/30/60-minute sessions before declaring Build 009 stable.
