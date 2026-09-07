import UIKit

final class ObservatoryViewController: UIViewController {
    private let engine: METSEEngineBridge
    private var timer: Timer?

    private let healthLabel = UILabel()
    private let runtimeBody = UILabel()
    private let frameBody = UILabel()
    private let characterBody = UILabel()
    private let worldBody = UILabel()
    private let integrityBody = UILabel()
    private let rendererBody = UILabel()

    init(engine: METSEEngineBridge) {
        self.engine = engine
        super.init(nibName: nil, bundle: nil)
        modalPresentationStyle = .overFullScreen
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override var prefersStatusBarHidden: Bool { true }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .landscape }

    override func viewDidLoad() {
        super.viewDidLoad()
        configureUI()
        refresh()
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        startTimer()
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        timer?.invalidate()
        timer = nil
    }

    deinit {
        timer?.invalidate()
    }

    private func configureUI() {
        view.backgroundColor = UIColor(red: 0.008, green: 0.015, blue: 0.016, alpha: 0.97)

        let closeButton = UIButton(type: .system)
        closeButton.setImage(UIImage(systemName: "xmark"), for: .normal)
        closeButton.tintColor = .white
        closeButton.backgroundColor = UIColor.white.withAlphaComponent(0.08)
        closeButton.layer.cornerRadius = 20
        closeButton.accessibilityLabel = "إغلاق مركز الأرصاد"
        closeButton.addAction(UIAction { [weak self] _ in
            self?.dismiss(animated: true)
        }, for: .touchUpInside)

        let titleLabel = UILabel()
        titleLabel.text = "مركز الأرصاد"
        titleLabel.font = .systemFont(ofSize: 24, weight: .bold)
        titleLabel.textColor = .white

        let subtitleLabel = UILabel()
        subtitleLabel.text = "رصد حي للمحرك، الأداء، الحركة، العالم، السلامة والرسم"
        subtitleLabel.font = .systemFont(ofSize: 11, weight: .medium)
        subtitleLabel.textColor = UIColor.white.withAlphaComponent(0.55)

        healthLabel.font = .monospacedSystemFont(ofSize: 11, weight: .bold)
        healthLabel.textAlignment = .center
        healthLabel.layer.cornerRadius = 12
        healthLabel.layer.masksToBounds = true
        healthLabel.translatesAutoresizingMaskIntoConstraints = false
        healthLabel.heightAnchor.constraint(equalToConstant: 32).isActive = true
        healthLabel.widthAnchor.constraint(greaterThanOrEqualToConstant: 190).isActive = true

        let headerText = UIStackView(arrangedSubviews: [titleLabel, subtitleLabel])
        headerText.axis = .vertical
        headerText.spacing = 2

        let header = UIStackView(arrangedSubviews: [closeButton, headerText, UIView(), healthLabel])
        header.axis = .horizontal
        header.alignment = .center
        header.spacing = 12
        closeButton.translatesAutoresizingMaskIntoConstraints = false
        closeButton.widthAnchor.constraint(equalToConstant: 40).isActive = true
        closeButton.heightAnchor.constraint(equalToConstant: 40).isActive = true

        let copyButton = makeActionButton(title: "نسخ تقرير الرصد", symbol: "doc.on.doc")
        copyButton.addAction(UIAction { [weak self] _ in
            guard let self else { return }
            UIPasteboard.general.string = self.fullReportText()
            self.flashCopiedState(on: copyButton)
        }, for: .touchUpInside)

        let shareButton = makeActionButton(title: "مشاركة التقرير", symbol: "square.and.arrow.up")
        shareButton.addAction(UIAction { [weak self, weak shareButton] _ in
            guard let self, let shareButton else { return }
            let controller = UIActivityViewController(activityItems: [self.fullReportText()], applicationActivities: nil)
            controller.popoverPresentationController?.sourceView = shareButton
            self.present(controller, animated: true)
        }, for: .touchUpInside)

        let actions = UIStackView(arrangedSubviews: [copyButton, shareButton])
        actions.axis = .horizontal
        actions.spacing = 10
        actions.distribution = .fillEqually

        let scrollView = UIScrollView()
        scrollView.alwaysBounceVertical = true
        let content = UIStackView()
        content.axis = .vertical
        content.spacing = 12

        let firstRow = UIStackView(arrangedSubviews: [makeCard(title: "Runtime", symbol: "cpu", body: runtimeBody), makeCard(title: "Frame Performance", symbol: "gauge.with.dots.needle.50percent", body: frameBody)])
        firstRow.axis = .horizontal
        firstRow.spacing = 12
        firstRow.distribution = .fillEqually

        let secondRow = UIStackView(arrangedSubviews: [makeCard(title: "Character", symbol: "figure.walk", body: characterBody), makeCard(title: "World & Collision", symbol: "square.3.layers.3d", body: worldBody)])
        secondRow.axis = .horizontal
        secondRow.spacing = 12
        secondRow.distribution = .fillEqually

        let thirdRow = UIStackView(arrangedSubviews: [makeCard(title: "Integrity", symbol: "checkmark.shield", body: integrityBody), makeCard(title: "Renderer", symbol: "display", body: rendererBody)])
        thirdRow.axis = .horizontal
        thirdRow.spacing = 12
        thirdRow.distribution = .fillEqually

        [firstRow, secondRow, thirdRow, actions].forEach { content.addArrangedSubview($0) }
        scrollView.addSubview(content)
        content.translatesAutoresizingMaskIntoConstraints = false

        view.addSubview(header)
        view.addSubview(scrollView)
        header.translatesAutoresizingMaskIntoConstraints = false
        scrollView.translatesAutoresizingMaskIntoConstraints = false

        NSLayoutConstraint.activate([
            header.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 18),
            header.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -18),
            header.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 10),

