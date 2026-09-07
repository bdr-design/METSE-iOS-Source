# METSE 0.1.4 / Build 006 — Character & Camera Foundation

## الهدف
تحويل الحركة من كاميرا حرة إلى لاعب تكتيكي له حالة فيزيائية واضحة داخل C++، مع إبقاء Metal طبقة عرض فقط وSwift طبقة إدخال/UI فقط.

## Character Motor
- fixed-step عند 60Hz داخل EngineCore.
- موضع X/Y/Z وسرعة X/Y/Z.
- تسارع أرضي وتباطؤ مستقل بدل القفز الفوري إلى السرعة القصوى.
- Jog وSprint وسرعات مستقلة للانحناء والانبطاح.
- تقليل سرعة الرجوع للخلف.
- جاذبية وتماس fail-safe مع ground plane عند Y=0 كبداية قبل collision هندسي كامل.
- لا allocation دوري داخل fixedStep.

## Stance
Standing -> Crouched -> Prone -> Standing. لكل وضعية سرعة وارتفاع عين مستقل، والانتقال في ارتفاع الكاميرا تدريجي.

## Camera / Body separation
bodyYaw لاتجاه الجسم وviewYawOffset لالتفات النظر. يوجد soft limit يبدأ بعده الجسم باللحاق بالنظر، وhard limit يمنع التفاف الكاميرا بلا حدود. pitch محدود ومتحقق منه داخل invariants.

## Control Plane
Movement/Look/Sprint/Stance intents تمر عبر executeAtomic مع reject/rollback والجورنال نفسه.

## Black Box
أضيف Y position وvelocity XYZ وbody/camera yaw وcamera height وhorizontal speed وstance وgrounded وsprinting.

## ما لم ينفذ بعد
Capsule/world collision، jump/vault/mantle، Foot IK/animation، Weapon rig/ADS/recoil، وTerrain geometry حقيقي. هذه تأتي بعد تثبيت Character Motor على الجهاز.
