import UIKit
import MetalKit

final class GameViewController: UIViewController {
    private var engine: METSEEngineBridge?
    private var statusTimer: Timer?

    private let statusLabel = UILabel()
    private let leftPad = UIView()
    private let rightPad = UIView()
    private let joystickBase = UIView()
    private let joystickKnob = UIView()
    private var leftStart = CGPoint.zero
    private var rightLast = CGPoint.zero
    private let joystickRadius: CGFloat = 52

    private let stanceButton = UIButton(type: .system)
    private let sprintButton = UIButton(type: .system)

    override var prefersStatusBarHidden: Bool { true }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .landscape }
    override var preferredInterfaceOrientationForPresentation: UIInterfaceOrientation { .landscapeRight }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black

        guard let device = MTLCreateSystemDefaultDevice() else {
            showUnsupportedMetal()
            return
        }

        let metalView = MTKView(frame: .zero, device: device)
        metalView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(metalView)
        NSLayoutConstraint.activate([
            metalView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            metalView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            metalView.topAnchor.constraint(equalTo: view.topAnchor),
            metalView.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])

        engine = METSEEngineBridge(view: metalView)
        engine?.start()
        configureInput()
        configureHUD()
        refreshStatus()
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        navigationController?.setNavigationBarHidden(true, animated: animated)
        startStatusTimer()
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        statusTimer?.invalidate()
        statusTimer = nil
        engine?.setMoveForward(0, strafe: 0)
        engine?.setSprintHeld(false)
        resetJoystick(animated: false)
        if isMovingFromParent {
            engine?.stop()
        }
    }

    deinit {
        statusTimer?.invalidate()
    }

    private func configureHUD() {
        let backButton = makeRoundButton(symbol: "chevron.backward", size: 44)
        backButton.backgroundColor = UIColor.black.withAlphaComponent(0.36)
        backButton.accessibilityLabel = "رجوع"
        backButton.addAction(UIAction { [weak self] _ in
            self?.navigationController?.popViewController(animated: true)
        }, for: .touchUpInside)

        let observatoryButton = makeRoundButton(symbol: "waveform.path.ecg", size: 44)
        observatoryButton.backgroundColor = UIColor(red: 0.08, green: 0.27, blue: 0.22, alpha: 0.86)
        observatoryButton.accessibilityLabel = "مركز الأرصاد"
        observatoryButton.addAction(UIAction { [weak self] _ in
            guard let self, let engine = self.engine else { return }
            self.present(ObservatoryViewController(engine: engine), animated: true)
        }, for: .touchUpInside)

        statusLabel.text = "ENGINE ONLINE • BUILD 007"
        statusLabel.font = .monospacedSystemFont(ofSize: 8.5, weight: .semibold)
        statusLabel.textColor = UIColor(red: 0.55, green: 0.92, blue: 0.75, alpha: 1)
        statusLabel.backgroundColor = UIColor.black.withAlphaComponent(0.34)
        statusLabel.textAlignment = .center
        statusLabel.layer.cornerRadius = 10
        statusLabel.layer.masksToBounds = true
        statusLabel.adjustsFontSizeToFitWidth = true
        statusLabel.minimumScaleFactor = 0.58

        let fireButton = makeRoundButton(symbol: "scope", size: 68)
        fireButton.backgroundColor = UIColor(red: 0.72, green: 0.18, blue: 0.10, alpha: 0.78)
        fireButton.accessibilityLabel = "إطلاق"
        fireButton.addAction(UIAction { [weak self] _ in
            self?.engine?.triggerFire()
        }, for: .touchUpInside)

        configureSprintButton()
        configureStanceButton()

        [backButton, observatoryButton, statusLabel, fireButton, sprintButton, stanceButton].forEach {
            view.addSubview($0)
            $0.translatesAutoresizingMaskIntoConstraints = false
        }

        NSLayoutConstraint.activate([
            backButton.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 12),
            backButton.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 10),
            backButton.widthAnchor.constraint(equalToConstant: 44),
            backButton.heightAnchor.constraint(equalToConstant: 44),

            observatoryButton.leadingAnchor.constraint(equalTo: backButton.trailingAnchor, constant: 8),
            observatoryButton.centerYAnchor.constraint(equalTo: backButton.centerYAnchor),
            observatoryButton.widthAnchor.constraint(equalToConstant: 44),
            observatoryButton.heightAnchor.constraint(equalToConstant: 44),

            statusLabel.leadingAnchor.constraint(equalTo: observatoryButton.trailingAnchor, constant: 8),
            statusLabel.centerYAnchor.constraint(equalTo: backButton.centerYAnchor),
            statusLabel.widthAnchor.constraint(greaterThanOrEqualToConstant: 360),
            statusLabel.heightAnchor.constraint(equalToConstant: 30),

            fireButton.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -20),
            fireButton.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -20),
            fireButton.widthAnchor.constraint(equalToConstant: 68),
            fireButton.heightAnchor.constraint(equalToConstant: 68),

            sprintButton.trailingAnchor.constraint(equalTo: fireButton.trailingAnchor),
            sprintButton.bottomAnchor.constraint(equalTo: fireButton.topAnchor, constant: -12),
            sprintButton.widthAnchor.constraint(equalToConstant: 58),
            sprintButton.heightAnchor.constraint(equalToConstant: 58),

            stanceButton.trailingAnchor.constraint(equalTo: fireButton.leadingAnchor, constant: -14),
            stanceButton.centerYAnchor.constraint(equalTo: fireButton.centerYAnchor),
            stanceButton.widthAnchor.constraint(equalToConstant: 58),
            stanceButton.heightAnchor.constraint(equalToConstant: 58)
        ])
    }

    private func configureSprintButton() {
        sprintButton.setImage(UIImage(systemName: "figure.run"), for: .normal)
        sprintButton.tintColor = .white
        sprintButton.backgroundColor = UIColor.black.withAlphaComponent(0.48)
        sprintButton.layer.cornerRadius = 29
        sprintButton.accessibilityLabel = "ركض سريع"
        sprintButton.addAction(UIAction { [weak self] _ in
            self?.engine?.setSprintHeld(true)
            self?.sprintButton.backgroundColor = UIColor(red: 0.17, green: 0.54, blue: 0.38, alpha: 0.88)
        }, for: .touchDown)
        sprintButton.addAction(UIAction { [weak self] _ in
            self?.engine?.setSprintHeld(false)
            self?.sprintButton.backgroundColor = UIColor.black.withAlphaComponent(0.48)
        }, for: [.touchUpInside, .touchUpOutside, .touchCancel, .touchDragExit])
    }

    private func configureStanceButton() {
        stanceButton.setImage(UIImage(systemName: "figure.stand"), for: .normal)
        stanceButton.tintColor = .white
        stanceButton.backgroundColor = UIColor.black.withAlphaComponent(0.48)
        stanceButton.layer.cornerRadius = 29
        stanceButton.accessibilityLabel = "تغيير الوضعية"
        stanceButton.addAction(UIAction { [weak self] _ in
            self?.engine?.cycleStance()
            self?.refreshStatus()
        }, for: .touchUpInside)
    }

    private func makeRoundButton(symbol: String, size: CGFloat) -> UIButton {
        let button = UIButton(type: .system)
        button.setImage(UIImage(systemName: symbol), for: .normal)
        button.tintColor = .white
        button.layer.cornerRadius = size / 2
        return button
    }

    private func configureInput() {
        leftPad.backgroundColor = .clear
        rightPad.backgroundColor = .clear
        leftPad.translatesAutoresizingMaskIntoConstraints = false
        rightPad.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(leftPad)
        view.addSubview(rightPad)

        NSLayoutConstraint.activate([
            leftPad.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            leftPad.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            leftPad.topAnchor.constraint(equalTo: view.centerYAnchor),
            leftPad.widthAnchor.constraint(equalTo: view.widthAnchor, multiplier: 0.44),

            rightPad.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            rightPad.topAnchor.constraint(equalTo: view.topAnchor),
            rightPad.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            rightPad.widthAnchor.constraint(equalTo: view.widthAnchor, multiplier: 0.50)
        ])

        configureJoystickVisuals()
        leftPad.addGestureRecognizer(UIPanGestureRecognizer(target: self, action: #selector(movePan(_:))))
        rightPad.addGestureRecognizer(UIPanGestureRecognizer(target: self, action: #selector(lookPan(_:))))
    }

    private func configureJoystickVisuals() {
        joystickBase.backgroundColor = UIColor.white.withAlphaComponent(0.08)
        joystickBase.layer.borderWidth = 1
        joystickBase.layer.borderColor = UIColor.white.withAlphaComponent(0.14).cgColor
        joystickBase.layer.cornerRadius = joystickRadius
        joystickBase.isUserInteractionEnabled = false
        joystickBase.alpha = 0.22

        joystickKnob.backgroundColor = UIColor(red: 0.42, green: 0.88, blue: 0.68, alpha: 0.24)
        joystickKnob.layer.borderWidth = 1
        joystickKnob.layer.borderColor = UIColor(red: 0.42, green: 0.88, blue: 0.68, alpha: 0.46).cgColor
        joystickKnob.layer.cornerRadius = 24
        joystickKnob.isUserInteractionEnabled = false

        joystickBase.frame = CGRect(x: 48, y: max(40, view.bounds.height - 142), width: joystickRadius * 2, height: joystickRadius * 2)
        joystickKnob.frame = CGRect(x: 0, y: 0, width: 48, height: 48)
        joystickKnob.center = joystickBase.center
        view.addSubview(joystickBase)
        view.addSubview(joystickKnob)
    }

    @objc private func movePan(_ gesture: UIPanGestureRecognizer) {
        let localPoint = gesture.location(in: leftPad)
        let viewPoint = leftPad.convert(localPoint, to: view)

        if gesture.state == .began {
            leftStart = localPoint
            joystickBase.center = viewPoint
            joystickKnob.center = viewPoint
            joystickBase.alpha = 0.55
            joystickKnob.alpha = 1.0
        }

        if gesture.state == .ended || gesture.state == .cancelled || gesture.state == .failed {
            engine?.setMoveForward(0, strafe: 0)
            resetJoystick(animated: true)
            return
        }

        let rawDX = localPoint.x - leftStart.x
        let rawDY = localPoint.y - leftStart.y
        let length = max(0.0001, sqrt(rawDX * rawDX + rawDY * rawDY))
        let scale = min(1.0, joystickRadius / length)
        let clampedDX = rawDX * scale
        let clampedDY = rawDY * scale
        joystickKnob.center = CGPoint(x: joystickBase.center.x + clampedDX, y: joystickBase.center.y + clampedDY)

        let strafe = Float(clampedDX / joystickRadius)
        let forward = Float(-clampedDY / joystickRadius)
        engine?.setMoveForward(forward, strafe: strafe)
    }

    @objc private func lookPan(_ gesture: UIPanGestureRecognizer) {
        let point = gesture.location(in: rightPad)

        if gesture.state == .began {
            rightLast = point
            return
        }

        if gesture.state == .ended || gesture.state == .cancelled || gesture.state == .failed {
            rightLast = point
            return
        }

        let dx = Float(point.x - rightLast.x) * 0.0042
        let dy = Float(point.y - rightLast.y) * 0.0035
        rightLast = point
        engine?.addLookYaw(dx, pitch: -dy)
    }

    private func resetJoystick(animated: Bool) {
        let changes = {
            self.joystickKnob.center = self.joystickBase.center
            self.joystickBase.alpha = 0.22
            self.joystickKnob.alpha = 0.68
        }
        if animated {
            UIView.animate(withDuration: 0.16, animations: changes)
        } else {
            changes()
        }
    }

    private func startStatusTimer() {
        statusTimer?.invalidate()
        statusTimer = Timer.scheduledTimer(withTimeInterval: 0.8, repeats: true) { [weak self] _ in
            self?.refreshStatus()
        }
        if let statusTimer {
            RunLoop.main.add(statusTimer, forMode: .common)
        }
    }

    private func refreshStatus() {
        statusLabel.text = engine?.statusString() ?? "ENGINE OFFLINE"
        guard let snapshot = engine?.observatorySnapshot(), let stance = snapshot["stance"] as? String else { return }
        let symbol: String
        switch stance {
        case "CROUCH": symbol = "figure.strengthtraining.traditional"
        case "PRONE": symbol = "figure.cooldown"
        default: symbol = "figure.stand"
        }
        stanceButton.setImage(UIImage(systemName: symbol), for: .normal)
    }

    private func showUnsupportedMetal() {
        let label = UILabel()
        label.text = "هذا الجهاز لا يدعم Metal المطلوب لتشغيل METSE."
        label.textColor = .white
        label.textAlignment = .center
        label.numberOfLines = 0
        label.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(label)
        NSLayoutConstraint.activate([
            label.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            label.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 30),
            label.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -30)
        ])
    }
}
