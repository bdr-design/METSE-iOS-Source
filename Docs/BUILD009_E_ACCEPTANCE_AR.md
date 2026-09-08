# Build 009-E Acceptance Gate

009-E is accepted only when one commit passes all of the following without bypasses:

1. Source Integrity SHA-256 verification.
2. Existing Build 009 architecture guardrails.
3. Build 009-E action/cover guardrails.
4. All C++ tests with C++20 `-Wall -Wextra -Wpedantic -Werror`.
5. Swift syntax gate.
6. Metal compiler gate.
7. Xcode iPhoneOS platform compile with signing disabled.
8. Native unsigned IPA build.

A failed gate is repaired at the source and the real manifest is resealed. No guardrail is weakened to make the build green.
