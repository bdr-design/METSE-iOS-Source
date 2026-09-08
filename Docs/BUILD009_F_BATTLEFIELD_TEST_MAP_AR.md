# METSE Build 009-F — Battlefield Test Map

## الهدف
إنشاء ساحة اختبار تكتيكية كثيفة ومحدودة الحجم فوق WorldCollision الحالي، بحيث تكون الخريطة جزءًا فعليًا من Simulation Truth وليست مجرد رسم بصري.

## العقود غير القابلة للكسر
- `WorldCollisionCore` هو المصدر الوحيد لهندسة العوائق المستخدمة في collision / LOS / cover / ballistic contact.
- لا توجد قائمة مبانٍ ثانية داخل Bridge أو Metal.
- مساحة العالم تبقى `96m × 96m` ضمن الحدود `[-48,+48]` على X/Z في هذه المرحلة.
- أول 6 عوائق من Build 008/009-A..E تبقى بنفس الترتيب والأبعاد والمواد لحماية Aim Truth والاختبارات القديمة.
- سقف العوائق `20` فقط، وسقف cover candidates مشتق منه: `80`.
- جميع حلقات collision / raycast / cover extraction تبقى bounded.
- لا تغيير في fixed simulation 60 Hz ولا في حد 32 مقاتلًا.
- VERSION/BUILD يبقيان `0.3.0 / 8` حتى Build 009 Release Seal النهائي.

## تركيب الساحة
### Legacy compatibility zone — indices 0...5
الهندسة القديمة كما هي، بما فيها concrete/steel/wood والـlow overhead الخاص باختبار crouch clearance.

### Urban — indices 6...9
كتل Brick/Concrete/Glass في الغرب والشمال الغربي لاختبار:
- حجب LOS.
- cover/peek حول مبانٍ قصيرة ومتوسطة.
- تباين المواد القابلة وغير القابلة للاختراق.

### Industrial — indices 10...14
Steel containers + concrete warehouse + glass booth + thin wood divider في الشرق والجنوب الشرقي لاختبار:
- lanes ضيقة.
- flank / peek transitions.
- steel terminal contact مقابل glass/wood soft contact.

### Rocky/Open — indices 15...19
Rock blocks + Soil berm + north-east brick ruin لاختبار:
- hard natural cover.
- low cover.
- open traversal lanes.

## Open-Lane Truth
الممر `x = 5` من `z=-44` إلى `z=44` يجب أن يبقى مفتوحًا للرؤية/الحركة. هذا ممر اختبار متعمد، وليس فراغًا عرضيًا.

## Legacy Aim Truth
يجب أن تبقى الأشعة التالية بلا عائق:
- camera `(0,1.64,0)` → target 1 `(-10,1.05,18)`
- camera `(0,1.64,0)` → target 2 `(22,1.05,26)`

## المواد
009-F يجب أن تحتوي هندسة فعلية من جميع Material SSOT entries:
- Concrete
- Steel
- Wood
- Brick
- Glass
- Soil
- Rock

## Regression / Stress
`Tests/BattlefieldMapTests.cpp` يغطي:
- هوية legacy obstacles.
- جميع المواد السبعة.
- open lane.
- nearest material contact في Urban / Industrial / Rocky / Soil / Glass.
- collision منع الدخول في urban block.
- بقاء legacy target rays مفتوحة.
- cover pool bounded وأقل من 256 بسبب `uint8_t coverCandidateIndex`.
- 32-agent stress على الخريطة الموسعة.
- decision budget <= 4 لكل fixed step.
- deterministic repeated scenario على مواضع/actions/knowledge الأساسية.

## بوابات القبول
لا تعتبر 009-F مكتملة إلا إذا نجح على نفس commit:
1. Source Integrity.
2. Architecture Guardrails.
3. Build 009-E Guardrails.
4. Build 009-F Battlefield Guardrails.
5. C++20 tests مع `-Wall -Wextra -Wpedantic -Werror`.
6. Swift parse gate.
7. Metal compile.
8. Xcode platform compile.
9. unsigned IPA build + artifact upload.

## خارج النطاق
- لا AI projectile damage على اللاعب قبل Player/Team/Faction/Friendly-fire contract.
- لا Audio/FX؛ هذه 009-G.
- لا Observatory V4/Black Box V2؛ هذه 009-H.
- لا رفع VERSION/BUILD الآن.
