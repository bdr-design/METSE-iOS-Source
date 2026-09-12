import UIKit

final class METSESettingsViewController: UIViewController {
    private let lookSlider = UISlider()
    private let adsSlider = UISlider()
    private let gyroSwitch = UISwitch()
    private let gyroSlider = UISlider()
    private let gyroSliderRow = UIView()
    private let handednessSwitch = UISwitch()
    private let hudSlider = UISlider()

    private let lookValueLabel = UILabel()
    private let adsValueLabel = UILabel()
    private let gyroValueLabel = UILabel()
    private let hudValueLabel = UILabel()

    override var prefersStatusBarHidden: Bool { true }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .landscape }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "الإعدادات"
        view.backgroundColor = UIColor(red: 0.01, green: 0.018, blue: 0.020, alpha: 1)
        configureUI()
        loadCurrentValues()
    }

    private func configureUI() {
        let scroll = UIScrollView()
        scroll.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(scroll)
        NSLayoutConstraint.activate([
            scroll.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 24),
            scroll.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -24),
            scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 12),
            scroll.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -12)
        ])

        let content = UIStackView()
        content.axis = .vertical
        content.spacing = 22
        content.translatesAutoresizingMaskIntoConstraints = false
        scroll.addSubview(content)
        NSLayoutConstraint.activate([
            content.leadingAnchor.constraint(equalTo: scroll.leadingAnchor),
            content.trailingAnchor.constraint(equalTo: scroll.trailingAnchor),
            content.topAnchor.constraint(equalTo: scroll.topAnchor),
            content.bottomAnchor.constraint(equalTo: scroll.bottomAnchor),
            content.widthAnchor.constraint(equalTo: scroll.widthAnchor)
        ])

        content.addArrangedSubview(sectionCard(title: "الحساسية", rows: [
            sliderRow(label: "حساسية النظر", valueLabel: lookValueLabel, slider: lookSlider,
                      min: Float(METSESettings.lookSensitivityRange.lowerBound),
                      max: Float(METSESettings.lookSensitivityRange.upperBound),
                      action: #selector(lookChanged)),
            sliderRow(label: "حساسية التصويب (ADS)", valueLabel: adsValueLabel, slider: adsSlider,
                      min: Float(METSESettings.adsSensitivityRange.lowerBound),
                      max: Float(METSESettings.adsSensitivityRange.upperBound),
                      action: #selector(adsChanged)),
            switchRow(label: "الجيروسكوب أثناء التصويب", toggle: gyroSwitch, action: #selector(gyroToggled)),
            gyroSliderRowView()
        ]))

        content.addArrangedSubview(sectionCard(title: "التحكم", rows: [
            switchRow(label: "تخطيط لليد اليسرى", toggle: handednessSwitch, action: #selector(handednessChanged))
        ]))

        content.addArrangedSubview(sectionCard(title: "الواجهة", rows: [
            sliderRow(label: "شفافية الواجهة", valueLabel: hudValueLabel, slider: hudSlider, min: 0.4, max: 1.0, action: #selector(hudChanged))
        ]))

        let resetButton = UIButton(type: .system)
        var resetConfig = UIButton.Configuration.plain()
        resetConfig.title = "استعادة الإعدادات الافتراضية"
        resetConfig.image = UIImage(systemName: "arrow.counterclockwise")
        resetConfig.imagePadding = 8
        resetButton.configuration = resetConfig
        resetButton.tintColor = UIColor(red: 1, green: 0.55, blue: 0.35, alpha: 1)
        resetButton.addAction(UIAction { [weak self] _ in self?.resetTapped() }, for: .touchUpInside)
        content.addArrangedSubview(resetButton)
    }

    private func sectionCard(title: String, rows: [UIView]) -> UIView {
        let heading = UILabel()
        heading.text = title
        heading.font = .systemFont(ofSize: 18, weight: .bold)
        heading.textColor = .white

        let card = UIStackView(arrangedSubviews: rows)
        card.axis = .vertical
        card.spacing = 14
        card.isLayoutMarginsRelativeArrangement = true
        card.layoutMargins = UIEdgeInsets(top: 16, left: 16, bottom: 16, right: 16)
        card.backgroundColor = UIColor.white.withAlphaComponent(0.05)
        card.layer.cornerRadius = 16

        let wrapper = UIStackView(arrangedSubviews: [heading, card])
        wrapper.axis = .vertical
        wrapper.spacing = 10
        return wrapper
    }

    private func sliderRow(label text: String, valueLabel: UILabel, slider: UISlider, min: Float, max: Float, action: Selector) -> UIView {
        let label = UILabel()
        label.text = text
        label.font = .systemFont(ofSize: 14, weight: .medium)
        label.textColor = .white

        valueLabel.font = .monospacedSystemFont(ofSize: 13, weight: .semibold)
        valueLabel.textColor = UIColor(red: 0.46, green: 0.82, blue: 0.67, alpha: 1)
        valueLabel.textAlignment = .right
        valueLabel.widthAnchor.constraint(equalToConstant: 52).isActive = true

        let top = UIStackView(arrangedSubviews: [label, valueLabel])
        top.axis = .horizontal
        top.distribution = .fill

        slider.minimumValue = min
        slider.maximumValue = max
        slider.tintColor = UIColor(red: 0.18, green: 0.56, blue: 0.42, alpha: 1)
        slider.addTarget(self, action: action, for: .valueChanged)

        let stack = UIStackView(arrangedSubviews: [top, slider])
        stack.axis = .vertical
        stack.spacing = 6
        return stack
    }

    private func switchRow(label text: String, toggle: UISwitch, action: Selector) -> UIView {
        let label = UILabel()
        label.text = text
        label.font = .systemFont(ofSize: 14, weight: .medium)
        label.textColor = .white
        toggle.onTintColor = UIColor(red: 0.18, green: 0.56, blue: 0.42, alpha: 1)
        toggle.addTarget(self, action: action, for: .valueChanged)
        let stack = UIStackView(arrangedSubviews: [label, UIView(), toggle])
        stack.axis = .horizontal
        return stack
    }

    private func gyroSliderRowView() -> UIView {
        let row = sliderRow(label: "حساسية الجيروسكوب", valueLabel: gyroValueLabel, slider: gyroSlider,
                             min: Float(METSESettings.gyroscopeSensitivityRange.lowerBound),
                             max: Float(METSESettings.gyroscopeSensitivityRange.upperBound),
                             action: #selector(gyroSensitivityChanged))
        gyroSliderRow.translatesAutoresizingMaskIntoConstraints = false
        gyroSliderRow.addSubview(row)
        row.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([
            row.leadingAnchor.constraint(equalTo: gyroSliderRow.leadingAnchor),
            row.trailingAnchor.constraint(equalTo: gyroSliderRow.trailingAnchor),
            row.topAnchor.constraint(equalTo: gyroSliderRow.topAnchor),
            row.bottomAnchor.constraint(equalTo: gyroSliderRow.bottomAnchor)
        ])
        return gyroSliderRow
    }

    private func loadCurrentValues() {
        lookSlider.value = Float(METSESettings.lookSensitivity)
        adsSlider.value = Float(METSESettings.adsSensitivity)
        gyroSwitch.isOn = METSESettings.gyroscopeEnabled
        gyroSlider.value = Float(METSESettings.gyroscopeSensitivity)
        handednessSwitch.isOn = METSESettings.leftHandedLayout
        hudSlider.value = Float(METSESettings.hudOpacity)
        updateAllLabels()
        gyroSliderRow.isHidden = !gyroSwitch.isOn
        gyroSliderRow.alpha = gyroSwitch.isOn ? 1.0 : 0.4
    }

    private func updateAllLabels() {
        lookValueLabel.text = String(format: "%.2f", lookSlider.value)
        adsValueLabel.text = String(format: "%.2f", adsSlider.value)
        gyroValueLabel.text = String(format: "%.2f", gyroSlider.value)
        hudValueLabel.text = String(format: "%.0f%%", hudSlider.value * 100)
    }

    @objc private func lookChanged() {
        METSESettings.lookSensitivity = Double(lookSlider.value)
        updateAllLabels()
    }

    @objc private func adsChanged() {
        METSESettings.adsSensitivity = Double(adsSlider.value)
        updateAllLabels()
    }

    @objc private func gyroToggled() {
        METSESettings.gyroscopeEnabled = gyroSwitch.isOn
        UIView.animate(withDuration: 0.18) {
            self.gyroSliderRow.isHidden = !self.gyroSwitch.isOn
            self.gyroSliderRow.alpha = self.gyroSwitch.isOn ? 1.0 : 0.4
        }
    }

    @objc private func gyroSensitivityChanged() {
        METSESettings.gyroscopeSensitivity = Double(gyroSlider.value)
        updateAllLabels()
    }

    @objc private func handednessChanged() {
        METSESettings.leftHandedLayout = handednessSwitch.isOn
    }

    @objc private func hudChanged() {
        METSESettings.hudOpacity = Double(hudSlider.value)
        updateAllLabels()
    }

    private func resetTapped() {
        METSESettings.resetToDefaults()
        loadCurrentValues()
    }
}
