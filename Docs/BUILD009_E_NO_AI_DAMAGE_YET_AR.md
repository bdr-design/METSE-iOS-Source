# 009-E AI Damage Boundary

`fireAuthorized` means the tactical agent has fresh visual provenance, a valid action, a ready WeaponCore, muzzle clearance and an unobstructed ballistic probe. It does not spawn a projectile and it does not damage the player.

Production AI damage requires a later combatant-targeting contract defining player damage state, identity/team/faction and friendly-fire filtering before any `WeaponCore::fire -> BallisticsCore::spawn` path is enabled for AI.
