# بناء iOS

## المتطلبات
- macOS runner خاص بك.
- Unreal Engine 5.8 مثبت.
- Xcode المتوافق مع UE 5.8.
- GitHub Actions variable باسم `UE_ROOT` يشير إلى جذر Unreal.

## المسار
`main -> verified-source-gates -> macOS preflight -> Unreal Automation -> BuildCookRun -> unsigned IPA -> Artifacts`

المسار Fail Closed. إذا لم يعلن إصدار Unreal المثبت عن خيار no-code-sign في BuildCookRun، يرفض السكربت إنشاء IPA بدل إنتاج ملف وهمي.
