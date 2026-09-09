import Foundation

@main
enum DiagnosticArchiveTests {
    static func main() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("metse-archive-tests-" + UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        let archive = METSEDiagnosticArchive(directory: root)
        func record(_ text: String, phase: METSEDiagnosticRecord.Phase = .active) -> METSEDiagnosticRecord {
            METSEDiagnosticRecord(schema: 1, launchID: UUID().uuidString, phase: phase, checkpointAt: 100, reportCapturedAt: 90, report: text)
        }
        let missing = try archive.read(); assert(missing == nil)
        let first = try archive.begin(record("first")); assert(first == nil)
        try archive.save(record("checkpoint", phase: .background))
        let prior = try archive.begin(record("second"))
        assert(prior?.report == "checkpoint" && prior?.phase == .background)
        let previous = try archive.read(previous: true); assert(previous?.report == "checkpoint")
        for _ in 0..<100 { try archive.save(record("bounded")) }
        let files = try FileManager.default.contentsOfDirectory(atPath: root.path); assert(files.count == 2)
        do {
            try archive.save(record(String(repeating: "x", count: METSEDiagnosticArchive.byteCap + 1)))
            assertionFailure("oversized report accepted")
        } catch METSEDiagnosticArchive.Failure.oversized { }
        let retained = try archive.read(); assert(retained?.report == "bounded")
        // Invalid existing evidence must not be silently overwritten on launch.
        try Data("invalid".utf8).write(to: root.appendingPathComponent("current.json"), options: .atomic)
        do { _ = try archive.begin(record("replacement")); assertionFailure("corruption ignored") }
        catch { }
        let corrupt = try String(contentsOf: root.appendingPathComponent("current.json"), encoding: .utf8)
        assert(corrupt == "invalid")
        print("010-F bounded atomic diagnostic archive: PASS")
    }
}
