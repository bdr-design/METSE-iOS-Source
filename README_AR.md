# METSE — Middle East Tactical Simulation Engine

Native iPhone tactical engine: Swift/UIKit shell → Objective-C++ adapter → portable C++20 simulation → Metal renderer.

## الحالة الحالية
- Version 0.3.0
- Build 008 — Mega Combat Foundation
- 32 combatants max
- Single simulation ownership + bounded input queue
- Character/3D collision + camera feel
- Weapon/ADS/recoil/reload/obstruction
- Ballistics/projectiles/material impact
- Anatomical damage foundation
- Visibility/Culling budgets
- Observatory V2 + Black Box + SHA journal + deterministic state hash
- Thermal presentation fallback 60→30 مع بقاء simulation fixed-step 60Hz
- Native Metal indexed-mesh viewmodel مع depth وإضاءة وأصل M4A1 هندسي قابل للاستبدال

لا توجد تبعية WebView أو Unity/Unreal داخل Runtime اللعبة.
