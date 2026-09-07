# نطاق METSE V0.1 Build 001

## موجود الآن
- Unreal Engine 5.8 C++ foundation.
- Character movement foundation بدون Tick مخصص.
- MovementIntent مضغوط ومهيأ للشبكة.
- Health replication والضرر server-authoritative.
- Performance Governor وWorld budget بحد 32 مقاتلاً.
- Update Center لمحتوى PAK عبر HTTPS مع SHA-256 وCompatibility/Schema gates وStaging وRollback.
- GitHub source gates وmacOS iOS build scripts.

## حدود مقصودة
- لا أصول رسومية أو Motion Matching assets بعد.
- لا Ballistics/AI/Vehicles كاملة بعد.
- لا ادعاء بأن C++ اجتاز UE compile حتى يعمل على runner فيه UE 5.8 فعليًا.
- تحديثات C++/Native لا تمر عبر Update Center وتحتاج IPA جديد.
