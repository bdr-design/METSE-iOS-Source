import UIKit

final class UpdateCenterViewController: UIViewController {
    private let status = UILabel()

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "مركز التحديث"
        view.backgroundColor = UIColor(red: 0.01, green: 0.018, blue: 0.020, alpha: 1)

        let heading = UILabel()
        heading.text = "Development Content Channel"
        heading.font = .preferredFont(forTextStyle: .title2)
        heading.textColor = .white

        status.font = .monospacedSystemFont(ofSize: 12, weight: .regular)
        status.textColor = UIColor.white.withAlphaComponent(0.62)
        status.numberOfLines = 0

        var config = UIButton.Configuration.filled()
        config.title = "فحص التحديثات"
        config.image = UIImage(systemName: "arrow.clockwise")
        config.imagePadding = 8
        config.cornerStyle = .large
        let check = UIButton(configuration: config)
        check.addAction(UIAction { [weak self] _ in self?.checkConfiguration() }, for: .touchUpInside)
        check.heightAnchor.constraint(greaterThanOrEqualToConstant: 50).isActive = true

        let note = UILabel()
        note.text = "التحديثات هنا للبيانات والمحتوى فقط. تغيير المحرك أو الكود الأصلي يحتاج IPA جديد."
        note.font = .preferredFont(forTextStyle: .footnote)
        note.textColor = UIColor.white.withAlphaComponent(0.42)
        note.numberOfLines = 0

        let stack = UIStackView(arrangedSubviews: [heading, status, check, note])
        stack.axis = .vertical
        stack.spacing = 18
        view.addSubview(stack)
        stack.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 28),
            stack.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -28),
            stack.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 28)
        ])

        refreshStatus()
    }

    private func refreshStatus() {
        do {
            let b = try METSEContentStore.bootstrap()
            status.text = "Version: \(b.contentVersion)\nSchema: \(b.contentSchema)\nSequence: \(b.sequence)\nChannel: \(b.channel)"
        } catch {
            status.text = "CONTENT ERROR: \(error.localizedDescription)"
        }
    }

    private func checkConfiguration() {
        do {
            let b = try METSEContentStore.bootstrap()
            guard let url = URL(string: b.updateManifestURL), !b.updateManifestURL.isEmpty else {
                show("لا يوجد Update Manifest URL مهيأ بعد. المركز يعمل Fail-Closed حتى نربط قناة التطوير.")
                return
            }
            guard url.scheme?.lowercased() == "https",
                  let host = url.host?.lowercased(),
                  b.trustedUpdateHosts.map({ $0.lowercased() }).contains(host) else {
                show("تم رفض عنوان التحديث لأنه ليس HTTPS أو ليس ضمن Trusted Hosts.")
                return
            }
            show("قناة التحديث مهيأة بشكل صالح. تنزيل وتثبيت Content Pack سيُفعّل في المرحلة التالية.")
        } catch {
            show(error.localizedDescription)
        }
    }

    private func show(_ message: String) {
        let alert = UIAlertController(title: "METSE Update Center", message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "حسنًا", style: .default))
        present(alert, animated: true)
    }
}
