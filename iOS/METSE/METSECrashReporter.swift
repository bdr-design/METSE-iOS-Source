import Foundation
#if canImport(Darwin)
import Darwin
#endif

/// Catches crashes the OS itself doesn't hand back to the app - installed as the very
/// first thing at launch (see AppDelegate). Two independent paths, because most real
/// crashes are NOT Swift/ObjC exceptions:
///   1. NSSetUncaughtExceptionHandler - Objective-C/Swift runtime exceptions (invalid
///      argument, "attempted to access privacy-sensitive data without a usage
///      description", etc). Runs in a mostly-normal process state, so normal
///      Foundation calls (String, file writes) are fine here.
///   2. POSIX signal handlers - SIGSEGV/SIGBUS/SIGILL/SIGABRT/SIGFPE/SIGTRAP, which
///      covers memory-access crashes (EXC_BAD_ACCESS et al) that NSException never
///      sees at all. Signal handlers are NOT a safe place for arbitrary Swift/malloc
///      work in the strictest sense; this keeps that path as small as practical and
///      leans on backtrace_symbols_fd (documented as usable without malloc) for the
///      actual stack. This is a best-effort debugging aid, not a shipped production
///      crash reporter - good enough to stop needing Xcode's device log window for
///      day-to-day iteration.
enum METSECrashReporter {
    private static var crashFD: Int32 = -1
    private static let crashFileURL: URL = {
        let root = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        let dir = root.appendingPathComponent("METSEDiagnostics", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir.appendingPathComponent("last_crash.txt")
    }()

    static func install() {
        crashFD = open(crashFileURL.path, O_WRONLY | O_CREAT | O_TRUNC, 0o644)
        NSSetUncaughtExceptionHandler { exception in
            METSECrashReporter.writeExceptionCrash(exception)
        }
        for signalValue in [SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP] {
            signal(signalValue, METSECrashReporter.handleSignal)
        }
    }

    /// Call once at launch, before anything else reads diagnostics. Returns the crash
    /// text and deletes the file (one-shot: shown once, then gone), or nil if the
    /// previous launch exited without hitting either handler above.
    static func consumeLastCrashIfAny() -> String? {
        guard let data = try? Data(contentsOf: crashFileURL), !data.isEmpty,
              let text = String(data: data, encoding: .utf8) else { return nil }
        try? FileManager.default.removeItem(at: crashFileURL)
        return text
    }

    private static func writeExceptionCrash(_ exception: NSException) {
        let text = """
        METSE UNCAUGHT EXCEPTION
        name: \(exception.name.rawValue)
        reason: \(exception.reason ?? "unknown")
        stack:
        \(exception.callStackSymbols.joined(separator: "\n"))
        """
        try? text.data(using: .utf8)?.write(to: crashFileURL, options: .atomic)
    }

    private static let handleSignal: @convention(c) (Int32) -> Void = { signalValue in
        let fd = METSECrashReporter.crashFD
        guard fd >= 0 else { _exit(1) }
        let header = "METSE SIGNAL CRASH\nsignal: \(signalValue)\nstack:\n"
        header.utf8CString.withUnsafeBufferPointer { buffer in
            if let base = buffer.baseAddress { _ = write(fd, base, strlen(base)) }
        }
        var callstack = [UnsafeMutableRawPointer?](repeating: nil, count: 64)
        let count = backtrace(&callstack, 64)
        backtrace_symbols_fd(&callstack, count, fd)
        signal(signalValue, SIG_DFL)
        raise(signalValue)
    }
}
