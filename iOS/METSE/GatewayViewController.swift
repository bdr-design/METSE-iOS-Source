import UIKit

final class GatewayViewController: UIViewController {
    private let gradient = CAGradientLayer()
    private let contentStack = UIStackView()

    override func viewDidLoad() {
        super.viewDidLoad()
        navigationItem.backButtonDisplayMode = .minimal
        configureBackground()
        configureGateway()
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        gradient.frame = view.bounds
    }

    private func configureBackground() {
        view.backgroundColor = UIColor(red: 0.008, green: 0.014, blue: 0.016, alpha: 1)
        gradient.colors = [
            UIColor(red: 0.025, green: 0.060, blue: 0.055, alpha: 1).cgColor,
            UIColor(red: 0.008, green: 0.014, blue: 0.016, alpha: 1).cgColor
        ]
        gradient.startPoint = CGPoint(x: 0.15, y: 0.0)
        gradient.endPoint = CGPoint(x: 0.85, y: 1.0)
        view.layer.insertSublayer(gradient, at: 0)
    }

    private func configureGateway() {
        let scroll = UIScrollView()
        scroll.alwaysBounceVertical = true
        scroll.showsVerticalScrollIndicator = false
        view.addSubview(scroll)
        scroll.translatesAutoresizingMaskIntoConstraints = false

        let content = UIView()
        scroll.addSubview(content)
        content.translatesAutoresizingMaskIntoConstraints = false

        contentStack.axis = .vertical
        contentStack.spacing = 18
        content.addSubview(contentStack)
        contentStack.translatesAutoresizingMaskIntoConstraints = false

        NSLayoutConstraint.activate([
            scroll.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),
            scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scroll.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor),
            content.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor),
            content.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor),
            content.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor),
            content.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor),
            content.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor),
            contentStack.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 28),
            contentStack.trailingAnchor.constraint(equalTo: content.trailingAnchor, constant: -28),
            contentStack.topAnchor.constraint(equalTo: content.topAnchor, constant: 24),
            contentStack.bottomAnchor.constraint(equalTo: content.bottomAnchor, constant: -24)
        ])

        contentStack.addArrangedSubview(makeHeader())
        contentStack.addArrangedSubview(makePrimaryAction())
        contentStack.addArrangedSubview(makeStatusRow())
        contentStack.addArrangedSubview(makeModuleGrid())
        contentStack.addArrangedSubview(makeFooter())
    }

    private func makeHeader() -> UIView {
        let eyebrow = UILabel()
        eyebrow.text = "MIDDLE EAST TACTICAL SIMULATION ENGINE"
        eyebrow.font = .monospacedSystemFont(ofSize: 11, weight: .semibold)
        eyebrow.textColor = UIColor(red: 0.48, green: 0.76, blue: 0.64, alpha: 1)

        let title = UILabel()
        title.text = "METSE"
        title.font = .systemFont(ofSize: 42, weight: .black)
        title.textColor = .white

        let subtitle = UILabel()
        subtitle.text = "مركز القيادة"
        subtitle.font = .preferredFont(forTextStyle: .title3)
        subtitle.textColor = UIColor.white.withAlphaComponent(0.62)

        let stack = UIStackView(arrangedSubviews: [eyebrow, title, subtitle])
        stack.axis = .vertical
        stack.spacing = 4
        return stack
    }

    private func makePrimaryAction() -> UIView {
        var config = UIButton.Configuration.filled()
        config.title = "ابدأ جلسة تكتيكية"
        config.subtitle = "دخول مباشر إلى المحرك"
        config.image = UIImage(systemName: "play.fill")
        config.imagePadding = 12
        config.cornerStyle = .large
        config.baseBackgroundColor = UIColor(red: 0.20, green: 0.53, blue: 0.41, alpha: 1)
        config.baseForegroundColor = .white
        config.contentInsets = NSDirectionalEdgeInsets(top: 18, leading: 22, bottom: 18, trailing: 22)

        let button = UIButton(configuration: config)
        button.contentHorizontalAlignment = .leading
        button.titleLabel?.font = .preferredFont(forTextStyle: .headline)
        button.heightAnchor.constraint(greaterThanOrEqualToConstant: 72).isActive = true
        button.addAction(UIAction { [weak self] _ in self?.openGame() }, for: .touchUpInside)
        return button
    }

    private func makeStatusRow() -> UIView {
        let values = ["BUILD 002", "NATIVE METAL", "CONTENT DEV"]
        let row = UIStackView()
        row.axis = .horizontal
        row.spacing = 8
        row.distribution = .fillProportionally
        values.forEach { value in
            let label = UILabel()
            label.text = value
            label.font = .monospacedSystemFont(ofSize: 10, weight: .medium)
            label.textColor = UIColor.white.withAlphaComponent(0.58)
            label.textAlignment = .center
            label.backgroundColor = UIColor.white.withAlphaComponent(0.045)
            label.layer.cornerRadius = 9
            label.layer.masksToBounds = true
            label.heightAnchor.constraint(equalToConstant: 30).isActive = true
            row.addArrangedSubview(label)
        }
        return row
    }

    private func makeModuleGrid() -> UIView {
        let outer = UIStackView()
        outer.axis = .vertical
        outer.spacing = 10

        var index = 0
        let modules = GatewayModule.secondary
        while index < modules.count {
            let row = UIStackView()
            row.axis = .horizontal
            row.spacing = 10
            row.distribution = .fillEqually
            for offset in 0..<2 {
                let i = index + offset
                if i < modules.count {
                    let module = modules[i]
                    let card = GatewayCardButton(module: module)
                    card.addAction(UIAction { [weak self] _ in self?.open(module.destination) }, for: .touchUpInside)
                    row.addArrangedSubview(card)
                } else {
                    row.addArrangedSubview(UIView())
                }
            }
            outer.addArrangedSubview(row)
            index += 2
        }
        return outer
    }

    private func makeFooter() -> UIView {
        let label = UILabel()
        label.text = "Core 0.1 • Content Schema 1 • 32 combatants max"
        label.font = .monospacedSystemFont(ofSize: 10, weight: .regular)
        label.textColor = UIColor.white.withAlphaComponent(0.34)
        label.textAlignment = .center
        return label
    }

    private func openGame() {
        navigationController?.pushViewController(GameViewController(), animated: true)
    }

    private func open(_ destination: GatewayModule.Destination) {
        switch destination {
        case .game: openGame()
        case .updates:
            navigationController?.pushViewController(UpdateCenterViewController(), animated: true)
        default:
            navigationController?.pushViewController(ModulePlaceholderViewController(destination: destination), animated: true)
        }
    }
}
