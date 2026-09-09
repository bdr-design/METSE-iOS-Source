import UIKit

// Main-thread admission; one I/O job plus one latest-wins pending checkpoint.
final class METSEDiagnosticRecorder {
    static let shared = METSEDiagnosticRecorder()
    private let queue = DispatchQueue(label: "metse.diagnostics.disk", qos: .utility)
    private let archive: METSEDiagnosticArchive
    private let launchID = UUID().uuidString
    private var started = false, busy = false, initialized = false
    private var pending: METSEDiagnosticRecord?
    private var phase = METSEDiagnosticRecord.Phase.inactive
    private var cachedReport = "", capturedAt: TimeInterval?
    private(set) var previousText = "التقرير السابق لم يُحمّل بعد."
    private(set) var status = "لم يبدأ الحفظ"
    private(set) var coalesced: UInt64 = 0

    private init() {
        let root = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        archive = METSEDiagnosticArchive(directory: root.appendingPathComponent("METSEDiagnostics", isDirectory: true))
    }
    func start() {
        precondition(Thread.isMainThread)
        guard !started else { return }; started = true
        let center = NotificationCenter.default
        center.addObserver(self, selector: #selector(background), name: UIApplication.didEnterBackgroundNotification, object: nil)
        center.addObserver(self, selector: #selector(active), name: UIApplication.didBecomeActiveNotification, object: nil)
        center.addObserver(self, selector: #selector(inactive), name: UIApplication.willResignActiveNotification, object: nil)
        checkpoint()
    }
    @objc private func background() { phase = .background; checkpoint() }
    @objc private func active() { phase = .active; checkpoint() }
    @objc private func inactive() { phase = .inactive; checkpoint() }
    func capture(_ report: String) {
        precondition(Thread.isMainThread)
        guard report.utf8.count <= METSEDiagnosticArchive.byteCap else { status = "فشل: التقرير تجاوز الحد"; return }
        cachedReport = report; capturedAt = Date().timeIntervalSince1970
        checkpoint()
    }
    func endGameplay() { precondition(Thread.isMainThread); phase = .sessionEnded; checkpoint() }
    private func checkpoint() {
        precondition(Thread.isMainThread)
        if pending != nil { coalesced &+= 1 }
        pending = METSEDiagnosticRecord(schema: 1, launchID: launchID, phase: phase,
            checkpointAt: Date().timeIntervalSince1970, reportCapturedAt: capturedAt, report: cachedReport)
        pump()
    }
    private func pump() {
        guard !busy, let record = pending else { return }
        pending = nil; busy = true
        let needsBegin = !initialized
        queue.async { [self] in
            var previous: METSEDiagnosticRecord?
            var failure: String?
            do {
                if needsBegin { previous = try archive.begin(record) }
                else { try archive.save(record) }
            } catch { failure = String(describing: error) }
            let prior = previous, problem = failure
            DispatchQueue.main.async { [self] in
                busy = false
                if let problem { status = "فشل حفظ التشخيص: \(problem)" }
                else {
                    initialized = true; status = "حُفظت آخر لقطة؛ لا ضمان لحفظ لحظة الانهيار"
                    if needsBegin {
                        if let prior {
                            previousText = "Previous launch \(prior.launchID)\nLast persisted phase: \(prior.phase.rawValue) — not proof of crash or clean exit\nCheckpoint \(prior.checkpointAt) / report captured \(prior.reportCapturedAt.map(String.init(describing:)) ?? "unknown")\n" + prior.report
                        } else { previousText = "لا يوجد تقرير سابق محفوظ." }
                    }
                }
                // No retry loop on failure; next real checkpoint may retry.
                if problem == nil { pump() }
            }
        }
    }
}