            scrollView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 18),
            scrollView.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -18),
            scrollView.topAnchor.constraint(equalTo: header.bottomAnchor, constant: 12),
            scrollView.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -10),

            content.leadingAnchor.constraint(equalTo: scrollView.contentLayoutGuide.leadingAnchor),
            content.trailingAnchor.constraint(equalTo: scrollView.contentLayoutGuide.trailingAnchor),
            content.topAnchor.constraint(equalTo: scrollView.contentLayoutGuide.topAnchor),
            content.bottomAnchor.constraint(equalTo: scrollView.contentLayoutGuide.bottomAnchor),
            content.widthAnchor.constraint(equalTo: scrollView.frameLayoutGuide.widthAnchor)
        ])
    }

    private func makeCard(title: String, symbol: String, body: UILabel) -> UIView {
        let container = UIView()
        container.backgroundColor = UIColor.white.withAlphaComponent(0.055)
        container.layer.cornerRadius = 16
        container.layer.borderWidth = 1
        container.layer.borderColor = UIColor.white.withAlphaComponent(0.06).cgColor

        let icon = UIImageView(image: UIImage(systemName: symbol))
        icon.tintColor = UIColor(red: 0.45, green: 0.88, blue: 0.69, alpha: 1)
        icon.contentMode = .scaleAspectFit
        icon.translatesAutoresizingMaskIntoConstraints = false
        icon.widthAnchor.constraint(equalToConstant: 18).isActive = true
        icon.heightAnchor.constraint(equalToConstant: 18).isActive = true

        let titleLabel = UILabel()
        titleLabel.text = title
        titleLabel.font = .systemFont(ofSize: 13, weight: .semibold)
        titleLabel.textColor = UIColor.white.withAlphaComponent(0.82)

        let titleRow = UIStackView(arrangedSubviews: [icon, titleLabel, UIView()])
        titleRow.axis = .horizontal
        titleRow.alignment = .center
        titleRow.spacing = 8

        body.font = .monospacedSystemFont(ofSize: 10.5, weight: .regular)
        body.textColor = UIColor.white.withAlphaComponent(0.72)
        body.numberOfLines = 0
        body.setContentCompressionResistancePriority(.required, for: .vertical)

        let stack = UIStackView(arrangedSubviews: [titleRow, body])
        stack.axis = .vertical
        stack.spacing = 8
        container.addSubview(stack)
        stack.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 14),
            stack.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -14),
            stack.topAnchor.constraint(equalTo: container.topAnchor, constant: 12),
            stack.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -12),
            container.heightAnchor.constraint(greaterThanOrEqualToConstant: 112)
        ])
        return container
    }

    private func makeActionButton(title: String, symbol: String) -> UIButton {
        var configuration = UIButton.Configuration.filled()
        configuration.title = title
        configuration.image = UIImage(systemName: symbol)
        configuration.imagePadding = 8
        configuration.cornerStyle = .medium
        configuration.baseBackgroundColor = UIColor(red: 0.16, green: 0.42, blue: 0.34, alpha: 1)
        configuration.baseForegroundColor = .white
        let button = UIButton(configuration: configuration)
        button.heightAnchor.constraint(equalToConstant: 46).isActive = true
        return button
    }

    private func startTimer() {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 0.75, repeats: true) { [weak self] _ in
            self?.refresh()
        }
        if let timer {
            RunLoop.main.add(timer, forMode: .common)
        }
    }

    private func refresh() {
        let snapshot = engine.observatorySnapshot()

        let journalValid = bool(snapshot, "journalValid")
        let worldValid = bool(snapshot, "worldValid")
        let observatoryValid = bool(snapshot, "observatoryValid")
        let thermal = thermalDescription()
        let healthy = journalValid && worldValid && observatoryValid && thermal.level < 2
        healthLabel.text = healthy ? "● SYSTEM NOMINAL • \(thermal.name)" : "● ATTENTION • \(thermal.name)"
        healthLabel.textColor = healthy ? UIColor(red: 0.53, green: 0.94, blue: 0.71, alpha: 1) : UIColor(red: 1.0, green: 0.73, blue: 0.34, alpha: 1)
        healthLabel.backgroundColor = healthLabel.textColor.withAlphaComponent(0.10)

        runtimeBody.text = String(format:
            "Build 007 • v%@\nTick %@ • %.1fs\nObserved %@ • BB %@\nTelemetry retained %@",
            string(snapshot, "version"),
            string(snapshot, "simulationTick"),
            number(snapshot, "simulationSeconds"),
            string(snapshot, "observedFrames"),
            string(snapshot, "blackBoxFrames"),
            string(snapshot, "retainedTelemetryFrames")
        )

        frameBody.text = String(format:
            "FPS %.1f\nAVG %.2f ms • P95 %.2f ms\nMAX %.2f ms\n>20ms %@ • >33ms %@\nCatch-up clamps %@",
            number(snapshot, "estimatedFPS"),
            number(snapshot, "averageFrameMs"),
            number(snapshot, "p95FrameMs"),
            number(snapshot, "maxFrameMs"),
            string(snapshot, "framesOver20ms"),
            string(snapshot, "framesOver33ms"),
            string(snapshot, "catchUpClampedFrames")
        )

        characterBody.text = String(format:
            "%@ • %@ • %.2f m/s\nXYZ %.2f / %.2f / %.2f\nCamera H %.2f • Roll %.4f\nDistance %.1f m • Peak %.2f\nSprint %.1fs • Air %.1fs",
            string(snapshot, "stance"),
            string(snapshot, "gait"),
            number(snapshot, "speed"),
            number(snapshot, "playerX"),
            number(snapshot, "playerY"),
            number(snapshot, "playerZ"),
            number(snapshot, "cameraHeight"),
            number(snapshot, "cameraRoll"),
            number(snapshot, "distanceTravelled"),
            number(snapshot, "peakSpeed"),
            number(snapshot, "sprintSeconds"),
            number(snapshot, "airborneSeconds")
        )

        worldBody.text = String(format:
            "Obstacles %@\nCollision contacts %@\nWorld %@\nGrounded %@\nStance transitions %@\nGait transitions %@",
            string(snapshot, "worldObstacleCount"),
            string(snapshot, "collisionContacts"),
            worldValid ? "VALID" : "FAIL",
            bool(snapshot, "grounded") ? "YES" : "NO",
            string(snapshot, "stanceTransitions"),
            string(snapshot, "gaitTransitions")
        )

        integrityBody.text = "Journal \(journalValid ? "OK" : "FAIL")\nCommitted \(string(snapshot, "commandsCommitted"))\nRejected \(string(snapshot, "commandsRejected"))\nRolledBack \(string(snapshot, "commandsRolledBack"))\nSim rollback \(string(snapshot, "simulationInvariantRollbacks"))\nHead \(String(string(snapshot, "journalHead").prefix(12)))…"

        rendererBody.text = String(format:
            "Rendered %@\nDrawable misses %@\nCPU AVG %.3f ms\nCPU MAX %.3f ms\nThermal %@",
            string(snapshot, "renderedFrames"),
            string(snapshot, "drawableMisses"),
            number(snapshot, "renderCpuAverageMs"),
            number(snapshot, "renderCpuMaxMs"),
            thermal.name
        )
    }

    private func fullReportText() -> String {
        let thermal = thermalDescription().name
        return engine.observatoryReportText() + "Thermal: \(thermal)\nCaptured: \(ISO8601DateFormatter().string(from: Date()))\n"
    }

    private func flashCopiedState(on button: UIButton) {
        let oldTitle = button.configuration?.title
        var copiedConfiguration = button.configuration
        copiedConfiguration?.title = "تم النسخ"
        button.configuration = copiedConfiguration
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.1) {
            var restoredConfiguration = button.configuration
            restoredConfiguration?.title = oldTitle
            button.configuration = restoredConfiguration
        }
    }

    private func number(_ dictionary: [String: Any], _ key: String) -> Double {
        (dictionary[key] as? NSNumber)?.doubleValue ?? 0
    }

    private func bool(_ dictionary: [String: Any], _ key: String) -> Bool {
        (dictionary[key] as? NSNumber)?.boolValue ?? false
    }

    private func string(_ dictionary: [String: Any], _ key: String) -> String {
        if let value = dictionary[key] as? String { return value }
        if let value = dictionary[key] as? NSNumber { return value.stringValue }
        return "—"
    }

    private func thermalDescription() -> (name: String, level: Int) {
        switch ProcessInfo.processInfo.thermalState {
        case .nominal: return ("NOMINAL", 0)
        case .fair: return ("FAIR", 1)
        case .serious: return ("SERIOUS", 2)
        case .critical: return ("CRITICAL", 3)
        @unknown default: return ("UNKNOWN", 1)
        }
    }
}
