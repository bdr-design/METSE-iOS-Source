# METSE Build 010-D — Injury / Local Team Reactions

## المرجعية وحدود الاستفادة

الأساس هو main الأخضر `7cd31ba6b17304f9c29c2d8aee9d68dcaf042899` بعد 010-C،
مع ARCHITECTURE_AR وعقود 009-E و010-A/B/C. الملف BUILD009_REFERENCE_BINARY_AUDIT_AR
يوضح أن My Files1/2/3 حزمة Unity/Harekat2 ثنائية تتضمن مؤشرات حقن، وليست مصدر METSE.
لم تُنقل منها أصول أو مكتبات أو مفاتيح إعداد أو مكونات حقن.

مصادر إلهام رسمية تاريخية من Squad، وليست وصفاً لأحدث إصدار أو لكود داخلي متاح:

- [Offworld: Revisiting the Infantry Combat Overhaul، 20 سبتمبر 2023](https://www.joinsquad.com/archive/revisiting-the-infantry-combat-overhaul-c0926):
  يوضح أهمية تعاون الفريق وتأثير الضغط واختبارات اللعب التكرارية. نستفيد من أهداف
  السلوك واختبار التوازن، ولا نتبنى جعل حركة السلاح البصرية تغير حقيقة المقذوف في METSE.
- [Squad 6.0 release notes، 26 سبتمبر 2023](https://www.joinsquad.com/archive/squad-update-6-0-release-notes-eb2b4):
  يربط الضغط بإعادة التموضع والاحتماء وإتاحة وقت للاستجابة. تحويل ذلك إلى استجابة AI
  محدودة بالزمن والمعرفة هو تصميم خاص بـMETSE، لا ادعاء أن Squad يطبق هذا الكود أو AI مماثلاً.

أرقام 0.35s و2s و18m و128 أدناه قرارات METSE تحتاج ضبط جهاز/لعب؛ ليست قيماً من Squad.
لا رفع لسقف 32 مقاتلاً، ولا PiP أو network أو عتاد إضافي أو تغيير Aim Truth.

## عقد الحدث والذرية

- DamageCore وحده يحدد الإصابة/النزف/العجز/الموت ويضيف DamageResult ذا sequence وcorrelation.
- EngineCore يستهلك النتائج بالترتيب داخل fixed slice بعد ballistics/bleeding ومزامنة
  الحالة القتالية. يرسلها مباشرة إلى TacticalAI دون queue ثانية.
- observer لا يخفض health ولا يكتب حالة سلاح موازية. health01 تظل مرآة DamageCore.
- sequence مكرر أو منقطع أو هوية غير موجودة أو payload غير صالح يفشل بلا أثر؛ داخل
  Engine يؤدي إلى rollback كامل. السجل نفسه ليس قناة تنبيه عالمية للـAI.
- ردود الفعل وعداداتها وموضع الاستهلاك تدخل checkpoint/hash/invariant. أحداث Integrity
  الخاصة بالضرر لا تُنشر إلا بعد نجاح الشريحة. لا event تسريب من slice متراجعة.

## استجابة الإصابة الذاتية

Impact مسجل مع hit وdamage > 0، لمقاتل AI لا يزال combatCapable، يضع recovery=0.35s.
يمنع إطلاقه أثناء التعافي ويطلب إعادة تقييم ضمن الأربع قرارات المسموحة. لا انحراف
خفي للتصويب ولا تغيير للطلقة التي أُطلقت قبل الإصابة في نفس الشريحة.
النزف لا يعيد تفعيل recovery كل شريحة، وarmor-only دون damage لا يفعلها.
الاستجابة لا تمنح معرفة اتجاه/موضع المهاجم؛ دون معرفة سابقة يبقى Hold.

## مشاهدة خسارة زميل محلياً

عند Incapacitated أو Dead فقط، يفحص TacticalAI مرشحين bounded من الـAI الحاليين:

1. المستقبِل combatCapable، ليس المصاب نفسه، وعلاقته Friendly وفق CombatantCore.
2. المسافة ثلاثية الأبعاد <= 18m وموقع الخسارة داخل horizontal FOV الحالي للمستقبِل.
3. WorldCollision LOS من عين المستقبِل إلى صدر المقاتل غير محجوب.

المشاهدة الناجحة تضع concern=2s مع casualty ID وcorrelation. عند معرفة تهديد مسبق
تُفضَّل cover/retreat وتُمنع العودة إلى peek أثناء هذه الاستجابة في قرار الغطاء؛
لا تُنشئ مشاهدة الزميل أي Vision أو hearing أو تحديث lastKnownPlayerPosition.
لا relay للخبر ولا hive mind. المستقبِل الذي ينتظر دوره يبقى تحت ميزانية الأربع
قرارات؛ الاستجابة لا تعني توقفاً متزامناً فورياً لكل الفريق.

هذا **local team reaction** وليس نظام SquadId أو squad roster/leader/mission جديداً.
لا revive أو medic أو reinforcements أو slot reuse في المرحلة. الجثة تبقى في
موضع TacticalAI الموجود؛ لا أنيميشن جسم جديد يُستخدم كحقيقة تصادم.

## انتهاء الحالة والحدود والمرصد

التوقيت simulation-only. عند الانتهاء تُمسح IDs/correlation المرتبطة، وعند فقدان
combatCapable تُمسح جميع ردود الفعل. لا health/state جديد يظل حياً بعد موت المقاتل.
فحوص مشاهدة الخسائر <=128 للشريحة؛ ترتيب damage ledger ثم agent index حتمي.
البقية تُسقط drop-tail دون تأجيل، مع استمرار احتساب الضرر واستهلاك sequence.

| الحقل | المعنى |
|---|---|
| recoveringAgents / concernedAgents | أعداد حالية، وليست أعطالاً |
| injuryReactions | إصابات Impact فعّلت التعافي خلال الجلسة |
| witnessedLosses | أزواج حدث خسارة/مستقبِل شاهدها، لا عدد القتلى الفريد |
| lossChecksThisStep / 128 | فحوص مرشحين بآخر شريحة |
| lossBudgetDrops | أحداث خسارة قُطعت معالجة مستقبليها جزئياً/كلياً |
| lastObservedDamageSequence | آخر DamageResult مستهلك بالترتيب |

العدادات تصل Bridge/Observatory كنص وsnapshot فقط. budget drop ليس Integrity failure.

## الاختبارات والقبول

TacticalInjuryTeamTests: أثر الإصابة والتعافي، عدم mutation للصحة، رفض duplicate/gap/
NaN/unknown identity، عدم قفل الإطلاق المستمر بسبب bleed، تجاهل غير القادر على القتال،
حدود المسافة/FOV وLOS وFriendly، عدم نقل موقع المهاجم أو relay، انتهاء IDs، ضغط
32 مقاتلاً وdrop policy وعدد القرارات، ضرر حقيقي ثم rollback/replay لمحركين مستقلين.
جميع اختبارات 009 و010-A/B/C تبقى مفعلة. guardrails_010d يسبق strict C++، ثم بوابات
Swift/Metal/Xcode/IPA/upload على head SHA صريح، ثم CI على main بعد الدمج.

تقرير الجهاز المرفوع من 009-H يمثل نحو 84.5s وذروة AI=2 مع callback max=53.05ms؛
ليس اختبار 32-combatant أو 30/60 دقيقة. لا يثبت غياب مشاكل أخرى أو حل hitch 1150ms.
لا نعلن قبولاً حرارياً أو لعبياً ولا نرفع VERSION/BUILD دون القياسات المطلوبة.
