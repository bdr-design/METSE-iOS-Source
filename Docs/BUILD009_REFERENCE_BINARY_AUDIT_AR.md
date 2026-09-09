# تدقيق المرجع الثنائي المرفوع — My Files1/2/3

تمت مراجعة الأجزاء الثلاثة بعد إشارة المستخدم إلى اكتمال الرفع. هذه الملفات ليست مصدر METSE النصي؛ هي أجزاء مسطحة من حزمة iOS مبنية بمحرك Unity، ولا تحتوي على مسارات المصدر الأصلية أو manifest كامل يثبت إعادة البناء.

## هوية الأجزاء وسلامتها

| الجزء | SHA-256 للأرشيف | فحص ZIP |
|---|---|---|
| `My Files1 (1).zip` | `2698c677f3898b9e347d35bda386bbddc793b4a0f6554b034ceda19deefcca80` | CRC PASS |
| `My Files2 (1).zip` | `ec00895fa55c9cc2486443c1d23b77ccf7d2d6b1176d309a172490406aecfb02` | CRC PASS |
| `My Files3 (1).zip` | `81ed0d0c5287f0ec5d877a6114217e7576712b025f603d07816f8d9c2e823b0d` | CRC PASS |

الأسماء والمحتويات تثبت وجود `Harekat2` و`UnityFramework` و`global-metadata.dat` وملفات Unity Data/Scenes. يظهر في الـbinary bundle identifier `com.devlaps.harekat2`، بينما `Info.plist` المتاح في الجزء الثاني يخص إطار `iGameGod` بإصدار `0.4.4` وليس إصدار اللعبة.

الأجزاء ليست split-volume ZIP متسلسلًا يمكن دمجه بأمان: كل ملف ZIP مستقل ومساراته مسطحة. مجموعها 80 entry فقط، بينما `CodeResources` في الجزء الأول يتوقع 107 ملفات للتطبيق. توجد أسماء متكررة مع محتوى مختلف، منها `Assets.car` و`Info.plist`، ونسخ `GoogleService-Info.plist` متطابقة في جزأين ومختلفة في الثالث. لذلك لا توجد طريقة آمنة لإعادة بناء IPA موثوق من هذه الأجزاء دون المسارات الأصلية وmanifest كامل.

## حدود الاعتماد الهندسي

لا يتم نسخ أي binary أو Unity asset أو Firebase configuration أو dylib إلى METSE. معماريًا، يبقى METSE Native iPhone (Swift/UIKit + Objective-C++ bridge + portable C++20 core + Metal)، ولا يتحول إلى Unity أو WebView أو Core موازٍ. هذه الحزمة ليست **مصدر حقيقة** لحالة اللعب أو الزمن أو الضرر، ولا يمكن منها إثبات سلوك gameplay أو إصلاحه دون المصدر والاختبارات.

## خطر التلوث/الحقن

الأجزاء تحتوي بوضوح على:

- `CydiaSubstrate` و`SubstrateLoader/Launcher/Injection`.
- `iGameGod` و`Harekat2Online.dylib`.
- رموز memory search/patch، fishhook، Swift hooks، و`MSHookFunction`.
- اعتمادًا صريحًا على `@executable_path/Frameworks/Harekat2Online.dylib` وCydiaSubstrate.

هذا يعني أن الحزمة ليست مرجع production نظيفًا؛ قد تكون معدلة أو محقونة على جهاز jailbroken. لا يجوز اعتبار logs أو النتائج الصادرة منها دليلًا على أخطاء METSE، ولا يجوز إدخال هذه المكونات في IPA أو المشروع. أي artifact إنتاجي يجب أن يمر بفحص الاعتماديات والتوقيع وSource Integrity على نفس commit.

كما توجد ملفات Firebase تتضمن معرفات ومفاتيح إعداد عميلة. لم تُنسخ القيم هنا، ولا تُعاد تعبئتها في METSE. إذا كانت مرتبطة بخدمة حية، يجب تقييدها/تدويرها وفق سياسة مالك المشروع.

## ملاحظات هندسية مفيدة تم اعتمادها بأمان

الحزمة توفر مؤشرات تصميمية فقط، منها وجود CrashReporter/Crashlytics وProfilerRecorder وSupportLogger وmemory-pressure callbacks (أي **memory pressure** events)، إضافة إلى Photon/Firebase وNavMesh وanti-cheat. هذه ليست دعوة لإضافة Networking أو Unity إلى METSE؛ الاستفادة المحدودة الآمنة هي تحسين مركز الرصد ليُظهر:

1. تحذيرات ضغط الذاكرة كحادث مستقل لا يختلط مع thermal fallback أو callback gap.
2. حدود lifecycle typed: resign-active، background، foreground، وactive بدل عداد واحد مبهم.
3. فصل مشكلة مؤكدة عن coverage ناقصة أو runtime خارجي محقون.

تم تنفيذ هذه النقاط في Bridge/Observatory V4 مع عدادات bounded، وmask مستقل `memory=0x400`، وظهورها في Snapshot والتقرير وواجهة مركز الرصد. لا يتغير gameplay truth ولا fixed simulation 60 Hz.

## قرار الدمج

المرجع مفيد كدليل تدقيق ثنائي وحدود أمنية، لكنه ليس بديلًا عن مصدر METSE ولا يُدمج ككود أو أصول. أي ادعاء عن إصلاح عطل يجب أن يستند إلى مصدر METSE، اختبار regression، وartifact مبني من نفس SHA.
