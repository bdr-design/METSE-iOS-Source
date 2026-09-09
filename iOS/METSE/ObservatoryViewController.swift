import UIKit

final class ObservatoryViewController: UIViewController {
    private let engine: METSEEngineBridge
    private var timer: Timer?
    private let healthLabel = UILabel()
    private let runtimeBody = UILabel(), frameBody = UILabel(), inputBody = UILabel(), weaponBody = UILabel(), combatBody = UILabel(), worldBody = UILabel(), integrityBody = UILabel(), rendererBody = UILabel(), issuesBody = UILabel(), scopeBody = UILabel()

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
        let title = UILabel(); title.text = "مركز الرصد V4"; title.font = .systemFont(ofSize: 24, weight: .bold); title.textColor = .white
        let subtitle = UILabel(); subtitle.text = "Runtime / Input / Weapon / Ballistics / Anatomy / Tactical AI / Integrity / Thermal"; subtitle.font = .monospacedSystemFont(ofSize: 9.5, weight: .medium); subtitle.textColor = UIColor.white.withAlphaComponent(0.55)
        let titleStack = UIStackView(arrangedSubviews: [title, subtitle]); titleStack.axis = .vertical; titleStack.spacing = 2
        healthLabel.font = .monospacedSystemFont(ofSize: 10.5, weight: .bold); healthLabel.textAlignment = .center; healthLabel.layer.cornerRadius = 12; healthLabel.layer.masksToBounds = true; healthLabel.widthAnchor.constraint(greaterThanOrEqualToConstant: 210).isActive = true; healthLabel.heightAnchor.constraint(equalToConstant: 32).isActive = true
        let header = UIStackView(arrangedSubviews: [close, titleStack, UIView(), healthLabel]); header.axis = .horizontal; header.alignment = .center; header.spacing = 12

