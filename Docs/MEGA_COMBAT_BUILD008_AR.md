# METSE 0.3.0 — Build 008 Mega Combat Foundation

هذه الحزمة تعيد بناء أساس القتال من الصفر داخل C++/Metal. ملفات Harekat 2 المرفوعة استُخدمت كمرجع تدقيقي معماري فقط، ولم يُنقل منها كود أو Asset.

## الملكية والتنفيذ
- Render callback هو مالك تقدم المحاكاة.
- UIKit يضع input typed في InputCommandQueue bounded (64) ولا يعدل gameplay state مباشرة.
- Move/Look/Sprint/ADS قابلة للـcoalescing مع حواجز ترتيب أمام Fire/Reload/Stance.
- Fire/Reload/Stance تمر عبر atomic checkpoint + invariant validation + commit/rollback + journal.

## Character / Camera
- Walk/Tactical/Jog/Sprint/Crouch/Crawl.
- acceleration/deceleration + gravity + landing + bob/roll/acceleration lean.
- 3D capsule clearance ومنع الوقوف تحت سقف منخفض.

## Weapon / Ballistics / Damage
- 30/90، 700 RPM، reload، ADS، recoil، sway، obstruction.
- Height-over-bore مع convergence 100m.
- 128 projectile fixed-capacity؛ continuous segment collision، gravity/drag.
- Concrete/Steel/Wood وpenetration أولي للخشب فقط حسب الطاقة.
- Hit regions: Head/Thorax/Abdomen/Limb؛ health/armor thresholds؛ correlation ID عبر shot-impact-damage.

## Visibility / Performance
- سقف 32 entity.
- Full/Reduced/Minimal/Dormant presentation/simulation-budget tiers فقط؛ لا تخفيض لدقة ballistics/damage.
- Thermal fallback يغيّر presentation 60→30 عند serious/critical بينما fixed-step يبقى 60Hz.

## Observatory V2
- AVG/P95/P99/MAX و1% Low و0.1% Low.
- Input queue، weapon، projectiles، impacts، damage، visibility، integrity، renderer، thermal.
- Deterministic State Hash + SHA journal + Black Box.
