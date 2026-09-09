import Foundation

// Diagnostic evidence only: never read this archive into gameplay state.
struct METSEDiagnosticRecord: Codable {
    enum Phase: String, Codable { case active, background, inactive, sessionEnded }
    let schema: Int
    let launchID: String
    let phase: Phase
    let checkpointAt: TimeInterval
    let reportCapturedAt: TimeInterval?
    let report: String
}

final class METSEDiagnosticArchive {
    static let byteCap = 128 * 1024
    enum Failure: Error { case oversized, invalidRecord }
    private let directory: URL
    init(directory: URL) { self.directory = directory }
    private func url(_ previous: Bool) -> URL {
        directory.appendingPathComponent(previous ? "previous.json" : "current.json")
    }
    func read(previous: Bool = false) throws -> METSEDiagnosticRecord? {
        let file = url(previous)
        guard FileManager.default.fileExists(atPath: file.path) else { return nil }
        let attributes = try FileManager.default.attributesOfItem(atPath: file.path)
        guard let size = attributes[.size] as? NSNumber, size.intValue <= Self.byteCap else { throw Failure.oversized }
        let data = try Data(contentsOf: file)
        guard data.count <= Self.byteCap else { throw Failure.oversized }
        let record = try JSONDecoder().decode(METSEDiagnosticRecord.self, from: data)
        try validate(record)
        return record
    }
    private func validate(_ record: METSEDiagnosticRecord) throws {
        guard record.schema == 1, UUID(uuidString: record.launchID) != nil,
              record.checkpointAt.isFinite, record.checkpointAt >= 0,
              record.reportCapturedAt.map({ $0.isFinite && $0 >= 0 }) ?? true else { throw Failure.invalidRecord }
        guard record.report.utf8.count <= Self.byteCap else { throw Failure.oversized }
    }
    private func write(_ record: METSEDiagnosticRecord, previous: Bool) throws {
        try validate(record)
        let data = try JSONEncoder().encode(record)
        guard data.count <= Self.byteCap else { throw Failure.oversized }
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        var folder = directory
        var values = URLResourceValues(); values.isExcludedFromBackup = true
        try folder.setResourceValues(values)
        #if os(iOS)
        try data.write(to: url(previous), options: [.atomic, .completeFileProtectionUntilFirstUserAuthentication])
        #else
        try data.write(to: url(previous), options: .atomic)
        #endif
    }
    // Each file is atomically replaced. This is not a two-file transaction.
    // A failed rotation leaves current evidence readable; never silently clear it.
    func begin(_ record: METSEDiagnosticRecord) throws -> METSEDiagnosticRecord? {
        try validate(record)
        let prior = try read()
        var fallback: METSEDiagnosticRecord?
        if prior == nil { fallback = try read(previous: true) }
        if let prior { try write(prior, previous: true) }
        try write(record, previous: false)
        if let prior { return prior }
        return fallback
    }
    func save(_ record: METSEDiagnosticRecord) throws { try write(record, previous: false) }
}
