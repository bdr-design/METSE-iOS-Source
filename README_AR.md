# METSE v0.1.0 — Foundation Build 001

المصدر الأساسي لمحاكي حرب تكتيكي عالي الدقة للـiPhone، مخصص للشرق الأوسط، وبحد أقصى 32 مقاتلاً إجمالاً.

## مبادئ ثابتة
- iPhone-first وSustained Performance للجلسات الطويلة.
- 60 FPS هدف، 30 FPS وضع حماية.
- لا Tick عشوائي ولا تجاوز لفشل الاختبارات.
- Fail Closed: أي بوابة تحقق تفشل تمنع اعتماد البناء.
- تحديث المحتوى فقط من داخل التطبيق؛ تغييرات C++ تحتاج IPA جديد.

## مركز التحديث
`UMETSEUpdateCenterSubsystem` يمر عبر HTTPS -> Manifest validation -> Size -> SHA-256 -> Staging -> Atomic commit -> MountPak -> Active. أي فشل يبقي المحتوى الحالي كما هو.

## بناء iOS
المسار المعتمد: `main -> source gates -> macOS/Xcode/UE5.8 -> tests -> unsigned IPA -> Artifacts -> توقيع المستخدم`.
