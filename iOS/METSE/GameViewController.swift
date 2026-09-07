import UIKit
import MetalKit

final class GameViewController: UIViewController {
    private var engine: METSEEngineBridge?
    private let statusLabel = UILabel()

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "جلسة تكتيكية"
        view.backgroundColor = .black

        guard let device = MTLCreateSystemDefaultDevice() else {
            showUnsupportedMetal()
            return
        }

        let metalView = MTKView(frame: .zero, device: device)
        view.addSubview(metalView)
        metalView.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([
            metalView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            metalView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            metalView.topAnchor.constraint(equalTo: view.topAnchor),
            metalView.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])

        engine = METSEEngineBridge(view: metalView)
        engine?.start()
        configureHUD()
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        if isMovingFromParent { engine?.stop() }
    }

    private func configureHUD() {
        statusLabel.text = "ENGINE ONLINE • NATIVE METAL"
        statusLabel.font = .monospacedSystemFont(ofSize: 11, weight: .semibold)
        statusLabel.textColor = UIColor(red: 0.58, green: 0.90, blue: 0.76, alpha: 1)
        statusLabel.backgroundColor = UIColor.black.withAlphaComponent(0.36)
        statusLabel.layer.cornerRadius = 10
        statusLabel.layer.masksToBounds = true
        statusLabel.textAlignment = .center
        view.addSubview(statusLabel)
        statusLabel.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([
            statusLabel.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 16),
            statusLabel.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 12),
            statusLabel.widthAnchor.constraint(greaterThanOrEqualToConstant: 190),
            statusLabel.heightAnchor.constraint(equalToConstant: 32)
        ])
    }

    private func showUnsupportedMetal() {
        let label = UILabel()
        label.text = "هذا الجهاز لا يدعم Metal المطلوب لتشغيل METSE."
        label.textColor = .white
        label.textAlignment = .center
        label.numberOfLines = 0
        view.addSubview(label)
        label.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([
            label.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            label.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 30),
            label.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -30)
        ])
    }
}
