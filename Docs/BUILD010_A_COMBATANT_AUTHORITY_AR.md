# METSE Build 010-A — Combatant Authority / Player Damage

## الهدف

هذه المرحلة تغلق الفجوة التي تركها Build 009-E عمدًا: لا توجد قبلها هوية
Combatant كاملة، ولا Team/Faction filtering، ولا Player DamageTarget، ولا مسار
موثوق من AI weapon إلى ضرر اللاعب.

## مصدر الحقيقة والملكية

- CombatantCore هو مصدر حقيقة الهوية والعلاقة بين المقاتلين، بسعة ثابتة 32.
- TacticalAICore يملك حركة الـAI وحالة سلاحه.
- DamageCore يملك حالة الإصابة، ويستقبل مواضع AI كمرآة one-way فقط.
- EngineCore يملك ترتيب الشريحة 60 Hz، وتنسيق الـatomic checkpoint/rollback.
- WorldCollisionCore يظل مصدر حقيقة LOS/obstruction/ballistic world hit.
- Bridge/Metal/Audio يستهلكون snapshot والأحداث ولا يتخذون قرارات لعب.

## عقد الهوية والعلاقة

كل Combatant له id وteamId وfactionId وrole. العلاقة تكون:

- Self: يمنع إصابة المصدر لنفسه.
- Friendly: يمنعها HostileOnly، وتُسمح فقط عند سياسة AllowFriendlyFire.
- Neutral: لا تُقبل في المسار العدائي الافتراضي.
- Hostile: يُسمح بالاستهداف.

مصدر بلا provenance يبقى مسار اختبارات/توافق قديمًا ولا يُستخدم في EngineCore
الإنتاجي. كل طلقة إنتاجية تحمل sourceCombatantId/team/faction.

## المسار القتالي

    TacticalAI Vision + fire authorization
      -> TacticalAI WeaponCore.fire
      -> EngineCore attaches Combatant provenance
      -> BallisticsCore projectile
      -> DamageCore source-filtered trace
      -> Player DamageTarget / AI DamageTarget
      -> bounded DamageResult ledger + Integrity events

لا يوجد استدعاء مباشر لخفض صحة اللاعب من AI، ولا نسخة ثانية من حركة الكيان.
الـPlayer target مخزن مستقل داخل DamageCore حتى تبقى أول أهداف التدريب القديمة
متوافقة، وتبقى إصابات AI للاعب مفعلة فقط مع includePlayerTarget.

## الحدود والذرية

- 32 Combatants كحد أعلى.
- 128 projectile وDamage ledger سعته 192 كما في Build 009.
- AI shots تُجمع في std::array ثابتة وتُرفض عند امتلاء projectile pool بدون
  blocking.
- فشل provenance أو spawn أو invariant يعيد الشريحة كاملة إلى checkpoint.
- friendly-fire denials gameplay outcomes؛ لا تلوث Integrity Journal.

## القبول

- تثبيت هوية player/AI والعلاقات والحالة.
- منع إصابة اللاعب من مصدر friendly.
- AI projectile يصل إلى Player DamageTarget عبر Ballistics.
- nearest world hit وLOS ما زالا حاكمين.
- 32-combatant setup bounded.
- AI shots/player impacts deterministic في سيناريو مكرر.
- Build 008/009 regressions وجميع بوابات CI تبقى خضراء.
