import UIKit
import MetalKit

final class GameViewController: UIViewController {
    private var engine: METSEEngineBridge?
    private var statusTimer: Timer?
    private let statusLabel = UILabel()
    private let leftPad = UIView()
    private let rightPad = UIView()
    private var leftStart = CGPoint.zero
    private var rightLast = CGPoint.zero

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
        if isMovingFromParent {
            engine?.stop()
        }
    }

    deinit {
        statusTimer?.invalidate()
    }

    private func startStatusTimer() {
        statusTimer?.invalidate()
        statusTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            self?.refreshStatus()
        }
        if let statusTimer {
            RunLoop.main.add(statusTimer, forMode: .common)
        }
    }

    private func refreshStatus() {
        statusLabel.text = engine?.statusString() ?? "ENGINE OFFLINE"
    }

    private func configureHUD() {
        let backButton = UIButton(type: .system)
        backButton.setImage(UIImage(systemName: "chevron.backward"), for: .normal)
        backButton.tintColor = .white
        backButton.backgroundColor = UIColor.black.withAlphaComponent(0.34)
        backButton.layer.cornerRadius = 22
        backButton.accessibilityLabel = "رجوع"
        backButton.addAction(UIAction { [weak self] _ in
            self?.navigationController?.popViewController(animated: true)
        }, for: .touchUpInside)

        statusLabel.text = "ENGINE ONLINE • 60HZ"
        statusLabel.font = .monospacedSystemFont(ofSize: 9, weight: .semibold)
        statusLabel.textColor = UIColor(red: 0.55, green: 0.92, blue: 0.75, alpha: 1)
        statusLabel.backgroundColor = UIColor.black.withAlphaComponent(0.34)
        statusLabel.textAlignment = .center
        statusLabel.layer.cornerRadius = 10
        statusLabel.layer.masksToBounds = true
        statusLabel.adjustsFontSizeToFitWidth = true
        statusLabel.minimumScaleFactor = 0.72

        let fireButton = UIButton(type: .system)
        fireButton.setImage(UIImage(systemName: "scope"), for: .normal)
        fireButton.tintColor = .white
        fireButton.backgroundColor = UIColor(red: 0.72, green: 0.18, blue: 0.10, alpha: 0.78)
        fireButton.layer.cornerRadius = 34
        fireButton.accessibilityLabel = "إطلاق"
        fireButton.addAction(UIAction { [weak self] _ in
            self?.engine?.triggerFire()
        }, for: .touchUpInside)

        [backButton, statusLabel, fireButton].forEach {
            view.addSubview($0)
            $0.translatesAutoresizingMaskIntoConstraints = false
        }

        NSLayoutConstraint.activate([
            backButton.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 12),
            backButton.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 10),
            backButton.widthAnchor.constraint(equalToConstant: 44),
            backButton.heightAnchor.constraint(equalToConstant: 44),

            statusLabel.leadingAnchor.constraint(equalTo: backButton.trailingAnchor, constant: 10),
            statusLabel.centerYAnchor.constraint(equalTo: backButton.centerYAnchor),
            statusLabel.widthAnchor.constraint(greaterThanOrEqualToConstant: 290),
            statusLabel.heightAnchor.constraint(equalToConstant: 30),

            fireButton.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -20),
            fireButton.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -20),
            fireButton.widthAnchor.constraint(equalToConstant: 68),
            fireButton.heightAnchor.constraint(equalToConstant: 68)
        ])
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

        leftPad.addGestureRecognizer(UIPanGestureRecognizer(target: self, action: #selector(movePan(_:))))
        rightPad.addGestureRecognizer(UIPanGestureRecognizer(target: self, action: #selector(lookPan(_:))))
    }

    @objc private func movePan(_ gesture: UIPanGestureRecognizer) {
        let point = gesture.location(in: leftPad)

        if gesture.state == .began {
            leftStart = point
        }

        if gesture.state == .ended || gesture.state == .cancelled || gesture.state == .failed {
            engine?.setMoveForward(0, strafe: 0)
            return
        }

        let dx = Float((point.x - leftStart.x) / 70.0)
        let dy = Float((point.y - leftStart.y) / 70.0)
        let forward = max(-1, min(1, -dy))
        let strafe = max(-1, min(1, dx))
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
