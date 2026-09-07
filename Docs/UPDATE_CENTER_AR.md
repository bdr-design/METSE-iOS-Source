# METSE Update Center — V0.1

## الهدف
تحديث محتوى وبيانات اللعبة أثناء التطوير من داخل التطبيق، بدون إعادة توقيع IPA لكل تعديل محتوى.

## حد أمان صريح
المركز لا ينزّل ولا ينفّذ C++ أو dylib أو أي كود أصلي. أي تغيير في Core/Plugins/Native iOS يحتاج IPA جديد.

## المسار
HTTPS manifest -> compatibility gate -> download staging -> size check -> SHA-256 -> atomic file commit -> Unreal MountPak -> active state.

## Fail Closed
- لا HTTPS: رفض.
- Manifest ناقص: رفض.
- Core build مختلف: رفض.
- Content schema مختلف: رفض.
- SHA-256 خطأ: رفض وحذف staged package.
- Mount handler غير موجود: رفض.
- Mount يرجع nullptr: رفض.
- لا يتم تغيير ActiveContentVersion إلا بعد نجاح mount.

## Rollback
يحفظ النظام active/previous package paths. Rollback لا ينجح إلا إذا الحزمة السابقة موجودة ويمكن Mount لها فعلياً.

## قيد V0.1
التنزيل الحالي في الذاكرة ومحدود إلى 256 MiB. هذا مقصود للنسخة الأولية. قبل الحزم الكبيرة سنحوّله إلى streamed/chunked download مع resume.
