# Build 009-E Regression Matrix

| Contract | Regression |
|---|---|
| Wall blocks visual awareness | `Wall blocks player` |
| Cover candidates come from WorldCollision | `Cover truth` |
| Overhead-only geometry is invalid cover | `Cover truth` |
| No firing through cover | `No firing through cover` |
| Lost LOS uses previous last-known only | `Lost LOS` |
| AI reload uses WeaponCore | `AI reload action` |
| Incapacitated agent executes no action | `Incapacitated-but-alive` |
| TacticalAI transform mirrors one-way into Damage | `one-way hit-target mirror` |
| Decision work is bounded | `32-agent stress` |
| Repeated state/input is deterministic | `Repeated identical scenarios` |

All tests compile with C++20 and `-Wall -Wextra -Wpedantic -Werror` through `Scripts/test_engine_core.sh`.
