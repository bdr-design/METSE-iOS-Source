# METSE — Native Metal Baseline

هذه هي قاعدة METSE الجديدة بعد فصل المحرك عن واجهة التطبيق.

## البنية

- `iOS/METSE/` — App Shell وبوابة الدخول وواجهات النظام.
- `Engine/Core/` — محرك المحاكاة C++ بلا UIKit/Metal.
- `Engine/Platform/Apple/` — جسر Objective-C++ بين iOS والمحرك.
- `Shaders/` — Metal shaders.
- `Content/` — بيانات قابلة للتحديث بدون تغيير الكود.
- `Scripts/` — الجدار الوقائي وبناء GitHub.
- `Tests/` — اختبارات المحرك المستقلة.

## قاعدة الملكية

Swift لا يملك simulation state.
Metal لا يملك gameplay state.
C++ Core لا يعرف UIKit أو Xcode.
Update Center لا ينزل أي كود تنفيذي؛ فقط Content/Data متوافق مع schema.

## بناء iPhone

GitHub Actions على `macos-15`:
Source integrity → Guardrails → C++ tests → Swift parse → XcodeGen → xcodebuild iphoneos بدون توقيع → Payload/METSE.app → IPA unsigned.

لا تحتاج شهادة داخل GitHub. بعد نجاح الـArtifact تنقل IPA وتوقعه بشهادتك.