        let rows = [
            row(card("Runtime", "cpu", runtimeBody), card("Frame Performance", "gauge.with.dots.needle.50percent", frameBody)),
            row(card("Input Ownership", "rectangle.stack.badge.play", inputBody), card("Weapon", "scope", weaponBody)),
            row(card("Combat / Anatomy", "bolt.horizontal.circle", combatBody), card("World / Tactical AI", "square.3.layers.3d", worldBody)),
            row(card("Integrity", "checkmark.shield", integrityBody), card("Renderer / Timing", "display", rendererBody)),
            row(card("Problems / Coverage", "exclamationmark.triangle", issuesBody), card("Session Scope", "clock.badge.checkmark", scopeBody))
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
        let validKeys = ["journalValid","worldValid","observatoryValid","queueValid","weaponValid","ballisticsValid","damageValid","visibilityValid","tacticalAIValid","audioFXValid"]
        let problemMask = uint64(s, "diagnosticProblemMask")
        let coverageMask = uint64(s, "acceptanceCoverageMask")
        let valid = validKeys.allSatisfy { bool(s,$0) }
        let hardFault = !valid || problemMask != 0
        let thermalFallback = !hardFault && thermal.level >= 2
        let coveragePending = !hardFault && !thermalFallback && coverageMask != 0
        healthLabel.text = hardFault ? "● ATTENTION • \(thermal.name)" : (thermalFallback ? "● THERMAL FALLBACK • \(thermal.name)" : (coveragePending ? "● COVERAGE PENDING • \(thermal.name)" : "● SYSTEM NOMINAL • \(thermal.name)"))
        healthLabel.textColor = hardFault ? UIColor(red: 1.0, green: 0.45, blue: 0.34, alpha: 1) : (thermalFallback || coveragePending ? UIColor(red: 0.95, green: 0.82, blue: 0.42, alpha: 1) : UIColor(red: 0.53, green: 0.94, blue: 0.71, alpha: 1)); healthLabel.backgroundColor = healthLabel.textColor.withAlphaComponent(0.10)

        runtimeBody.text = String(format: "Build %@ • v%@\nPresentation %@ FPS / Simulation 60Hz\nTick %@ • %.1fs\nBB %@", str(s,"build"),str(s,"version"),str(s,"presentationFPS"),str(s,"simulationTick"),num(s,"simulationSeconds"),str(s,"blackBoxFrames"))
        frameBody.text = String(format: "Window FPS %.1f • AVG %.2fms\nP95 %.2f • P99 %.2f • MAX %.2f\n1%% LOW %.1f • 0.1%% LOW %.1f\nSlice window AVG %.2f • MAX %.2fms\nSlow window >20ms %@ • clamp %@\nSession >20ms %@ • >33ms %@", num(s,"estimatedFPS"),num(s,"averageFrameMs"),num(s,"p95FrameMs"),num(s,"p99FrameMs"),num(s,"maxFrameMs"),num(s,"onePercentLowFPS"),num(s,"pointOnePercentLowFPS"),num(s,"simulationSliceAverageMs"),num(s,"simulationSliceMaxMs"),str(s,"windowSimulationSlicesOver20ms"),str(s,"windowCatchUpClampedFrames"),str(s,"framesOver20ms"),str(s,"framesOver33ms"))
        inputBody.text = "Depth \(str(s,"queueDepth")) / Peak \(str(s,"queueHighWatermark"))\nCoalesced \(str(s,"queueCoalesced"))\nEvicted \(str(s,"queueEvicted"))\nRejected critical \(str(s,"queueRejectedCritical")) • invalid \(str(s,"queueRejectedInvalid"))\nDenials cooldown \(str(s,"denyFireCooldown")) • sprint \(str(s,"denyFireSprintRecovery")) • combat disabled \(str(s,"denyFireCombatDisabled"))"
        weaponBody.text = String(format: "Ammo %@ / %@\nADS %.0f%% • reload %@\nRemaining %.2fs\nObstructed %@\nState %@", str(s,"ammo"),str(s,"reserveAmmo"),num(s,"adsAlpha")*100,bool(s,"reloading") ? "YES":"NO",num(s,"reloadRemaining"),bool(s,"weaponObstructed") ? "YES":"NO",bool(s,"weaponValid") ? "VALID":"FAIL")
        combatBody.text = String(format: "Combatants %@ / 32 • active %@ • wounded %@\nincapacitated %@ • dead %@ • removed %@\nAI shots %@ • player impacts %@\nPlayer %.1f HP • %@ • bleed %.2f/s\nShots %@ • Active P %@ • Spawned %@\nWorld %@ • Target %@ • Pen %@ • Rico %@\nHits %@ • Incap %@ • Kills %@\nTarget %.1f HP • %@ • bleed %.2f/s\nArmor H %.0fJ / T %.0fJ • armorHits %@\nFriendly-fire denials %@", str(s,"combatantCount"),str(s,"combatantActive"),str(s,"combatantWounded"),str(s,"combatantIncapacitated"),str(s,"combatantDead"),str(s,"combatantRemoved"),str(s,"aiShotsFired"),str(s,"aiTargetImpacts"),num(s,"playerHealth"),str(s,"playerCombatState"),num(s,"playerBleedingPerSecond"),str(s,"shotsFired"),str(s,"activeProjectiles"),str(s,"projectilesSpawned"),str(s,"worldImpacts"),str(s,"targetImpacts"),str(s,"penetrations"),str(s,"ricochets"),str(s,"damageHits"),str(s,"damageIncapacitations"),str(s,"damageKills"),num(s,"primaryTargetHealth"),str(s,"primaryTargetCombatState"),num(s,"primaryTargetBleedingPerSecond"),num(s,"primaryTargetHelmetArmorJoules"),num(s,"primaryTargetTorsoArmorJoules"),str(s,"damageArmorHits"),str(s,"friendlyFireDenials"))
        worldBody.text = String(format: "Obstacles %@ • Contacts %@\nProjectile contacts %@ • terminal %@ • target %@\nVisibility F/R/M/D %@/%@/%@/%@\nAI active %@ • LOS %@ • decisions %@\nOrder %@ • threat %.2f\nWorld %@ • AI %@", str(s,"worldObstacleCount"),str(s,"collisionContacts"),str(s,"projectileContacts"),str(s,"projectileTerminalContacts"),str(s,"projectileTargetContacts"),str(s,"visibilityFull"),str(s,"visibilityReduced"),str(s,"visibilityMinimal"),str(s,"visibilityDormant"),str(s,"aiActive"),str(s,"aiLOS"),str(s,"aiDecisions"),str(s,"aiSquadOrder"),num(s,"aiHighestThreat"),bool(s,"worldValid") ? "VALID":"FAIL",bool(s,"tacticalAIValid") ? "VALID":"FAIL")
        worldBody.text = (worldBody.text ?? "") + "\nSuppressed \(str(s,"aiSuppressed")) • checks \(str(s,"aiSuppressionChecks"))/\(str(s,"aiSuppressionCheckCap"))\nExposure observations \(str(s,"aiSuppressionObservations"))\nBudget-truncated segments \(str(s,"aiSuppressionBudgetDrops"))"
        integrityBody.text = "Journal \(bool(s,"journalValid") ? "OK":"FAIL")\nCommitted \(str(s,"commandsCommitted")) • Rejected \(str(s,"commandsRejected"))\nRollback \(str(s,"commandsRolledBack")) • Sim \(str(s,"simulationInvariantRollbacks"))\nBleed transitions \(str(s,"damageBleedTransitions"))\nState \(String(str(s,"stateHash").prefix(12)))…\nJournal \(String(str(s,"journalHead").prefix(12)))…"
        rendererBody.text = String(format: "Rendered %@ • misses %@\nCPU avg %.3fms • max %.3fms\nLock wait avg %.3fms • max %.3fms\nCore critical avg %.3fms • max %.3fms\nCallback avg %.2fms • max %.2fms\n>budget %@ • >50 %@ • >100 %@ • >250 %@\nLifecycle R/B/F/A %@/%@/%@/%@\nMemory warnings %@\nThermal %@ • fallback frames %@ • %@ FPS", str(s,"renderedFrames"),str(s,"drawableMisses"),num(s,"renderCpuAverageMs"),num(s,"renderCpuMaxMs"),num(s,"coreLockWaitAverageMs"),num(s,"coreLockWaitMaxMs"),num(s,"coreCriticalAverageMs"),num(s,"coreCriticalMaxMs"),num(s,"callbackGapAverageMs"),num(s,"callbackGapMaxMs"),str(s,"callbackGapsOverBudget"),str(s,"callbackGapsOver50ms"),str(s,"callbackGapsOver100ms"),str(s,"callbackGapsOver250ms"),str(s,"lifecycleWillResignActive"),str(s,"lifecycleDidEnterBackground"),str(s,"lifecycleWillEnterForeground"),str(s,"lifecycleDidBecomeActive"),str(s,"memoryWarningEvents"),str(s,"thermalState"),str(s,"thermalFallbackFrames"),str(s,"presentationFPS"))
        issuesBody.text = String(format: "Hard problem mask 0x%llx\nCoverage mask 0x%llx\nRejected telemetry %@ • tick regressions %@\nSpike causes callback %@ • catch-up %@ • slice %@\nMemory warnings %@\nFX drops %@ • presentation drops %@\nBits: integrity 1 • telemetry 2 • callback 4 • catch-up 8 • slice 10 • thermal 200 • memory 400", uint64(s,"diagnosticProblemMask"), uint64(s,"acceptanceCoverageMask"), str(s,"telemetryRejectedSamples"), str(s,"simulationTickRegressions"), str(s,"preSpikeCallbackFrames"), str(s,"preSpikeCatchUpFrames"), str(s,"preSpikeSimulationFrames"), str(s,"memoryWarningEvents"), str(s,"fxDropped"), str(s,"audioPresentationDrops"))
        scopeBody.text = String(format: "Observed %.1fs • retained %.1fs\nSamples %@ • retained frames %@\nAI peak %@ / 32 • LOS peak %@\nProjectile peak %@ / 128\nThermal fallback callbacks %@", num(s,"observedRealSeconds"), num(s,"retainedRealSeconds"), str(s,"observedFrames"), str(s,"retainedFrames"), str(s,"aiPeakActive"), str(s,"aiPeakLOS"), str(s,"peakProjectiles"), str(s,"thermalFallbackFrames"))
    }

    private func fullReportText() -> String { engine.observatoryReportText() + "Thermal: \(thermalDescription().name)\nCaptured: \(ISO8601DateFormatter().string(from: Date()))\n" }
    private func num(_ d: [String: Any], _ k: String) -> Double { (d[k] as? NSNumber)?.doubleValue ?? 0 }
    private func bool(_ d: [String: Any], _ k: String) -> Bool { (d[k] as? NSNumber)?.boolValue ?? false }
    private func str(_ d: [String: Any], _ k: String) -> String { if let v = d[k] as? String { return v }; if let v = d[k] as? NSNumber { return v.stringValue }; return "—" }
    private func uint64(_ d: [String: Any], _ k: String) -> UInt64 { (d[k] as? NSNumber)?.uint64Value ?? 0 }
    private func thermalDescription() -> (name: String, level: Int) { switch ProcessInfo.processInfo.thermalState { case .nominal:return("NOMINAL",0);case .fair:return("FAIR",1);case .serious:return("SERIOUS",2);case .critical:return("CRITICAL",3);@unknown default:return("UNKNOWN",1) } }
}
