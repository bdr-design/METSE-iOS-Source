#!/usr/bin/env python3
"""Build 012-X: in-app crash reporter, installed before anything else at launch."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
errors = []


def req(condition, message):
    if not condition:
        errors.append(message)


def text(path):
    file = ROOT / path
    return file.read_text(errors="ignore") if file.exists() else ""


reporter = text("iOS/METSE/METSECrashReporter.swift")
viewer = text("iOS/METSE/METSECrashReportViewController.swift")
app_delegate = text("iOS/METSE/AppDelegate.swift")
gateway = text("iOS/METSE/GatewayViewController.swift")

req(reporter != "", "METSECrashReporter.swift missing")
req(viewer != "", "METSECrashReportViewController.swift missing")

req("NSSetUncaughtExceptionHandler" in reporter, "must catch uncaught NSException crashes")
for sig in ("SIGABRT", "SIGSEGV", "SIGBUS", "SIGILL", "SIGFPE", "SIGTRAP"):
    req(sig in reporter, f"must install a handler for {sig} - NSException alone misses memory-access crashes")
req("backtrace_symbols_fd" in reporter, "signal path must capture a real backtrace, not just the signal number")
req("@convention(c)" in reporter, "the signal handler must be a true C function pointer, not a capturing Swift closure")

# Installation ordering: must be the very first thing AppDelegate does, so it can
# observe crashes from anything that runs after it, including engine/renderer init.
install_index = app_delegate.find("METSECrashReporter.install()")
recorder_index = app_delegate.find("METSEDiagnosticRecorder.shared.start()")
req(install_index != -1 and recorder_index != -1 and install_index < recorder_index,
    "METSECrashReporter.install() must run before anything else in AppDelegate")

req("consumeLastCrashIfAny" in gateway and "METSECrashReportViewController" in gateway,
    "Gateway must surface a previous crash automatically on next launch")

if errors:
    print("METSE BUILD 012-X CRASH REPORTER GUARDRAILS: FAIL")
    for error in errors:
        print(" -", error)
    sys.exit(1)
print("METSE BUILD 012-X CRASH REPORTER GUARDRAILS: PASS")
