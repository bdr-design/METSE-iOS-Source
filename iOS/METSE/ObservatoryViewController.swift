import UIKit

final class ObservatoryViewController: UIViewController {
    private let engine: METSEEngineBridge
    private var timer: Timer?
    private let healthLabel = UILabel()
    private let runtimeBody = UILabel(), frameBody = UILabel(), inputBody = UILabel(), weaponBody = UILabel(), combatBody = UILabel(), worldBody = UILabel(), integrityBody = UILabel(), rendererBody = UILabel()

    init(engine: METSEEngineBridge) { self.engine = engine; super.init(nibName: nil, bundle: nil); modalPresentationStyle = .overFullScreen }
    @available(*, unavailable) required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
    override var prefersStatusBarHidden: Bool { true }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .landscape }
    override func viewDidLoad() { super.viewDidLoad(); configureUI(); refresh() }
    override func viewWillAppear(_ animated: Bool) { super.viewWillAppear(animated); startTimer() }
    override func viewWillDisappear(_ animated: Bool) { super.viewWillDisappear(animated); timer?.invalidate(); timer = nil }
    deinit { timer?.invalidate() }

    private func configureUI() {
        view.backgroundColor = UIColor(red: 0.008, green: 0.015, blue: 0.016, alpha: 0.98)
        let close = UIButton(type: .system); close.setImage(UIImage(systemName: "xmark"), for: .normal); close.tintColor = .white; close.backgroundColor = UIColor.white.withAlphaComponent(0.08); close.layer.cornerRadius = 20; close.addAction(UIAction { [weak self] _ in self?.dismiss(animated: true) }, for: .touchUpInside); close.widthAnchor.constraint(equalToConstant: 40).isActive = true; close.heightAnchor.constraint(equalToConstant: 40).isActive = true
        let title = UILabel(); title.text = "مركز الأرصاد V2"; title.font = .systemFont(ofSize: 24, weight: .bold); title.textColor = .white
        let subtitle = UILabel(); subtitle.text = "رصد Runtime / Input Queue / Weapon / Ballistics / Damage / Culling / Integrity / Thermal"; subtitle.font = .monospacedSystemFont(ofSize: 9.5, weight: .medium); subtitle.textColor = UIColor.white.withAlphaComponent(0.55)
        let titleStack = UIStackView(arrangedSubviews: [title, subtitle]); titleStack.axis = .vertical; titleStack.spacing = 2
        healthLabel.font = .monospacedSystemFont(ofSize: 10.5, weight: .bold); healthLabel.textAlignment = .center; healthLabel.layer.cornerRadius = 12; healthLabel.layer.masksToBounds = true; healthLabel.widthAnchor.constraint(greaterThanOrEqualToConstant: 210).isActive = true; healthLabel.heightAnchor.constraint(equalToConstant: 32).isActive = true
        let header = UIStackView(arrangedSubviews: [close, titleStack, UIView(), healthLabel]); header.axis = .horizontal; header.alignment = .center; header.spacing = 12

        let rows = [
            row(card("Runtime", "cpu", runtimeBody), card("Frame Performance", "gauge.with.dots.needle.50percent", frameBody)),
            row(card("Input Ownership", "rectangle.stack.badge.play", inputBody), card("Weapon", "scope", weaponBody)),
            row(card("Combat Pipeline", "bolt.horizontal.circle", combatBody), card("World / Visibility", "square.3.layers.3d", worldBody)),
            row(card("Integrity", "checkmark.shield", integrityBody), card("Renderer / Thermal", "display", rendererBody))
        ]
        let copy = actionButton("نسخ التقرير", "doc.on.doc"); copy.addAction(UIAction { [weak self, weak copy] _ in guard let self else { return }; UIPasteboard.general.string = self.fullReportText(); var updated = copy?.configuration; updated?.title = "تم النسخ"; copy?.configuration = updated }, for: .touchUpInside)
        let share = actionButton("مشاركة التقرير", "square.and.arrow.up"); share.addAction(UIAction { [weak self, weak share] _ in guard let self, let share else { return }; let a = UIActivityViewController(activityItems: [self.fullReportText()], applicationActivities: nil); a.popoverPresentationController?.sourceView = share; self.present(a, animated: true) }, for: .touchUpInside)
        let actions = row(copy, share)
        let content = UIStackView(arrangedSubviews: rows + [actions]); content.axis = .vertical; content.spacing = 10
        let scroll = UIScrollView(); scroll.alwaysBounceVertical = true; scroll.addSubview(content); content.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(header); view.addSubview(scroll); header.translatesAutoresizingMaskIntoConstraints = false; scroll.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([
            header.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 18), header.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -18), header.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 10),
            scroll.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 18), scroll.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -18), scroll.topAnchor.constraint(equalTo: header.bottomAnchor, constant: 10), scroll.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -8),
            content.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor), content.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor), content.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor), content.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor), content.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor)
        ])
    }

    private func row(_ a: UIView, _ b: UIView) -> UIStackView { let r = UIStackView(arrangedSubviews: [a,b]); r.axis = .horizontal; r.spacing = 10; r.distribution = .fillEqually; return r }
    private func card(_ title: String, _ symbol: String, _ body: UILabel) -> UIView {
        let box = UIView(); box.backgroundColor = UIColor.white.withAlphaComponent(0.055); box.layer.cornerRadius = 15; box.layer.borderWidth = 1; box.layer.borderColor = UIColor.white.withAlphaComponent(0.06).cgColor
        let icon = UIImageView(image: UIImage(systemName: symbol)); icon.tintColor = UIColor(red: 0.45, green: 0.88, blue: 0.69, alpha: 1); icon.widthAnchor.constraint(equalToConstant: 18).isActive = true; icon.heightAnchor.constraint(equalToConstant: 18).isActive = true
        let heading = UILabel(); heading.text = title; heading.font = .systemFont(ofSize: 12.5, weight: .semibold); heading.textColor = UIColor.white.withAlphaComponent(0.84)
        let top = UIStackView(arrangedSubviews: [icon, heading, UIView()]); top.axis = .horizontal; top.spacing = 8; top.alignment = .center
        body.font = .monospacedSystemFont(ofSize: 9.8, weight: .regular); body.textColor = UIColor.white.withAlphaComponent(0.73); body.numberOfLines = 0
        let stack = UIStackView(arrangedSubviews: [top, body]); stack.axis = .vertical; stack.spacing = 7; box.addSubview(stack); stack.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([stack.leadingAnchor.constraint(equalTo: box.leadingAnchor, constant: 13), stack.trailingAnchor.constraint(equalTo: box.trailingAnchor, constant: -13), stack.topAnchor.constraint(equalTo: box.topAnchor, constant: 10), stack.bottomAnchor.constraint(equalTo: box.bottomAnchor, constant: -10), box.heightAnchor.constraint(greaterThanOrEqualToConstant: 105)])
        return box
    }
    private func actionButton(_ title: String, _ symbol: String) -> UIButton { var c = UIButton.Configuration.filled(); c.title = title; c.image = UIImage(systemName: symbol); c.imagePadding = 8; c.cornerStyle = .medium; c.baseBackgroundColor = UIColor(red: 0.16, green: 0.42, blue: 0.34, alpha: 1); c.baseForegroundColor = .white; let b = UIButton(configuration: c); b.heightAnchor.constraint(equalToConstant: 44).isActive = true; return b }

    private func startTimer() { timer?.invalidate(); timer = Timer.scheduledTimer(withTimeInterval: 0.75, repeats: true) { [weak self] _ in self?.refresh() }; if let timer { RunLoop.main.add(timer, forMode: .common) } }
    private func refresh() {
        let s = engine.observatorySnapshot(); let thermal = thermalDescription()
        let validKeys = ["journalValid","worldValid","observatoryValid","queueValid","weaponValid","ballisticsValid","damageValid","visibilityValid"]
        let healthy = validKeys.allSatisfy { bool(s,$0) } && thermal.level < 2
        healthLabel.text = healthy ? "● SYSTEM NOMINAL • \(thermal.name)" : "● ATTENTION • \(thermal.name)"; healthLabel.textColor = healthy ? UIColor(red: 0.53, green: 0.94, blue: 0.71, alpha: 1) : UIColor(red: 1.0, green: 0.73, blue: 0.34, alpha: 1); healthLabel.backgroundColor = healthLabel.textColor.withAlphaComponent(0.10)
        runtimeBody.text = String(format: "Build %@ • v%@\nPresentation %@ FPS / Simulation 60Hz\nTick %@ • %.1fs\nBB %@", str(s,"build"),str(s,"version"),str(s,"presentationFPS"),str(s,"simulationTick"),num(s,"simulationSeconds"),str(s,"blackBoxFrames"))
        frameBody.text = String(format: "FPS %.1f • AVG %.2fms\nP95 %.2f • P99 %.2f • MAX %.2f\n1%% LOW %.1f • 0.1%% LOW %.1f\n>20ms %@ • >33ms %@ • clamp %@", num(s,"estimatedFPS"),num(s,"averageFrameMs"),num(s,"p95FrameMs"),num(s,"p99FrameMs"),num(s,"maxFrameMs"),num(s,"onePercentLowFPS"),num(s,"pointOnePercentLowFPS"),str(s,"framesOver20ms"),str(s,"framesOver33ms"),str(s,"catchUpClampedFrames"))
        inputBody.text = "Depth \(str(s,"queueDepth")) / Peak \(str(s,"queueHighWatermark"))\nCoalesced \(str(s,"queueCoalesced"))\nEvicted \(str(s,"queueEvicted"))\nRejected critical \(str(s,"queueRejectedCritical"))\nRejected invalid \(str(s,"queueRejectedInvalid"))"
        weaponBody.text = String(format: "Ammo %@ / %@\nADS %.0f%% • reload %@\nRemaining %.2fs\nObstructed %@\nState %@", str(s,"ammo"),str(s,"reserveAmmo"),num(s,"adsAlpha")*100,bool(s,"reloading") ? "YES":"NO",num(s,"reloadRemaining"),bool(s,"weaponObstructed") ? "YES":"NO",bool(s,"weaponValid") ? "VALID":"FAIL")
        combatBody.text = String(format: "Shots %@ • Active P %@\nSpawned %@\nWorld impact %@ • Target impact %@\nPenetrations %@\nHits %@ • Kills %@ • Target HP %.1f", str(s,"shotsFired"),str(s,"activeProjectiles"),str(s,"projectilesSpawned"),str(s,"worldImpacts"),str(s,"targetImpacts"),str(s,"penetrations"),str(s,"damageHits"),str(s,"damageKills"),num(s,"primaryTargetHealth"))
        worldBody.text = "Obstacles \(str(s,"worldObstacleCount")) • Contacts \(str(s,"collisionContacts"))\nVisibility Full \(str(s,"visibilityFull"))\nReduced \(str(s,"visibilityReduced")) • Minimal \(str(s,"visibilityMinimal"))\nDormant \(str(s,"visibilityDormant"))\nWorld \(bool(s,"worldValid") ? "VALID":"FAIL")"
        integrityBody.text = "Journal \(bool(s,"journalValid") ? "OK":"FAIL")\nCommitted \(str(s,"commandsCommitted")) • Rejected \(str(s,"commandsRejected"))\nRollback \(str(s,"commandsRolledBack")) • Sim \(str(s,"simulationInvariantRollbacks"))\nState \(String(str(s,"stateHash").prefix(12)))…\nJournal \(String(str(s,"journalHead").prefix(12)))…"
        rendererBody.text = String(format: "Rendered %@ • misses %@\nCPU avg %.3fms • max %.3fms\nThermal %@\nPresentation %@ FPS\nGameplay fixed-step remains 60Hz", str(s,"renderedFrames"),str(s,"drawableMisses"),num(s,"renderCpuAverageMs"),num(s,"renderCpuMaxMs"),thermal.name,str(s,"presentationFPS"))
    }
    private func fullReportText() -> String { engine.observatoryReportText() + "Thermal: \(thermalDescription().name)\nCaptured: \(ISO8601DateFormatter().string(from: Date()))\n" }
    private func num(_ d: [String: Any], _ k: String) -> Double { (d[k] as? NSNumber)?.doubleValue ?? 0 }
    private func bool(_ d: [String: Any], _ k: String) -> Bool { (d[k] as? NSNumber)?.boolValue ?? false }
    private func str(_ d: [String: Any], _ k: String) -> String { if let v = d[k] as? String { return v }; if let v = d[k] as? NSNumber { return v.stringValue }; return "—" }
    private func thermalDescription() -> (name: String, level: Int) { switch ProcessInfo.processInfo.thermalState { case .nominal:return("NOMINAL",0);case .fair:return("FAIR",1);case .serious:return("SERIOUS",2);case .critical:return("CRITICAL",3);@unknown default:return("UNKNOWN",1) } }
}
