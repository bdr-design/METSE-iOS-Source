import UIKit
import MetalKit

final class GameViewController: UIViewController {
    private var engine: METSEEngineBridge?
    private var statusTimer: Timer?
    private var diagnosticTimer: Timer?
    private let statusLabel = UILabel()
    private let leftPad = UIView()
    private let rightPad = UIView()
    private let joystickBase = UIView()
    private let joystickKnob = UIView()
    private let stanceButton = UIButton(type: .system)
    private let sprintButton = UIButton(type: .system)
    private let aimButton = UIButton(type: .system)
    private let reloadButton = UIButton(type: .system)
    private var leftStart = CGPoint.zero
    private var rightLast = CGPoint.zero
    private var joystickHomeCenter = CGPoint.zero
    private var joystickActive = false
    private let joystickRadius: CGFloat = 52

    override var prefersStatusBarHidden: Bool { true }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .landscape }
    override var preferredInterfaceOrientationForPresentation: UIInterfaceOrientation { .landscapeRight }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        let safe = view.safeAreaInsets
        joystickHomeCenter = CGPoint(x: safe.left + joystickRadius + 18,
                                     y: view.bounds.height - safe.bottom - joystickRadius - 16)
        guard !joystickActive else { return }
        joystickBase.center = joystickHomeCenter
        joystickKnob.center = joystickHomeCenter
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black
        guard let device = MTLCreateSystemDefaultDevice() else { showUnsupportedMetal(); return }
        let metalView = MTKView(frame: .zero, device: device)
        metalView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(metalView)
        NSLayoutConstraint.activate([
            metalView.leadingAnchor.constraint(equalTo: view.leadingAnchor), metalView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            metalView.topAnchor.constraint(equalTo: view.topAnchor), metalView.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])
        guard let bridge = METSEEngineBridge(view: metalView) else { showUnsupportedMetal(); return }
        engine = bridge
        METSEDiagnosticRecorder.shared.beginGameplay()
        bridge.start()
        configureInput()
        configureHUD()
        refreshStatus()
        diagnosticTimer = Timer.scheduledTimer(withTimeInterval: 15, repeats: true) { [weak self] _ in
            guard UIApplication.shared.applicationState == .active, let engine = self?.engine else { return }
            METSEDiagnosticRecorder.shared.capture(engine.observatoryReportText())
        }
        if let diagnosticTimer { RunLoop.main.add(diagnosticTimer, forMode: .common) }
    }

    override func viewWillAppear(_ animated: Bool) { super.viewWillAppear(animated); navigationController?.setNavigationBarHidden(true, animated: animated); startStatusTimer() }
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        statusTimer?.invalidate(); statusTimer = nil
        engine?.setMoveForward(0, strafe: 0); engine?.setSprintHeld(false); engine?.setAimHeld(false)
        resetJoystick(animated: false)
        if isMovingFromParent {
            diagnosticTimer?.invalidate(); diagnosticTimer = nil
            METSEDiagnosticRecorder.shared.endGameplay()
            engine?.stop()
        }
    }
    deinit { statusTimer?.invalidate(); diagnosticTimer?.invalidate() }

    private func configureInput() {
        [leftPad, rightPad].forEach { $0.backgroundColor = .clear; $0.translatesAutoresizingMaskIntoConstraints = false; view.addSubview($0) }
        NSLayoutConstraint.activate([
            leftPad.leadingAnchor.constraint(equalTo: view.leadingAnchor), leftPad.bottomAnchor.constraint(equalTo: view.bottomAnchor), leftPad.topAnchor.constraint(equalTo: view.centerYAnchor), leftPad.widthAnchor.constraint(equalTo: view.widthAnchor, multiplier: 0.44),
            rightPad.trailingAnchor.constraint(equalTo: view.trailingAnchor), rightPad.topAnchor.constraint(equalTo: view.topAnchor), rightPad.bottomAnchor.constraint(equalTo: view.bottomAnchor), rightPad.widthAnchor.constraint(equalTo: view.widthAnchor, multiplier: 0.50)
        ])
        configureJoystickVisuals()
        leftPad.addGestureRecognizer(UIPanGestureRecognizer(target: self, action: #selector(movePan(_:))))
        rightPad.addGestureRecognizer(UIPanGestureRecognizer(target: self, action: #selector(lookPan(_:))))
    }

    private func configureJoystickVisuals() {
        joystickBase.backgroundColor = UIColor.white.withAlphaComponent(0.08); joystickBase.layer.borderWidth = 1; joystickBase.layer.borderColor = UIColor.white.withAlphaComponent(0.16).cgColor; joystickBase.layer.cornerRadius = joystickRadius; joystickBase.isUserInteractionEnabled = false; joystickBase.alpha = 0.24
        joystickKnob.backgroundColor = UIColor(red: 0.42, green: 0.88, blue: 0.68, alpha: 0.26); joystickKnob.layer.borderWidth = 1; joystickKnob.layer.borderColor = UIColor(red: 0.42, green: 0.88, blue: 0.68, alpha: 0.5).cgColor; joystickKnob.layer.cornerRadius = 24; joystickKnob.isUserInteractionEnabled = false
        joystickBase.frame = CGRect(x: 0, y: 0, width: joystickRadius * 2, height: joystickRadius * 2)
        joystickKnob.frame = CGRect(x: 0, y: 0, width: 48, height: 48)
        view.addSubview(joystickBase); view.addSubview(joystickKnob)
    }

    private func configureHUD() {
        let backButton = makeRoundButton(symbol: "chevron.backward", size: 44, background: UIColor.black.withAlphaComponent(0.38))
        backButton.accessibilityLabel = "رجوع"; backButton.addAction(UIAction { [weak self] _ in self?.navigationController?.popViewController(animated: true) }, for: .touchUpInside)
        let observatoryButton = makeRoundButton(symbol: "waveform.path.ecg", size: 44, background: UIColor(red: 0.08, green: 0.27, blue: 0.22, alpha: 0.9))
        observatoryButton.accessibilityLabel = "مركز الأرصاد"; observatoryButton.addAction(UIAction { [weak self] _ in guard let self, let engine = self.engine else { return }; self.present(ObservatoryViewController(engine: engine), animated: true) }, for: .touchUpInside)
        statusLabel.font = .monospacedSystemFont(ofSize: 13, weight: .bold); statusLabel.textColor = .white; statusLabel.backgroundColor = UIColor.black.withAlphaComponent(0.56); statusLabel.textAlignment = .center; statusLabel.layer.cornerRadius = 12; statusLabel.layer.masksToBounds = true; statusLabel.adjustsFontSizeToFitWidth = true; statusLabel.minimumScaleFactor = 0.78; statusLabel.accessibilityLabel = "حالة القتال"
        let fireButton = makeRoundButton(symbol: "scope", size: 70, background: UIColor(red: 0.72, green: 0.18, blue: 0.10, alpha: 0.82)); fireButton.accessibilityLabel = "إطلاق"; fireButton.addAction(UIAction { [weak self] _ in self?.engine?.triggerFire() }, for: .touchDown)
        configureHoldButton(sprintButton, symbol: "figure.run", label: "ركض سريع") { [weak self] held in self?.engine?.setSprintHeld(held) }
        configureHoldButton(aimButton, symbol: "viewfinder", label: "تصويب") { [weak self] held in self?.engine?.setAimHeld(held) }
        stanceButton.setImage(UIImage(systemName: "figure.stand"), for: .normal); styleAuxiliaryButton(stanceButton); stanceButton.accessibilityLabel = "تغيير الوضعية"; stanceButton.addAction(UIAction { [weak self] _ in self?.engine?.cycleStance(); self?.refreshStatus() }, for: .touchUpInside)
        reloadButton.setImage(UIImage(systemName: "arrow.clockwise"), for: .normal); styleAuxiliaryButton(reloadButton); reloadButton.accessibilityLabel = "تلقيم"; reloadButton.addAction(UIAction { [weak self] _ in self?.engine?.reloadWeapon() }, for: .touchUpInside)
        [backButton, observatoryButton, statusLabel, fireButton, sprintButton, aimButton, stanceButton, reloadButton].forEach { view.addSubview($0); $0.translatesAutoresizingMaskIntoConstraints = false }
        NSLayoutConstraint.activate([
            backButton.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 12), backButton.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 8), backButton.widthAnchor.constraint(equalToConstant: 44), backButton.heightAnchor.constraint(equalToConstant: 44),
            observatoryButton.leadingAnchor.constraint(equalTo: backButton.trailingAnchor, constant: 8), observatoryButton.centerYAnchor.constraint(equalTo: backButton.centerYAnchor), observatoryButton.widthAnchor.constraint(equalToConstant: 44), observatoryButton.heightAnchor.constraint(equalToConstant: 44),
            statusLabel.centerXAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerXAnchor), statusLabel.centerYAnchor.constraint(equalTo: backButton.centerYAnchor), statusLabel.widthAnchor.constraint(equalToConstant: 330), statusLabel.heightAnchor.constraint(equalToConstant: 38),
            statusLabel.leadingAnchor.constraint(greaterThanOrEqualTo: observatoryButton.trailingAnchor, constant: 10), statusLabel.trailingAnchor.constraint(lessThanOrEqualTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -12),
            fireButton.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -16), fireButton.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -16), fireButton.widthAnchor.constraint(equalToConstant: 70), fireButton.heightAnchor.constraint(equalToConstant: 70),
            aimButton.trailingAnchor.constraint(equalTo: fireButton.leadingAnchor, constant: -14), aimButton.bottomAnchor.constraint(equalTo: fireButton.bottomAnchor), aimButton.widthAnchor.constraint(equalToConstant: 58), aimButton.heightAnchor.constraint(equalToConstant: 58),
            sprintButton.trailingAnchor.constraint(equalTo: fireButton.trailingAnchor), sprintButton.bottomAnchor.constraint(equalTo: fireButton.topAnchor, constant: -10), sprintButton.widthAnchor.constraint(equalToConstant: 58), sprintButton.heightAnchor.constraint(equalToConstant: 58),
            reloadButton.trailingAnchor.constraint(equalTo: aimButton.trailingAnchor), reloadButton.bottomAnchor.constraint(equalTo: aimButton.topAnchor, constant: -10), reloadButton.widthAnchor.constraint(equalToConstant: 52), reloadButton.heightAnchor.constraint(equalToConstant: 52),
            stanceButton.trailingAnchor.constraint(equalTo: aimButton.leadingAnchor, constant: -12), stanceButton.bottomAnchor.constraint(equalTo: aimButton.bottomAnchor), stanceButton.widthAnchor.constraint(equalToConstant: 58), stanceButton.heightAnchor.constraint(equalToConstant: 58)
        ])
    }

    private func configureHoldButton(_ button: UIButton, symbol: String, label: String, action: @escaping (Bool) -> Void) {
        button.setImage(UIImage(systemName: symbol), for: .normal); styleAuxiliaryButton(button); button.accessibilityLabel = label
        button.addAction(UIAction { _ in button.backgroundColor = UIColor(red: 0.17, green: 0.54, blue: 0.38, alpha: 0.9); action(true) }, for: .touchDown)
        button.addAction(UIAction { _ in button.backgroundColor = UIColor.black.withAlphaComponent(0.5); action(false) }, for: [.touchUpInside, .touchUpOutside, .touchCancel, .touchDragExit])
    }
    private func styleAuxiliaryButton(_ button: UIButton) { button.tintColor = .white; button.backgroundColor = UIColor.black.withAlphaComponent(0.56); button.layer.cornerRadius = 29; button.layer.borderWidth = 1; button.layer.borderColor = UIColor.white.withAlphaComponent(0.14).cgColor }
    private func makeRoundButton(symbol: String, size: CGFloat, background: UIColor) -> UIButton { let b = UIButton(type: .system); b.setImage(UIImage(systemName: symbol), for: .normal); b.tintColor = .white; b.backgroundColor = background; b.layer.cornerRadius = size / 2; b.layer.borderWidth = 1; b.layer.borderColor = UIColor.white.withAlphaComponent(0.16).cgColor; return b }

    @objc private func movePan(_ gesture: UIPanGestureRecognizer) {
        let local = gesture.location(in: leftPad); let inView = leftPad.convert(local, to: view)
        if gesture.state == .began { joystickActive = true; leftStart = local; joystickBase.center = inView; joystickKnob.center = inView; joystickBase.alpha = 0.58 }
        if gesture.state == .ended || gesture.state == .cancelled || gesture.state == .failed { joystickActive = false; engine?.setMoveForward(0, strafe: 0); resetJoystick(animated: true); return }
        let dx = local.x - leftStart.x, dy = local.y - leftStart.y, length = max(0.0001, sqrt(dx * dx + dy * dy)), scale = min(1.0, joystickRadius / length), cx = dx * scale, cy = dy * scale
        joystickKnob.center = CGPoint(x: joystickBase.center.x + cx, y: joystickBase.center.y + cy)
        engine?.setMoveForward(Float(-cy / joystickRadius), strafe: Float(cx / joystickRadius))
    }
    @objc private func lookPan(_ gesture: UIPanGestureRecognizer) {
        let point = gesture.location(in: rightPad)
        if gesture.state == .began { rightLast = point; return }
        if gesture.state == .ended || gesture.state == .cancelled || gesture.state == .failed { rightLast = point; return }
        let dx = Float(point.x - rightLast.x) * 0.0040, dy = Float(point.y - rightLast.y) * 0.00335; rightLast = point; engine?.addLookYaw(dx, pitch: -dy)
    }
    private func resetJoystick(animated: Bool) { let changes = { self.joystickBase.center = self.joystickHomeCenter; self.joystickKnob.center = self.joystickHomeCenter; self.joystickBase.alpha = 0.24 }; animated ? UIView.animate(withDuration: 0.16, animations: changes) : changes() }
    private func startStatusTimer() { statusTimer?.invalidate(); statusTimer = Timer.scheduledTimer(withTimeInterval: 0.20, repeats: true) { [weak self] _ in self?.refreshStatus() }; if let statusTimer { RunLoop.main.add(statusTimer, forMode: .common) } }
    private func refreshStatus() {
        guard let snapshot = engine?.combatHUDSnapshot() else { statusLabel.text = "المحرك غير متاح"; return }
        let ammo = (snapshot["ammo"] as? NSNumber)?.intValue ?? 0
        let reserve = (snapshot["reserveAmmo"] as? NSNumber)?.intValue ?? 0
        let health = (snapshot["health"] as? NSNumber)?.doubleValue ?? 0
        let stance = snapshot["stance"] as? String ?? "STAND"
        let reloading = (snapshot["reloading"] as? NSNumber)?.boolValue ?? false
        let reloadRemaining = (snapshot["reloadRemaining"] as? NSNumber)?.doubleValue ?? 0
        let obstructed = (snapshot["obstructed"] as? NSNumber)?.boolValue ?? false
        let engaged = (snapshot["engagedAI"] as? NSNumber)?.intValue ?? 0
        let fps = (snapshot["presentationFPS"] as? NSNumber)?.intValue ?? 0
        let stanceAR = stance == "PRONE" ? "منبطح" : (stance == "CROUCH" ? "قرفصاء" : "واقف")
        let state = reloading ? String(format: "تلقيم %.1fث", reloadRemaining) : (obstructed ? "السلاح محجوب" : "جاهز")
        statusLabel.text = "\(ammo)/\(reserve)  •  صحة \(Int(health.rounded()))  •  \(stanceAR)  •  \(state)  •  AI \(engaged)  •  \(fps)FPS"
        statusLabel.textColor = obstructed ? UIColor(red: 1, green: 0.48, blue: 0.32, alpha: 1) : (reloading ? UIColor(red: 1, green: 0.78, blue: 0.28, alpha: 1) : .white)
        stanceButton.setImage(UIImage(systemName: stance == "PRONE" ? "figure.cooldown" : (stance == "CROUCH" ? "figure.strengthtraining.traditional" : "figure.stand")), for: .normal)
    }
    private func showUnsupportedMetal() { let label = UILabel(); label.text = "تعذر تشغيل Metal/المحرك على هذا الجهاز."; label.textColor = .white; label.textAlignment = .center; label.numberOfLines = 0; label.translatesAutoresizingMaskIntoConstraints = false; view.addSubview(label); NSLayoutConstraint.activate([label.centerYAnchor.constraint(equalTo: view.centerYAnchor), label.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 30), label.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -30)]) }
}
